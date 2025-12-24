# UART Nordic (BLE NUS) Architecture

## Overview
A UART-compatible transport over BLE Nordic UART Service. Presents UART-like API (`write_array`, `read_array`, `peek_byte`, `available`, `flush`) plus explicit `connect()` / `disconnect()` and link-state helper `is_connected()`. BLE mechanics are encapsulated behind an internal FSM.

## FSM (current skeleton)
- `IDLE`
- `CONNECTING`
- `DISCOVERING`
- `ENABLING_NOTIF`
- `UART_LINK_ESTABLISHED`
- `DISCONNECTING`
- `ERROR`

Transitions will be driven by BLE events (connect/discover/CCCD/MTU/notify) once implemented. For now, `connect()` sets state to `CONNECTING`, `disconnect()` to `DISCONNECTING`.

## Buffers
- RX: `RingBuffer` (512 bytes) with peek cache.
- TX: `RingBuffer` (512 bytes) to queue outgoing data (BLE fragmentation TBD).

## Client (BLE NUS)
- `connect()` / `disconnect()` / `is_connected()`
- UART interface: `write_array`, `read_array`, `peek_byte`, `available`, `flush`
- Options: `connect_on_demand` (auto-connect on UART access), `idle_timeout` (auto-disconnect after inactivity)
- Automations: `on_connected`, `on_disconnected`, `on_sent`, `on_data`
- Actions: `ble_nus_client.connect`, `ble_nus_client.disconnect`, `ble_nus_client.send`
- Internals: RX/TX ring buffers (512 bytes), MTU-driven chunking (MTU-3), TX queue chained via `ESP_GATTC_WRITE_CHAR_EVT`; RX via notifications into ring buffer. Activity timestamp drives idle timeout.

## Server (skeleton)
- `ble_nus_server` exposes the UART interface as a BLE NUS peripheral (ESP32 as server) with UUID/PIN/MTU/idle-timeout/auto-advertise options.
- Automations: `on_connected`, `on_disconnected`, `on_sent`, `on_data`.
- Actions: `ble_nus_server.start_advertising`, `ble_nus_server.stop_advertising`, `ble_nus_server.disconnect`.
- Internals (current state): service/characteristics created via `esp32_ble_server` (RX write, TX notify+CCCD), RX writes pushed to ring buffer, TX notifications sent from buffer; idle timeout calls disconnect. Needs full advertising/security/CCCD handling to be production-ready.

## Config (Python)
Validated UUIDs and PIN:
- `service_uuid` (default NUS UUID)
- `rx_uuid`
- `tx_uuid`
- `pin` (0-999999)
- `mtu` (default 247)
- `on_connected`, `on_disconnected` automations

## Status
Link management, BLE discovery, notifications, and TX/RX over BLE are not implemented yet. Skeleton is in place for future work.
