// JoypadLive — C# REST bridge for live controller-remap crowd control.
//
// Parity port of tools/joypad-live/python/server.py — same wire protocol, same
// HTTP contract — so Streamer.bot / curl / anything hitting localhost:8777
// gets identical responses whether you run the Python bridge or this one.
//
//   dotnet run -- /dev/cu.usbmodemXXXX                    # macOS / Linux
//   dotnet run -- COM5                                    # Windows
//   dotnet run -- /dev/cu.usbmodemXXXX --define-demo      # install AswapB + Chaos
//
// Endpoints:
//   GET  /health          { ok, port }
//   GET  /info            INFO
//   GET  /profiles        PROFILE.LIST
//   POST /profile/<n>     PROFILE.SET { index:n }
//   POST /neutral         PROFILE.SET { index:0 }
//   POST /save            PROFILE.SAVE — JSON body forwarded as args
//   POST /apply           PROFILE.APPLY — ephemeral RAM-only remap (no flash write)
//   POST /clear           PROFILE.CLEAR — drop the ephemeral override
//
// Only one process may own the CDC port at a time. Close config.joypad.ai
// (browser Web Serial) before running this.

using System.IO.Ports;
using System.Net;
using System.Text;
using System.Text.Json;

namespace JoypadLive;

public static class Program
{
    // From src/usb/usbd/cdc/cdc_protocol.h
    const byte SYNC    = 0xAA;
    const byte MSG_CMD = 0x01;
    const byte MSG_RSP = 0x02;

    static SerialPort _sp = null!;
    static readonly object _lock = new();

