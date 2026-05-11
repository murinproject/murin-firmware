#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");

const CONFIG_PATH = path.join(__dirname, "config.yaml");

const SOF = 0xaa;
const ESCAPE = 0x1b;
const ESCAPE_XOR = 0x20;

const MSG_HEARTBEAT = 0x00;
const MSG_CMD_MOTOR = 0x01;
const MSG_CMD_SERVO = 0x02;
const MSG_TELEMETRY = 0x03;
const MSG_CMD_CONFIG = 0x10;
const MSG_ACK = 0x7e;
const MSG_NACK = 0x7f;

const CFG_TELEM_ENABLE = 1;
const CFG_TELEM_RATE_MS = 2;
const CFG_TELEM_MASK = 3;
const CFG_TELEM_TIMEOUT_MS = 4;

const TYPE_NAMES = new Map([
  [MSG_HEARTBEAT, "HEARTBEAT"],
  [MSG_CMD_MOTOR, "CMD_MOTOR"],
  [MSG_CMD_SERVO, "CMD_SERVO"],
  [MSG_TELEMETRY, "TELEMETRY"],
  [MSG_CMD_CONFIG, "CMD_CONFIG"],
  [MSG_ACK, "ACK"],
  [MSG_NACK, "NACK"],
]);

const ERR_NAMES = new Map([
  [0x00, "OK"],
  [0x01, "ERR_CRC"],
  [0x02, "ERR_LEN"],
  [0x03, "ERR_TYPE"],
  [0x04, "ERR_CFG"],
  [0x05, "ERR_RANGE"],
]);

const CONFIG_KEY_NAMES = new Map([
  [CFG_TELEM_ENABLE, "TELEM_ENABLE"],
  [CFG_TELEM_RATE_MS, "TELEM_RATE_MS"],
  [CFG_TELEM_MASK, "TELEM_MASK"],
  [CFG_TELEM_TIMEOUT_MS, "TELEM_TIMEOUT_MS"],
]);

function loadConfig() {
  try {
    const text = fs.readFileSync(CONFIG_PATH, "utf8");
    const config = {};
    for (const rawLine of text.split(/\r?\n/)) {
      const line = rawLine.replace(/#.*/, "").trim();
      if (!line) continue;
      const match = line.match(/^([A-Za-z0-9_]+)\s*:\s*(.*)$/);
      if (!match) continue;
      const key = match[1];
      let value = match[2].trim();
      if (
        (value.startsWith('"') && value.endsWith('"')) ||
        (value.startsWith("'") && value.endsWith("'"))
      ) {
        value = value.slice(1, -1);
      } else if (/^-?\d+$/.test(value)) {
        value = Number.parseInt(value, 10);
      }
      config[key] = value;
    }
    return config;
  } catch (err) {
    if (err.code === "ENOENT") return {};
    throw err;
  }
}

function usage() {
  console.log(`usage: parser.js [--port PORT] [--baudrate BAUDRATE] [--raw]
                 [--heartbeat] [--motor LEFT RIGHT] [--servo CHANNEL PULSE_US]
                 [--config KEY VALUE] [--frame TYPE HEX_PAYLOAD]
                 [--json TYPE JSON_TEXT] [--help]

Read and send ESP32 framed-link serial messages.

options:
  --port PORT                 Serial port, e.g. COM11
  --baudrate BAUDRATE         Serial baudrate
  --raw                       Also print raw serial bytes
  --heartbeat                 Send one HEARTBEAT frame after opening the port
  --motor LEFT RIGHT          Send one CMD_MOTOR frame after opening the port
  --servo CHANNEL PULSE_US    Send one CMD_SERVO frame after opening the port
  --config KEY VALUE          Send one CMD_CONFIG frame after opening the port
  --frame TYPE HEX_PAYLOAD    Send generic framed-link message with hex payload
  --json TYPE JSON_TEXT       Send generic framed-link message with UTF-8 JSON payload
  --help                      Show this help message
`);
}

function parseInteger(value, name) {
  const parsed = Number.parseInt(value, 0);
  if (!Number.isFinite(parsed) || Number.isNaN(parsed)) {
    throw new Error(`${name} must be an integer`);
  }
  return parsed;
}

function parseU8(value, name = "value") {
  const parsed = parseInteger(value, name);
  if (parsed < 0 || parsed > 0xff) {
    throw new Error(`${name} must fit in uint8 range 0..255`);
  }
  return parsed;
}

function parseHexPayload(value) {
  const cleaned = value
    .replace(/0x/gi, "")
    .replace(/[\s:,_-]/g, "");
  if (cleaned.length % 2 !== 0) {
    throw new Error("hex payload must contain an even number of digits");
  }
  if (!/^[0-9a-fA-F]*$/.test(cleaned)) {
    throw new Error("hex payload contains non-hex characters");
  }
  return Buffer.from(cleaned, "hex");
}

function parseArgs(argv) {
  const config = loadConfig();
  const args = {
    port: config.port,
    baudrate: config.baudrate || 2000000,
    raw: false,
    sends: [],
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    switch (arg) {
      case "-h":
      case "--help":
        args.help = true;
        break;
      case "--port":
        args.port = argv[++i];
        break;
      case "--baudrate":
        args.baudrate = parseInteger(argv[++i], "baudrate");
        break;
      case "--raw":
        args.raw = true;
        break;
      case "--heartbeat":
        args.sends.push({ type: MSG_HEARTBEAT, payload: Buffer.alloc(0) });
        break;
      case "--motor":
        args.sends.push({
          type: MSG_CMD_MOTOR,
          payload: encodeMotor(parseInteger(argv[++i], "left"), parseInteger(argv[++i], "right")),
        });
        break;
      case "--servo":
        args.sends.push({
          type: MSG_CMD_SERVO,
          payload: encodeServo(parseInteger(argv[++i], "channel"), parseInteger(argv[++i], "pulse_us")),
        });
        break;
      case "--config":
        args.sends.push({
          type: MSG_CMD_CONFIG,
          payload: encodeConfig(parseInteger(argv[++i], "key"), parseInteger(argv[++i], "value")),
        });
        break;
      case "--frame":
        args.sends.push({
          type: parseU8(argv[++i], "type"),
          payload: parseHexPayload(argv[++i]),
        });
        break;
      case "--json": {
        const type = parseU8(argv[++i], "type");
        const jsonText = JSON.stringify(JSON.parse(argv[++i]));
        args.sends.push({ type, payload: Buffer.from(jsonText, "utf8") });
        break;
      }
      default:
        throw new Error(`unknown argument: ${arg}`);
    }
  }

  return args;
}

function crc16(data) {
  let crc = 0xffff;
  for (const byte of data) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) {
      crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
      crc &= 0xffff;
    }
  }
  return crc;
}

