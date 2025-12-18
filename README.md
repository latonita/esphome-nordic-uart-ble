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
  mtu: 247
  on_connected:
    - logger.log: "UART Nordic connected"
  on_disconnected:
    - logger.log: "UART Nordic disconnected"

# Example consumer (any UART-based component)
# some_component:
#   uart_id: ble_uart

# Example session polling (pseudo)
# Your component with update_interval: never
# and use on_connected + action to trigger update
interval:
  - interval: 60s
    then:
      - uart_nordic.connect: ble_uart

uart_nordic:
  id: ble_uart
  pin: 123456
  on_connected:
    - lambda: |-
        id(my_component).update();
    # - uart_nordic.disconnect: ble_uart # if component only sends data/receives once in update() then you can disconnect, if its long loop() process - dont call disconnect

# Example actions (shorthand)
button:
  - platform: template
    name: "Nordic UART Connect"
    on_press:
      - uart_nordic.connect: ble_uart
  - platform: template
    name: "Nordic UART Disconnect"
    on_press:
      - uart_nordic.disconnect: ble_uart
```