    public static int Main(string[] args)
    {
        string? port = null;
        int baud = 115200;
        int httpPort = 8777;
        bool defineDemo = false;
        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--baud":        baud = int.Parse(args[++i]); break;
                case "--http-port":   httpPort = int.Parse(args[++i]); break;
                case "--define-demo": defineDemo = true; break;
                default:              port ??= args[i]; break;
            }
        }
        if (port == null)
        {
            Console.Error.WriteLine("Usage: dotnet run -- <port> [--baud N] [--http-port N] [--define-demo]");
            Console.Error.WriteLine("  <port>: e.g. /dev/cu.usbmodemXXXX or COM5");
            return 1;
        }

        try
        {
            // DTR/RTS asserted on open. pyserial does this by default on POSIX;
            // System.IO.Ports does NOT, and TinyUSB CDC on RP2040 needs DTR=true
            // before it will respond to commands.
            _sp = new SerialPort(port, baud)
            {
                ReadTimeout = 100,
                DtrEnable = true,
                RtsEnable = true,
            };
            _sp.Open();
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Failed to open {port}: {e.Message}");
            return 1;
        }

        var info = Cmd("INFO");
        Console.WriteLine($"Connected {port}: {ToJson(info)}");
        if (info.TryGetValue("error", out var err))
            Console.Error.WriteLine($"WARNING: {err} — is this the adapter's CDC port, "
                + "and is something else (config.joypad.ai) holding it?");

        return defineDemo ? DefineDemo() : ServeHttp(port, httpPort);
    }

    // -------- demo profiles --------
    // button_map = 18 entries: position = source button, value = target (0=passthrough,
    // 1..24=remap to button N 1-based, 255=disable). See .dev/docs/streamer-live-remap.md §4.
    static readonly (string Name, int[] Map)[] Demos =
    {
        // A<->B swap: pos0(B1)->2(B2), pos1(B2)->1(B1)
        ("AswapB", new[] { 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0, 0, 0 }),
        // D-pad scramble: U->L(15), D->R(16), L->D(14), R->U(13)
        ("Chaos",  new[] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 16, 14, 13, 0, 0 }),
    };

    static int DefineDemo()
    {
        Console.WriteLine("Defining demo profiles...");
        foreach (var (name, map) in Demos)
        {
            var r = Cmd("PROFILE.SAVE", new() { ["index"] = 255, ["name"] = name, ["button_map"] = map });
            Console.WriteLine($"  PROFILE.SAVE {name,-8} -> {ToJson(r)}");
        }
        Console.WriteLine("Current profiles:");
        Console.WriteLine("  " + ToJson(Cmd("PROFILE.LIST")));
        return 0;
    }

    // -------- HTTP server --------
    static int ServeHttp(string port, int httpPort)
    {
        var http = new HttpListener();
        http.Prefixes.Add($"http://127.0.0.1:{httpPort}/");
        http.Start();
        Console.WriteLine($"REST API on http://127.0.0.1:{httpPort}/  "
            + "(GET /profiles | POST /profile/<n> | POST /neutral)");
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; http.Stop(); };

        while (http.IsListening)
        {
            HttpListenerContext ctx;
            try { ctx = http.GetContext(); } catch { break; }
            HandleRequest(ctx, port);
        }
        return 0;
    }

    static void HandleRequest(HttpListenerContext ctx, string port)
    {
        string path = ctx.Request.Url!.AbsolutePath.Trim('/');
        string method = ctx.Request.HttpMethod;
        Dictionary<string, object?> resp;
        int code = 200;
        try
        {
            if (method == "GET" && (path == "" || path == "health"))
                resp = new() { ["ok"] = true, ["port"] = port };
            else if (method == "GET" && path == "info")
                resp = Cmd("INFO");
            else if (method == "GET" && path == "profiles")
                resp = Cmd("PROFILE.LIST");
            // Hot-path selection uses PROFILE.SELECT — RAM only, no flash
            // write. The persistent boot default survives via /default/<n>.
            else if (method == "POST" && path.StartsWith("profile/")
                     && int.TryParse(path[8..], out int n))
                resp = Cmd("PROFILE.SELECT", new() { ["index"] = n });
            else if (method == "POST" && path == "neutral")
                resp = Cmd("PROFILE.SELECT", new() { ["index"] = 0 });
            else if (method == "POST" && path.StartsWith("default/")
                     && int.TryParse(path[8..], out int dn))
                // Persistent — writes to flash. For "make this the new boot
                // default," not for hot-path crowd-control switching.
                resp = Cmd("PROFILE.SET", new() { ["index"] = dn });
            else if (method == "POST" && path == "save")
            {
                using var reader = new StreamReader(ctx.Request.InputStream);
                var body = JsonSerializer.Deserialize<Dictionary<string, object?>>(reader.ReadToEnd()) ?? new();
                resp = Cmd("PROFILE.SAVE", body);
            }
            else if (method == "POST" && path == "apply")
            {
                // PROFILE.APPLY — ephemeral RAM-only remap. Body forwarded as args.
                using var reader = new StreamReader(ctx.Request.InputStream);
                var body = JsonSerializer.Deserialize<Dictionary<string, object?>>(reader.ReadToEnd()) ?? new();
                resp = Cmd("PROFILE.APPLY", body);
            }
            else if (method == "POST" && path == "clear")
            {
                resp = Cmd("PROFILE.CLEAR");
            }
            else if (method == "POST" && path == "overlay")
            {
                // OVERLAY.SET — runtime "live tweak" layer composed on top of
                // whatever profile is active. Body forwarded as args.
                using var reader = new StreamReader(ctx.Request.InputStream);
                var body = JsonSerializer.Deserialize<Dictionary<string, object?>>(reader.ReadToEnd()) ?? new();
                resp = Cmd("OVERLAY.SET", body);
            }
            else if (method == "POST" && path == "overlay/clear")
            {
                resp = Cmd("OVERLAY.CLEAR");
            }
            else
            {
                code = 404;
                resp = new() { ["ok"] = false, ["error"] = "not found" };
            }
        }
        catch (Exception ex)
        {
            code = 500;
            resp = new() { ["ok"] = false, ["error"] = ex.Message };
        }
        byte[] bytes = JsonSerializer.SerializeToUtf8Bytes(resp);
        ctx.Response.StatusCode = code;
        ctx.Response.ContentType = "application/json";
        ctx.Response.AppendHeader("Access-Control-Allow-Origin", "*");
        ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
        ctx.Response.Close();
        Console.WriteLine($"{method} /{path} -> {Encoding.UTF8.GetString(bytes)}");
    }

    // -------- CDC framing --------
    // Frame: [SYNC=0xAA][LEN:2 LE][TYPE][SEQ][PAYLOAD][CRC16:2 LE]
    // CRC-16-CCITT (poly 0x1021, init 0xFFFF) over TYPE+SEQ+PAYLOAD.
    static ushort Crc16(ReadOnlySpan<byte> data)
    {
        ushort crc = 0xFFFF;
        foreach (byte b in data)
        {
            crc ^= (ushort)(b << 8);
            for (int i = 0; i < 8; i++)
                crc = (crc & 0x8000) != 0 ? (ushort)((crc << 1) ^ 0x1021)
                                          : (ushort)(crc << 1);
        }
        return crc;
    }

    static Dictionary<string, object?> Cmd(string cmd, Dictionary<string, object?>? args = null)
    {
        var payload = new Dictionary<string, object?> { ["cmd"] = cmd };
        if (args != null)
            foreach (var kv in args) payload[kv.Key] = kv.Value;
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(payload);

        byte type = MSG_CMD, seq = 1;
        byte[] crcIn = new byte[2 + json.Length];
        crcIn[0] = type; crcIn[1] = seq;
        Buffer.BlockCopy(json, 0, crcIn, 2, json.Length);
        ushort crc = Crc16(crcIn);

        byte[] frame = new byte[5 + json.Length + 2];
        int i = 0;
        frame[i++] = SYNC;
        frame[i++] = (byte)(json.Length & 0xFF);
        frame[i++] = (byte)((json.Length >> 8) & 0xFF);
        frame[i++] = type;
        frame[i++] = seq;
        Buffer.BlockCopy(json, 0, frame, i, json.Length); i += json.Length;
        frame[i++] = (byte)(crc & 0xFF);
        frame[i++] = (byte)((crc >> 8) & 0xFF);

        lock (_lock)
        {
            _sp.DiscardInBuffer();
            _sp.Write(frame, 0, i);

            byte[] buf = new byte[1024];
            int got = 0;
            long deadline = Environment.TickCount64 + 800;
            while (Environment.TickCount64 < deadline && got < buf.Length)
            {
                try { got += _sp.Read(buf, got, buf.Length - got); }
                catch (TimeoutException) { }

                for (int q = 0; q + 5 <= got; q++)
                {
                    if (buf[q] != SYNC) continue;
                    int len = buf[q + 1] | (buf[q + 2] << 8);
                    if (q + 5 + len + 2 > got) break;
                    if (buf[q + 3] == MSG_RSP)
                    {
                        return JsonSerializer.Deserialize<Dictionary<string, object?>>(
                            buf.AsSpan(q + 5, len)) ?? new() { ["ok"] = false };
                    }
                }
            }
            return new() { ["ok"] = false, ["error"] = "timeout" };
        }
    }

    static string ToJson(object o) => JsonSerializer.Serialize(o);
}