function stuff(payload) {
  const out = [];
  for (const byte of payload) {
    if (byte === SOF || byte === ESCAPE) {
      out.push(ESCAPE, byte ^ ESCAPE_XOR);
    } else {
      out.push(byte);
    }
  }
  return Buffer.from(out);
}

function unstuff(payload) {
  const out = [];
  for (let i = 0; i < payload.length; i++) {
    let value = payload[i];
    if (value === ESCAPE && i + 1 < payload.length) {
      i += 1;
      value = payload[i] ^ ESCAPE_XOR;
    }
    out.push(value);
  }
  return Buffer.from(out);
}

function buildFrame(msgType, seq, payload = Buffer.alloc(0)) {
  const stuffed = stuff(payload);
  const header = Buffer.alloc(4);
  header.writeUInt8(msgType, 0);
  header.writeUInt8(seq, 1);
  header.writeUInt16LE(stuffed.length, 2);

  const crc = crc16(Buffer.concat([header, stuffed]));
  const frame = Buffer.alloc(1 + header.length + stuffed.length + 2);
  frame.writeUInt8(SOF, 0);
  header.copy(frame, 1);
  stuffed.copy(frame, 5);
  frame.writeUInt16LE(crc, 5 + stuffed.length);
  return frame;
}

function encodeMotor(leftPwm, rightPwm) {
  const payload = Buffer.alloc(4);
  payload.writeInt16LE(leftPwm, 0);
  payload.writeInt16LE(rightPwm, 2);
  return payload;
}

function encodeServo(channel, pulseUs) {
  const payload = Buffer.alloc(3);
  payload.writeUInt8(channel, 0);
  payload.writeUInt16LE(pulseUs, 1);
  return payload;
}

function encodeConfig(key, value) {
  const payload = Buffer.alloc(5);
  payload.writeUInt8(key, 0);
  payload.writeInt32LE(value, 1);
  return payload;
}

class FrameParser {
  constructor() {
    this.buffer = Buffer.alloc(0);
  }

