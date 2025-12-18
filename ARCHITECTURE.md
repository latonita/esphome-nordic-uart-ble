# UART Nordic (BLE NUS) Architecture

## Overview
A UART-compatible transport over BLE Nordic UART Service. Presents UART-like API (`write_array`, `read_array`, `peek_byte`, `available`, `flush`) plus explicit `connect()` / `disconnect()` and link-state helper `is_connected()`. BLE mechanics are encapsulated behind an internal FSM.

## FSM (current skeleton)
- `IDLE`
- `CONNECTING`
- `DISCOVERING`
- `ENABLING_NOTIF`
- `LINK_ESTABLISHED`
- `DISCONNECTING`
- `ERROR`

Transitions will be driven by BLE events (connect/discover/CCCD/MTU/notify) once implemented. For now, `connect()` sets state to `CONNECTING`, `disconnect()` to `DISCONNECTING`.

## Buffers
- RX: `RingBuffer` (512 bytes) with peek cache.
- TX: `RingBuffer` (512 bytes) to queue outgoing data (BLE fragmentation TBD).

## Public API (C++)
- `connect()` / `disconnect()`
- `is_connected()`
- UART interface: `write_array`, `read_array`, `peek_byte`, `available`, `flush`

## Config (Python)
Validated UUIDs and PIN:
- `service_uuid` (default NUS UUID)
- `rx_uuid`
- `tx_uuid`
- `pin` (0-999999)

## Status
Link management, BLE discovery, notifications, and TX/RX over BLE are not implemented yet. Skeleton is in place for future work.
