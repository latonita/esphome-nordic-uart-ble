[Nordic UART BLE](https://github.com/latonita/esphome-nordic-uart-ble) • [СПОДЭС/DLMS/COSEM](https://github.com/latonita/esphome-dlms-cosem) •
[МЭК-61107/IEC-61107](https://github.com/latonita/esphome-iec61107-meter) • [Энергомера МЭК/IEC](https://github.com/latonita/esphome-energomera-iec) •
[Энергомера CE](https://github.com/latonita/esphome-energomera-ce) • [СПб ЗИП ЦЭ2727А](https://github.com/latonita/esphome-ce2727a-meter) •
[Ленэлектро ЛЕ-2](https://github.com/latonita/esphome-le2-meter) • [Пульсар-М](https://github.com/latonita/esphome-pulsar-m) •
[Энергомера BLE](https://github.com/latonita/esphome-energomera-ble)

# ESPHome UART Nordic (BLE NUS) Client

A drop-in UART-like transport over Bluetooth LE Nordic UART Service (NUS). It implements the usual UART surface (`write_array`, `read_array`, `peek_byte`, `available`, `flush`) plus explicit `connect()` / `disconnect()`, optional auto-connect, and an optional idle auto-disconnect. You can point existing UART-based components at this transport to replace a wired UART with BLE NUS without code changes in the consumer.

## What is NUS (Nordic UART Service)?
Nordic UART Service emulates a serial link over BLE using two characteristics: RX (writes from central to peripheral) and TX (notifications from peripheral to central). The central subscribes to TX notifications, and once that’s acknowledged it writes outbound data to RX. Each packet is limited by the GATT payload (often 20 bytes unless MTU negotiation raises it); the higher-level framing/interpretation is entirely application-specific.


### Migrating from wired UART to Nordic UART
If you already have a component using a wired UART, you can swap it to BLE NUS by pointing it to the Nordic transport:

```
# Before (wired UART)
uart:
  id: uart_bus
  tx_pin: 1
  rx_pin: 3

some_component:
  uart_id: uart_bus

# After (BLE NUS)
uart_nordic:
  id: ble_uart
  pin: 123456
  # optional: idle_timeout: 5min
  # optional: autoconnect: true

some_component:
  uart_id: ble_uart
```

## Nordic UART Server (coming soon)
This repository also contains a skeleton `uart_nordic_server` component intended to expose NUS as a BLE peripheral (ESP32 acts as the NUS server). It mirrors the client-facing options (UUIDs, PIN, MTU, idle timeout, auto-advertise) and will offer the same UART-like API plus advertising controls and connect/disconnect triggers. The current implementation is a stub ready to be wired into `esp32_ble` server APIs.

## YAML (stub)
```yaml
uart_nordic:
  id: ble_uart
  pin: 123456
  service_uuid: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
  rx_uuid: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
  tx_uuid: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
  mtu: 247
  idle_timeout: 0s      # optional, default disables auto-disconnect
  autoconnect: false    # optional, auto-connect on UART access when disconnected

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

```


## Configuration variables
- **id** (Required): Component ID.
- **pin** (Required, int): 6-digit BLE pairing PIN.
- **service_uuid** (Optional, string): NUS service UUID. Default `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`.
- **rx_uuid** (Optional, string): NUS RX characteristic (writes from client). Default `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`.
- **tx_uuid** (Optional, string): NUS TX characteristic (notifications to client). Default `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`.
- **mtu** (Optional, int): Desired MTU, 23–517. Default `247`.
- **idle_timeout** (Optional, time): Auto-disconnect after no RX/TX activity. `0s` disables (default).
- **autoconnect** (Optional, bool): If `true`, any UART access while disconnected will trigger a BLE connect attempt (once per second max). Default `false`.
- **on_connected** (Optional): Automation when BLE link is established.
- **on_disconnected** (Optional): Automation when BLE link drops.
- All other options from `ble_client`.

## Triggers
- `on_connected`: Fired when the BLE link reaches UART_LINK_ESTABLISHED.
- `on_disconnected`: Fired when the BLE link closes.

## Actions
- `uart_nordic.connect`: Initiate a BLE connection.
- `uart_nordic.disconnect`: Disconnect the BLE link.

### Example triggers/actions
```
uart_nordic:
  id: ble_uart
  pin: 123456
  on_connected:
    - logger.log: "UART Nordic connected"
  on_disconnected:
    - logger.log: "UART Nordic disconnected"

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
