# ESPHome UART Nordic (BLE NUS) — Stub Component

This repo hosts a work-in-progress UART-like transport over Bluetooth LE Nordic UART Service (NUS). It exposes a UART-compatible interface (`write_array`, `read_array`, `peek_byte`, `available`, `flush`) plus explicit `connect()` / `disconnect()` controls and a simple FSM that will later drive BLE discovery and notification setup.

## YAML (stub)
```yaml
uart_nordic:
  id: ble_uart
  pin: 123456
  service_uuid: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
  rx_uuid: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
  tx_uuid: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E

# Example consumer (any UART-based component)
# some_component:
#   uart_id: ble_uart
```

**Status:** BLE link logic is stubbed; buffers and FSM skeleton are in place. `connect()`/`disconnect()` are placeholders; no real BLE traffic yet.
