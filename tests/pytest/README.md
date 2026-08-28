# ESP32 USB Protocol Utilities

This folder contains serial monitor tools and pytest-based hardware tests for the ESP32 framed-link / ROS2 message protocol. The tools can decode incoming frames, send ROS2 commands, send custom framed-link payloads, and validate ACK/NACK behavior against a running ESP32 firmware image.

## Files

* `conftest.py` - 
* `test_protocol.py` - protocol smoke tests for heartbeat, motor, servo, config, and invalid command handling
* `test_perf.py` - heartbeat ACK latency smoke test
* `test_config.yaml` - local serial port and test threshold configuration

## Prerequisites

Install the Python packages used by `parser.py` and the pytest tests:

```bash
python -m pip install pytest pyserial pyyaml
```

Optional: install the Node.js serial package if you want to use `parser.js`:

```bash
npm install serialport
```

Flash the ESP32 firmware before running the monitor or hardware tests, then connect the board over USB.

## Configure the Serial Port

Edit `utils/test_config.yaml`:

```yaml
port: "COM11"
baudrate: 2000000
```

Use the serial port for your machine:

* Windows: `COM11`, `COM3`, etc.
* Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
* macOS: `/dev/tty.usbserial-*` or `/dev/tty.usbmodem*`

If `port` is missing or empty, pytest will skip the hardware tests and the parser tools require `--port`.

## Parser Tools

Both parsers read framed-link serial data, decode ROS2 messages, and can send one or more command frames after opening the serial port.

Python:

```bash
python utils/parser.py
```

Node.js:

```bash
node utils/parser.js
```

Override the serial settings from the command line:

```bash
python utils/parser.py --port COM11 --baudrate 2000000
node utils/parser.js --port COM11 --baudrate 2000000
```

Print raw serial bytes as they arrive:

```bash
python utils/parser.py --raw
node utils/parser.js --raw
```

Send ROS2 command frames:

```bash
python utils/parser.py --heartbeat
python utils/parser.py --motor 512 -512
python utils/parser.py --servo 0 1500
python utils/parser.py --config 1 1

node utils/parser.js --heartbeat
node utils/parser.js --motor 512 -512
node utils/parser.js --servo 0 1500
node utils/parser.js --config 1 1
```

You can combine send options. For example, send a heartbeat and disable telemetry:

```bash
python utils/parser.py --heartbeat --config 1 0
node utils/parser.js --heartbeat --config 1 0
```

Send generic framed-link payloads:

```bash
python utils/parser.py --frame 0x30 "01 02 AA 1B"
python utils/parser.py --json 0x30 "{\"cmd\":\"motor\",\"left\":512,\"right\":-512}"

node utils/parser.js --frame 0x30 "01 02 AA 1B"
node utils/parser.js --json 0x30 "{\"cmd\":\"motor\",\"left\":512,\"right\":-512}"
```

Decoded output includes ACK/NACK responses, ROS2 command payloads, telemetry values, and printable custom text/JSON payloads.

## Run Tests

Run all utility tests from the repository root:

```bash
python -m pytest utils -v
```

Run only the protocol smoke tests:

```bash
python -m pytest utils/test_protocol.py -v
```

Run only the performance smoke test and show the latency output:

```bash
python -m pytest utils/test_perf.py -v -s
```

Run one test by name:

```bash
python -m pytest utils/test_protocol.py::test_heartbeat -v -s
```

## Performance Test Settings

`test_perf.py` reads optional keys from `utils/test_config.yaml`:

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

## Stress Test

Run the sustained heartbeat stress test (30 seconds by default):

```bash
python -m pytest tests/pytest/test_stress.py -v -s
```

Set `stress_duration_s` in `tests/pytest/test_config.yaml` to change the duration. The test also accepts `stress_ack_timeout` and `stress_ack_retries`; when omitted, those settings fall back to the corresponding performance-test values.

## Troubleshooting

If tests are skipped, check that `utils/test_config.yaml` has a valid `port`.

If pytest reports that it cannot open the serial port, close any serial monitor, ESP-IDF monitor, Serial Studio, or other program using the same port.

If ACK tests time out, confirm the ESP32 firmware is running the matching protocol implementation and that the configured baudrate matches the firmware UART/USB serial baudrate.

If imports fail, install the Python dependencies again with:

```bash
python -m pip install pytest pyserial pyyaml
```

If tests pass but pytest warns that it cannot write `.pytest_cache`, remove or fix permissions on the repository `.pytest_cache` directory. The warning does not mean the protocol tests failed.
