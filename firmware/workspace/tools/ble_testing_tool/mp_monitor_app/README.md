# MP Runtime Flask Monitor

Flask UI for `MessageProtocolRuntime` that streams incoming MP packets and exposes controls for:
- Dose Schedule (`PPI_AD_DOSE_SCHEDULE`)
- Unix Time request/push (`PPI_AD_TIME`)
- Dock Status request (`PPI_AD_DOCK_STATUS`)
- Plotting dock temperature, dock battery level, and ring docked status push logs

## Location

- App: `tools/ble_testing_tool/mp_monitor_app/app.py`
- Runtime backend: `tools/ble_testing_tool/tests/message_protocol_runtime.py`

## Requirements

- Python 3.10+
- Built wrapper library: `tools/ble_testing_tool/build/message_protocol_wrapper.so`
    - This can be built by running "make library" in the `tools/ble_testing_tool` folder. 
- `uv` (`https://docs.astral.sh/uv/`)
- Follow README.md in `tools/ble_testing_tool` to connect Dock with Dongle. 

## Run

Terminal A:

```bash
cd tools/ble_testing_tool/mp_monitor_app
uv sync
uv run app.py
```

Terminal B (after Terminal A is running), start bridge/client (must be done in WINDOWS):

```bash
cd tools/ble_testing_tool
uv run python main_tcp_com_bridge.py --port /dev/ttyACM0 --baud 115200
```

Open:

- `http://127.0.0.1:8080`

## Environment Variables

### Runtime (`MessageProtocolRuntime`) vars

- `MP_HIL_HOST` (default: `localhost`)
- `MP_HIL_PORT` (default: `5000`)
- `MP_HIL_IS_MASTER` (`1/0`, `true/false`, default: `1`)
- `MP_HIL_SESSION_ID` (optional uint32; decimal or hex like `0x1234ABCD`; random if unset)
- `MP_HIL_PROCESS_INTERVAL_S` (default: `0.001`)

### Monitor app vars

- `MP_MONITOR_BIND_HOST` (default: `127.0.0.1`)
- `MP_MONITOR_BIND_PORT` (default: `8080`)
- `MP_MONITOR_MAX_MESSAGES` (default: `1000`)
- `MP_MONITOR_MAX_TEMP_POINTS` (default: `2000`)
- `MP_MONITOR_MAX_DOCK_BATT_LEVEL_POINTS` (default: `2000`)
- `MP_MONITOR_MAX_RING_DOCKED_STATUS_POINTS` (default: `2000`)
- `MP_MONITOR_LOG_LEVEL` (default: `INFO`)

## API Endpoints

- `GET /api/state`
- `GET /api/stream` (SSE stream of decoded incoming packets)
- `GET /api/dose_schedule`
- `POST /api/dose_schedule/update`
- `POST /api/dose_schedule/request`
- `GET /api/time`
- `POST /api/time/request` (`PPI_TYPE_RQ`, `PPI_AD_TIME`, empty payload)
- `POST /api/time/push` (`PPI_TYPE_PUSH`, `PPI_AD_TIME`, `unix_time_s` uint32 payload)
- `POST /api/time/update` (legacy alias to `/api/time/push`)
- `GET /api/dock_status`
- `POST /api/dock_status/request`
- `GET /api/temperature_points`
- `GET /api/dock_battery_level_points`
- `GET /api/ring_docked_status_points`

## Obsolete Variables Removed

These older vars are no longer used by `app.py`:
- `MP_HIL_PROCESS_BATCH_SIZE`
- `MP_MONITOR_POLL_INTERVAL_S`
- `MP_MONITOR_QUEUE_SIZE`
- `MP_MONITOR_DRAIN_BATCH_SIZE`

## Troubleshooting

- If bridge exits with TCP connect failure, start monitor/runtime first.
- If monitor shows `create_message_protocol failed: 65534`, verify host/port and that the port is free.
