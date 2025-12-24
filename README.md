[DLMS/COSEM](https://github.com/latonita/esphome-dlms-cosem) •
[IEC-61107](https://github.com/latonita/esphome-iec61107-meter) •
[Energomera IEC](https://github.com/latonita/esphome-energomera-iec) •
[Energomera CE](https://github.com/latonita/esphome-energomera-ce) •
[SPb ZIP CE2727A](https://github.com/latonita/esphome-ce2727a-meter) •
[Lenelektro LE-2](https://github.com/latonita/esphome-le2-meter) •
[Pulsar-M](https://github.com/latonita/esphome-pulsar-m) •
[Energomera BLE](https://github.com/latonita/esphome-energomera-ble) •
[Nordic UART (BLE NUS)](https://github.com/latonita/esphome-nordic-uart-ble)

# ESPHome BLE NUS (Nordic UART Service) Client

A drop-in UART-like transport over Bluetooth LE Nordic UART Service (NUS). It implements the usual UART surface (`write_array`, `read_array`, `peek_byte`, `available`, `flush`) plus explicit `connect()` / `disconnect()`, optional auto-connect, and an optional idle auto-disconnect. You can point existing UART-based components at this transport to replace a wired UART with BLE NUS without code changes in the consumer.

Server part is under development yet.

## What is NUS (Nordic UART Service)?
Nordic UART Service emulates a serial link over BLE using two characteristics: RX (writes from central to peripheral) and TX (notifications from peripheral to central). The central subscribes to TX notifications, and once that’s acknowledged it writes outbound data to RX. Each packet is limited by the GATT payload (often 20 bytes unless MTU negotiation raises it); the higher-level framing/interpretation is entirely application-specific.

Default UUIDs for NUS are:
- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- RX: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- TX: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

Some devices ship with alternate UUIDs (common variant):
- Service: `6e400001-b5a3-f393-e0a9-e50e24dc4179`
- RX: `6e400002-b5a3-f393-e0a9-e50e24dc4179`
- TX: `6e400003-b5a3-f393-e0a9-e50e24dc4179`

Adjust the UUIDs in YAML to match your target peripheral if needed.


### Migrating from wired UART to Nordic UART Client
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
ble_nus_client:
  id: ble_uart
  pin: 123456
  # optional: idle_timeout: 5min
  # optional: connect_on_demand: true

some_component:
  uart_id: ble_uart
```


## YAML Configuration
```yaml
external_components:
  - source: github://latonita/esphome-nordic-uart-ble
    refresh: 10s
    components: [ble_nus_client]

ble_nus_client:
  id: ble_uart
  pin: 123456
  service_uuid: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
  rx_uuid: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
  tx_uuid: 6e400003-b5a3-f393-e0a9-e50e24dcca9e
  mtu: 247
  idle_timeout: 0s             # optional, default disables auto-disconnect
  connect_on_demand: false     # optional, auto-connect on UART access when disconnected

```


## Configuration variables
- **id** (Required): Component ID.
- **pin** (Required, int): 6-digit BLE pairing PIN.
- **service_uuid** (Optional, string): NUS service UUID. Default `6e400001-b5a3-f393-e0a9-e50e24dcca9e`.
- **rx_uuid** (Optional, string): NUS RX characteristic (writes from client). Default `6e400002-b5a3-f393-e0a9-e50e24dcca9e`.
- **tx_uuid** (Optional, string): NUS TX characteristic (notifications to client). Default `6e400003-b5a3-f393-e0a9-e50e24dcca9e`.
- **mtu** (Optional, int): Desired MTU, 23–517. Default `247`.
- **idle_timeout** (Optional, time): Auto-disconnect after no RX/TX activity. `0s` disables (default).
- **connect_on_demand** (Optional, bool): If `true`, any UART access while disconnected will trigger a BLE connect attempt (once per second max). Default `false`.
- All other options from `ble_client`.

## Automations
- `on_connected`: Fired when the BLE UART link established.
- `on_disconnected`: Fired when the BLE UART link closed.
- `on_sent`: Fired when transmission finished and confirmed by remote device.
- `on_data`: Fired when any notification payload is received.

## Actions
- `ble_nus_client.connect`: Initiate a BLE connection.
- `ble_nus_client.disconnect`: Disconnect the BLE link.
- `ble_nus_client.send`: Send data (list of bytes or string) over NUS.

### Example triggers/actions
```
ble_nus_client:
  id: ble_uart
  pin: 123456
  on_connected:
    - logger.log: "BLE NUS Client connected"
  on_disconnected:
    - logger.log: "BLE NUS Client disconnected"
  on_sent:
    - logger.log: "Data Transmission Completed"
  on_data:
    - logger.log: "Data Received"

button:
  - platform: template
    name: "NUS Connect"
    on_press:
      - ble_nus_client.connect: ble_uart
  - platform: template
    name: "NUS Disconnect"
    on_press:
      - ble_nus_client.disconnect: ble_uart

  - platform: template
    name: "Send Hello"
    on_press:
      - ble_nus_client.send:
          id: ble_uart
          data: "Hello\n"

  - platform: template
    name: "Send Bytes"
    on_press:
      - ble_nus_client.send:
          id: ble_uart
          data: [0x01, 0x02, 0x03, 0x04, 0x05]
```
