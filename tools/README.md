# ESP32 USB Protocol Utilities

This folder contains serial monitor tools and pytest-based hardware tests for the ESP32 framed-link / ROS2 message protocol. The tools can decode incoming frames, send ROS2 commands, send custom framed-link payloads, and validate ACK/NACK behavior against a running ESP32 firmware image.

## Files

* `parser.py` - interactive serial monitor and framed-link/ROS2 message sender
* `parser.js` - Node.js version of the interactive serial monitor and sender
* `config.yaml` - local serial port configuration

```yaml
port: "COM11"
baudrate: 2000000
```

## Parser Tools

Python:
```bash
python parser.py
```

Node.js:
```bash
node parser.js
```

Override the serial settings from the command line:

```bash
python parser.py --port COM11 --baudrate 2000000
node parser.js --port COM11 --baudrate 2000000
```

Print raw serial bytes as they arrive:

```bash
python parser.py --raw
node parser.js --raw
```

Send ROS2 command frames:

```bash
python parser.py --heartbeat
python parser.py --motor 512 -512
python parser.py --servo 0 1500
python parser.py --config 1 1

node parser.js --heartbeat
node parser.js --motor 512 -512
node parser.js --servo 0 1500
node parser.js --config 1 1
```

You can combine send options. For example, send a heartbeat and disable telemetry:

```bash
python parser.py --heartbeat --config 1 0
node parser.js --heartbeat --config 1 0
```

Send generic framed-link payloads:

```bash
python parser.py --frame 0x30 "01 02 AA 1B"
python parser.py --json 0x30 "{\"cmd\":\"motor\",\"left\":512,\"right\":-512}"

node parser.js --frame 0x30 "01 02 AA 1B"
node parser.js --json 0x30 "{\"cmd\":\"motor\",\"left\":512,\"right\":-512}"
```

Decoded output includes ACK/NACK responses, ROS2 command payloads, telemetry values, and printable custom text/JSON payloads.

## Run Tests

Run all utility tests from the repository root:

```bash
python -m pytest utils -v
```

Run only the protocol smoke tests:

```bash
python -m pytest test_protocol.py -v
```

Run only the performance smoke test and show the latency output:

```bash
python -m pytest test_perf.py -v -s
```

Run one test by name:

```bash
python -m pytest test_protocol.py::test_heartbeat -v -s
```

## Performance Test Settings

`test_perf.py` reads optional keys from `test_config.yaml`:

```yaml
port: "COM11"
baudrate: 2000000
perf_iterations: 50
perf_warmup_iterations: 5
perf_ack_timeout: 0.5
perf_max_avg_ack_ms: 50
perf_max_p95_ack_ms: 200
```

Increase `perf_ack_timeout`, `perf_max_avg_ack_ms`, or `perf_max_p95_ack_ms` if the board is running a debug build or the host is under heavy load.

## Troubleshooting

If tests are skipped, check that `test_config.yaml` has a valid `port`.

If pytest reports that it cannot open the serial port, close any serial monitor, ESP-IDF monitor, Serial Studio, or other program using the same port.

If ACK tests time out, confirm the ESP32 firmware is running the matching protocol implementation and that the configured baudrate matches the firmware UART/USB serial baudrate.

If imports fail, install the Python dependencies again with:

```bash
python -m pip install pytest pyserial pyyaml
```

If tests pass but pytest warns that it cannot write `.pytest_cache`, remove or fix permissions on the repository `.pytest_cache` directory. The warning does not mean the protocol tests failed.
