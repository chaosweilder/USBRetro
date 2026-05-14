// W3C button-name → JP_BUTTON_* bit mask, mirror of src/core/buttons.h.
// Names use the canonical Joypad OS vocabulary (B1-B4, DU/DD/DL/DR, etc.)
// plus a few common aliases (UP/DOWN/LEFT/RIGHT, START, SELECT) the assistant
// is likely to type.

export const BUTTON: Record<string, number> = {
  B1: 1 << 0,
  B2: 1 << 1,
  B3: 1 << 2,
  B4: 1 << 3,
  L1: 1 << 4,
  R1: 1 << 5,
  L2: 1 << 6,
  R2: 1 << 7,
  S1: 1 << 8,
  S2: 1 << 9,
  L3: 1 << 10,
  R3: 1 << 11,
  DU: 1 << 12,
  DD: 1 << 13,
  DL: 1 << 14,
  DR: 1 << 15,
  A1: 1 << 16,
  A2: 1 << 17,
  A3: 1 << 18,
  A4: 1 << 19,
  L4: 1 << 20,
  R4: 1 << 21,
};

const ALIASES: Record<string, string> = {
  UP: "DU",
  DOWN: "DD",
  LEFT: "DL",
  RIGHT: "DR",
  START: "S2",
  SELECT: "S1",
  HOME: "A1",
  GUIDE: "A1",
  A: "B1",
  B: "B2",
  X: "B3",
  Y: "B4",
  LB: "L1",
  RB: "R1",
  LT: "L2",
  RT: "R2",
  LS: "L3",
  RS: "R3",
  CROSS: "B1",
  CIRCLE: "B2",
  SQUARE: "B3",
  TRIANGLE: "B4",
};

export class ButtonParseError extends Error {}

// Accepts: "B1", "B1+DR", "B1, DR", ["B1", "DR"], or a numeric mask.
export function parseButtons(input: string | string[] | number): number {
  if (typeof input === "number") return input >>> 0;
  const tokens = (Array.isArray(input) ? input : input.split(/[+,\s]+/))
    .map((t) => t.trim().toUpperCase())
    .filter(Boolean);
  let mask = 0;
  for (const tok of tokens) {
    const canonical = ALIASES[tok] ?? tok;
    const bit = BUTTON[canonical];
    if (bit === undefined) {
      throw new ButtonParseError(`unknown button "${tok}" — valid: ${Object.keys(BUTTON).join(", ")}`);
    }
    mask |= bit;
  }
  return mask >>> 0;
}

export function maskToNames(mask: number): string[] {
  const names: string[] = [];
  for (const [name, bit] of Object.entries(BUTTON)) {
    if (mask & bit) names.push(name);
  }
  return names;
}

export type AnalogAxis = "LX" | "LY" | "RX" | "RY" | "LT" | "RT";

export const AXIS_INDEX: Record<AnalogAxis, number> = {
  LX: 0,
  LY: 1,
  RX: 2,
  RY: 3,
  LT: 4,
  RT: 5,
};

export function parseAxis(name: string): AnalogAxis {
  const u = name.toUpperCase() as AnalogAxis;
  if (!(u in AXIS_INDEX)) {
    throw new ButtonParseError(`unknown axis "${name}" — valid: LX, LY, RX, RY, LT, RT`);
  }
  return u;
}