  feed(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    const frames = [];

    while (true) {
      const start = this.buffer.indexOf(SOF);
      if (start < 0) {
        this.buffer = Buffer.alloc(0);
        break;
      }
      if (start > 0) {
        this.buffer = this.buffer.subarray(start);
      }
      if (this.buffer.length < 7) break;

      const msgType = this.buffer.readUInt8(1);
      const seq = this.buffer.readUInt8(2);
      const length = this.buffer.readUInt16LE(3);
      const total = 1 + 1 + 1 + 2 + length + 2;
      if (this.buffer.length < total) break;

      const rawPayload = this.buffer.subarray(5, 5 + length);
      const rawCrc = this.buffer.readUInt16LE(5 + length);
      const computed = crc16(this.buffer.subarray(1, 5 + length));
      this.buffer = this.buffer.subarray(total);

      if (computed !== rawCrc) {
        console.log(`  [CRC FAIL] expected 0x${hex16(computed)} got 0x${hex16(rawCrc)}`);
        continue;
      }

      frames.push({ msgType, seq, payload: unstuff(rawPayload) });
    }

    return frames;
  }
}

function hex16(value) {
  return value.toString(16).toUpperCase().padStart(4, "0");
}

function hexBytes(data, maxLen = 32) {
  if (!data || data.length === 0) return "-";
  const shown = data.subarray(0, maxLen).toString("hex").match(/.{1,2}/g).join(" ");
  if (data.length > maxLen) {
    return `${shown} ... +${data.length - maxLen} bytes`;
  }
  return shown;
}

function maybeDecodeText(payload) {
  const text = payload.toString("utf8");
  if (Buffer.from(text, "utf8").compare(payload) !== 0) return null;
  const stripped = text.trim();
  if (!stripped) return null;
  if (stripped[0] === "{" || stripped[0] === "[") {
    try {
      return `json=${JSON.stringify(JSON.parse(stripped))}`;
    } catch {
      return `text=${JSON.stringify(stripped)}`;
    }
  }
  if ([...stripped].every((ch) => ch >= " " || /\s/.test(ch))) {
    return `text=${JSON.stringify(stripped)}`;
  }
  return null;
}

function decodeTelemetry(payload) {
  if (payload.length !== 26) {
    return `bad_len=${payload.length} raw=${hexBytes(payload)}`;
  }

  const valid = payload.readUInt8(0);
  const err = payload.readUInt8(1);
  const timestamp = payload.readUInt32LE(2);
  const voltage = payload.readFloatLE(6);
  const current = payload.readFloatLE(10);
  const power = payload.readFloatLE(14);
  const energy = payload.readFloatLE(18);
  const voltageRaw = payload.readUInt16LE(22);
  const currentRaw = payload.readUInt16LE(24);
  const status = valid ? "valid" : "invalid";

  return (
    `${status} err=${err} timestamp=${timestamp}ms ` +
    `voltage=${voltage.toFixed(3)}V current=${current.toFixed(3)}A ` +
    `power=${power.toFixed(3)}W energy=${energy.toFixed(3)}Wh ` +
    `raw_v=${voltageRaw} raw_i=${currentRaw}`
  );
}

function decodeFrame({ msgType, seq, payload }) {
  const name = TYPE_NAMES.get(msgType) || `0x${msgType.toString(16).toUpperCase().padStart(2, "0")}`;

  if (msgType === MSG_TELEMETRY) {
    return `[RX] ${name} seq=${seq} ${decodeTelemetry(payload)}`;
  }
  if (msgType === MSG_ACK) {
    const ackSeq = payload.length >= 1 ? payload.readUInt8(0) : "missing";
    return `[RX] ${name} seq=${seq} ack_seq=${ackSeq} raw=${hexBytes(payload)}`;
  }
  if (msgType === MSG_NACK) {
    const nackSeq = payload.length >= 1 ? payload.readUInt8(0) : "missing";
    const err = payload.length >= 2 ? payload.readUInt8(1) : null;
    const errName = err === null ? "missing" : ERR_NAMES.get(err) || `0x${err.toString(16).toUpperCase().padStart(2, "0")}`;
    return `[RX] ${name} seq=${seq} nack_seq=${nackSeq} err=${errName} raw=${hexBytes(payload)}`;
  }
  if (msgType === MSG_HEARTBEAT) {
    return `[RX] ${name} seq=${seq}`;
  }
  if (msgType === MSG_CMD_MOTOR && payload.length === 4) {
    return `[RX] ${name} seq=${seq} left=${payload.readInt16LE(0)} right=${payload.readInt16LE(2)}`;
  }
  if (msgType === MSG_CMD_SERVO && payload.length === 3) {
    return `[RX] ${name} seq=${seq} channel=${payload.readUInt8(0)} pulse=${payload.readUInt16LE(1)}`;
  }
  if (msgType === MSG_CMD_CONFIG && payload.length === 5) {
    const key = payload.readUInt8(0);
    const value = payload.readInt32LE(1);
    const keyName = CONFIG_KEY_NAMES.get(key) || `0x${key.toString(16).toUpperCase().padStart(2, "0")}`;
    return `[RX] ${name} seq=${seq} key=${keyName}(${key}) value=${value}`;
  }

  const decodedText = maybeDecodeText(payload);
  if (decodedText) {
    return `[RX] ${name} seq=${seq} len=${payload.length} ${decodedText}`;
  }
  return `[RX] ${name} seq=${seq} len=${payload.length} raw=${hexBytes(payload)}`;
}

