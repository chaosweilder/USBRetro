// UART framing + CRC8 mirror of src/core/uart/uart_protocol.h
//
// Frame: [SYNC=0xAA][LEN][TYPE][PAYLOAD...][CRC8]
// CRC-8 over LEN+TYPE+PAYLOAD, polynomial 0x07, init 0x00, no reflection.
//
// The RX side has to coexist with ASCII printf logs on the same stream,
// so the parser scans for 0xAA, validates LEN/CRC, and emits anything that
// fails validation back as a log line.

import { EventEmitter } from "node:events";
import { SerialPort } from "serialport";

export const SYNC_BYTE = 0xaa;
export const MAX_PAYLOAD = 64;
export const HEADER_SIZE = 3;
export const CRC_SIZE = 1;
// Default matches pico_enable_stdio_uart's PICO_DEFAULT_UART_BAUD_RATE so the
// same UART can carry printf logs out and INPUT_EVENT packets in. Override
// per-connect if the firmware reconfigures stdio UART to 1 Mbaud.
export const DEFAULT_BAUD = 115_200;

export function crc8(data: Uint8Array, offset = 0, length = data.length - offset): number {
  let crc = 0;
  for (let i = 0; i < length; i++) {
    crc ^= data[offset + i];
    for (let bit = 0; bit < 8; bit++) {
      if (crc & 0x80) crc = ((crc << 1) ^ 0x07) & 0xff;
      else crc = (crc << 1) & 0xff;
    }
  }
  return crc;
}

export function buildFrame(type: number, payload: Uint8Array): Uint8Array {
  if (payload.length > MAX_PAYLOAD) {
    throw new Error(`payload ${payload.length} > MAX_PAYLOAD ${MAX_PAYLOAD}`);
  }
  const frame = new Uint8Array(HEADER_SIZE + payload.length + CRC_SIZE);
  frame[0] = SYNC_BYTE;
  frame[1] = payload.length;
  frame[2] = type & 0xff;
  frame.set(payload, HEADER_SIZE);
  frame[frame.length - 1] = crc8(frame, 1, 2 + payload.length);
  return frame;
}

export interface Packet {
  type: number;
  payload: Uint8Array;
}

// Stateful demuxer. Feed bytes; receive packets and log lines.
export class RxParser extends EventEmitter {
  private buf: number[] = [];
  // When we see 0xAA we tentatively try to parse a frame. If the CRC fails
  // we treat the 0xAA as a literal byte in the log stream and resync from
  // the next byte. This keeps log fidelity high even if a printf happens to
  // contain 0xAA (rare in ASCII, but possible).

  feed(chunk: Buffer | Uint8Array): void {
    for (let i = 0; i < chunk.length; i++) this.buf.push(chunk[i]);
    this.drain();
  }

  private drain(): void {
    while (this.buf.length > 0) {
      const sync = this.buf.indexOf(SYNC_BYTE);
      if (sync < 0) {
        this.flushLog(this.buf.length);
        return;
      }
      if (sync > 0) this.flushLog(sync);
      // buf[0] is now SYNC. Need at least 4 bytes for SYNC+LEN+TYPE+CRC.
      if (this.buf.length < HEADER_SIZE + CRC_SIZE) return;
      const len = this.buf[1];
      if (len > MAX_PAYLOAD) {
        // bad length — treat the SYNC as garbage, resync from next byte
        this.emitLogByte(this.buf.shift()!);
        continue;
      }
      const total = HEADER_SIZE + len + CRC_SIZE;
      if (this.buf.length < total) return; // need more bytes
      const frame = Uint8Array.from(this.buf.slice(0, total));
      const expected = crc8(frame, 1, 2 + len);
      const got = frame[total - 1];
      if (expected !== got) {
        // not a valid packet — emit the SYNC as a log byte, resync
        this.emitLogByte(this.buf.shift()!);
        continue;
      }
      this.buf.splice(0, total);
      const pkt: Packet = { type: frame[2], payload: frame.slice(HEADER_SIZE, HEADER_SIZE + len) };
      this.emit("packet", pkt);
    }
  }

  private logBuf: number[] = [];

  private emitLogByte(b: number): void {
    this.logBuf.push(b);
    this.flushCompletedLines();
  }

  private flushLog(count: number): void {
    for (let i = 0; i < count; i++) this.logBuf.push(this.buf.shift()!);
    this.flushCompletedLines();
  }

  private flushCompletedLines(): void {
    let nl: number;
    while ((nl = this.logBuf.indexOf(0x0a)) >= 0) {
      const line = Buffer.from(this.logBuf.splice(0, nl + 1)).toString("utf8").replace(/\r?\n$/, "");
      this.emit("log", line);
    }
  }

  // Drain any pending log bytes (call on close)
  flush(): void {
    if (this.logBuf.length > 0) {
      const line = Buffer.from(this.logBuf.splice(0)).toString("utf8");
      if (line.length > 0) this.emit("log", line);
    }
  }
}

// Connection wraps a SerialPort and an RxParser. Single instance per process
// for v1 — only one adapter at a time.
export class Connection extends EventEmitter {
  readonly port: SerialPort;
  readonly parser = new RxParser();
  readonly path: string;
  readonly baud: number;
  private logRing: { ts: number; line: string }[] = [];
  private readonly logRingMax = 500;

  constructor(path: string, baud: number = DEFAULT_BAUD) {
    super();
    this.path = path;
    this.baud = baud;
    this.port = new SerialPort({ path, baudRate: baud, autoOpen: false });
    this.port.on("data", (chunk: Buffer) => this.parser.feed(chunk));
    this.port.on("error", (err) => this.emit("error", err));
    this.port.on("close", () => {
      this.parser.flush();
      this.emit("close");
    });
    this.parser.on("packet", (p) => this.emit("packet", p));
    this.parser.on("log", (line: string) => {
      this.logRing.push({ ts: Date.now(), line });
      if (this.logRing.length > this.logRingMax) this.logRing.shift();
      this.emit("log", line);
    });
  }

  open(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.port.open((err) => (err ? reject(err) : resolve()));
    });
  }

  close(): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.port.isOpen) return resolve();
      this.port.close((err) => (err ? reject(err) : resolve()));
    });
  }

  send(type: number, payload: Uint8Array): Promise<void> {
    const frame = buildFrame(type, payload);
    return new Promise((resolve, reject) => {
      this.port.write(Buffer.from(frame), (err) => (err ? reject(err) : resolve()));
    });
  }

  recentLogs(sinceMs: number, grep?: string): string[] {
    const cutoff = Date.now() - sinceMs;
    const re = grep ? new RegExp(grep) : null;
    return this.logRing
      .filter((l) => l.ts >= cutoff && (!re || re.test(l.line)))
      .map((l) => l.line);
  }
}
