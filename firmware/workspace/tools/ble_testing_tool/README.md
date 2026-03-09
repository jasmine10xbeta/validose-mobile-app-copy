# BLE Testing Tool

This toolchain lets you run Message Protocol (MP) BLE tests from the Docker/Linux side while bridging MP packets to a physical nRF BLE dongle on Windows over a COM port.

## What Is In This Folder

- `Makefile`: builds the MP wrapper shared library used by Python tests.
- `build/message_protocol_wrapper.so`: generated shared library (created by `make library`).
- `tests/main_example.py`: primary test template (recommended starting point for new tests).
- `tests/main_slave_example.py`: optional slave-side example.
- `main_tcp_com_bridge.py`: TCP <-> COM bridge script (run on Windows).

## Prerequisites

1. nRF BLE dongle flashed with the correct firmware and connected to the BLE peer device.
2. Docker/container environment with this repo mounted.
3. TCP port `5000` exposed from the container to the host -> if you have not rebuilt your container since the port forwarding was added, you might have to do that (but only once).
4. Python 3 on Windows (for `main_tcp_com_bridge.py`).
5. `pyserial` installed where the bridge script runs -> it is highly recommended to us uv package manager. For installation go [here](https://docs.astral.sh/uv/getting-started/installation/).

## 1. Prepare The BLE Dongle

Flash and pair/connect the nRF BLE dongle to the BLE peer device first.

Reference guide (replace with real page):

- [Confluence: BLE Dongle Flash + Pairing Guide](https://10xbeta.atlassian.net/wiki/spaces/VALIDOSE/pages/267616263/nRF+Dongle+USB-+BLE+Tool+User+Guide)

Do not continue until the dongle is connected to the peer device. Once the dongle and peer is connected, close the Putty app to free the COM port. 

## 2. Build The MP Wrapper Library (inside container)

From `ble_testing_tool`:

```bash
cd /workspace/tools/ble_testing_tool
make clean
make library
```

Expected artifact:

- `build/message_protocol_wrapper.so`

## 3. Run A Test (inside container)

Go to the tests folder:

```bash
cd /workspace/tools/ble_testing_tool/tests
```

Recommended (`uv`):

```bash
uv sync
uv run main_example.py
```

Notes:

- `main_example.py` is the baseline template for new tests.
- For new tests, copy/rename it and extend:
  - Example: `cp main_example.py my_new_test.py`
  - Run: `uv run my_new_test.py`
- While running, this test acts as a TCP server and waits for a client on port `5000`.

If you are running the testing dashboard, navigate to mp_monitor_app and run: `uv run app.py`


## 4. Run The TCP <-> COM Bridge (on Windows host, outside container)

The bridge script must run on Windows so it can open the dongle COM port.
It is recommended to use `uv` on Windows for this script as well.

Copy it out of the container (example):

```bash
docker cp <container_name>:/workspace/tools/ble_testing_tool/main_tcp_com_bridge.py .
```

Recommended (`uv`, no manual venv needed):

```powershell
uv run --with pyserial main_tcp_com_bridge.py --port COM7 --baud 115200 --timeout 0.1
```

Show CLI help with `uv`:

```powershell
uv run --with pyserial main_tcp_com_bridge.py --help
```

Alternative (pip):

```powershell
py -m pip install pyserial
```

Bridge behavior:

- Connects to `localhost:5000` (the test process in container, via exposed port).
- Forwards TCP data to COM (`TCP -> COM`).
- Deframes MP packets from COM and forwards them back to TCP (`COM -> TCP`).

## 5. Optional: Slave Example

If needed later, there is an example slave instance:

```bash
cd /workspace/tools/ble_testing_tool/tests
uv run main_slave_example.py
```

This is optional and not required for the basic `main_example.py` flow.

## Common Failure Points

- `Failed to connect to TCP server` in bridge:
  - Test script is not running yet, or container port `5000` is not exposed.
- COM port open errors:
  - Wrong `--port` value, or another program is already using that COM port.
- Python import error for `serial`:
  - Use `uv run --with pyserial ...` or install with `py -m pip install pyserial`.

## Minimal End-To-End Checklist

1. Flash + pair dongle (Confluence guide).
1. `make clean && make library` in `ble_testing_tool`.
1. `uv run main_example.py` in `ble_testing_tool/tests`.
1. Run Windows bridge (recommended: `uv run --with pyserial main_tcp_com_bridge.py --port COMx ...`).
1. Verify bridge logs show bidirectional traffic (`TCP->COM` and `COM->TCP`).