class SerialMonitor {
  constructor(port, baudrate, raw) {
    this.port = port;
    this.baudrate = baudrate;
    this.raw = raw;
    this.seq = 0;
    this.parser = new FrameParser();
    this.telemetryCount = 0;
    this.telemetryFirstTime = null;
    this.telemetryLastTime = null;
    this.serial = null;
  }

  nextSeq() {
    const seq = this.seq;
    this.seq = (this.seq + 1) & 0xff;
    return seq;
  }

  send(msgType, payload) {
    const seq = this.nextSeq();
    const frame = buildFrame(msgType, seq, payload);
    this.serial.write(frame);
    const name = TYPE_NAMES.get(msgType) || `0x${msgType.toString(16).toUpperCase().padStart(2, "0")}`;
    console.log(`[TX] ${name} seq=${seq} payload=${hexBytes(payload)}`);
  }

  async open() {
    let SerialPort;
    try {
      ({ SerialPort } = require("serialport"));
    } catch {
      throw new Error("Missing npm package 'serialport'. Install it with: npm install serialport");
    }

    this.serial = new SerialPort({
      path: this.port,
      baudRate: this.baudrate,
      autoOpen: false,
    });

    await new Promise((resolve, reject) => {
      this.serial.open((err) => (err ? reject(err) : resolve()));
    });

    this.serial.set({ dtr: true, rts: true }, () => {});
    console.log(`[SERIAL] Connected to ${this.port} @ ${this.baudrate}`);

    this.serial.on("data", (data) => {
      if (this.raw) {
        console.log(`[RAW] ${hexBytes(data, 128)}`);
      }
      for (const frame of this.parser.feed(data)) {
        if (frame.msgType === MSG_TELEMETRY) {
          const now = process.hrtime.bigint();
          if (this.telemetryFirstTime === null) this.telemetryFirstTime = now;
          this.telemetryLastTime = now;
          this.telemetryCount += 1;
        }
        console.log(decodeFrame(frame));
      }
    });
  }

  close() {
    if (this.serial && this.serial.isOpen) {
      this.serial.close();
    }
  }

  telemetryRateReport() {
    if (this.telemetryCount === 0) {
      return "[STATS] Telemetry frames=0 rate=0.00 Hz";
    }
    if (this.telemetryCount === 1 || this.telemetryFirstTime === this.telemetryLastTime) {
      return "[STATS] Telemetry frames=1 rate=n/a";
    }
    const durationSec = Number(this.telemetryLastTime - this.telemetryFirstTime) / 1e9;
    const rate = (this.telemetryCount - 1) / durationSec;
    const periodMs = 1000 / rate;
    return (
      `[STATS] Telemetry frames=${this.telemetryCount} ` +
      `duration=${durationSec.toFixed(2)}s rate=${rate.toFixed(2)} Hz period=${periodMs.toFixed(1)} ms`
    );
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    usage();
    return 0;
  }
  if (!args.port) {
    console.error(`No serial port configured. Pass --port or set it in ${CONFIG_PATH}.`);
    return 2;
  }

  const monitor = new SerialMonitor(args.port, args.baudrate, args.raw);
  await monitor.open();
  for (const item of args.sends) {
    monitor.send(item.type, item.payload);
  }

  console.log("[SERIAL] Listening. Press Ctrl+C to stop.");
  process.on("SIGINT", () => {
    console.log("\n[SERIAL] Stopped");
    monitor.close();
    console.log(monitor.telemetryRateReport());
    process.exit(0);
  });
}

main().then(
  (code) => {
    if (typeof code === "number" && code !== 0) process.exit(code);
  },
  (err) => {
    console.error(`[SERIAL] ${err.message}`);
    process.exit(1);
  }
);
