import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, ble_client
from esphome.const import CONF_ID, CONF_PIN, CONF_SERVICE_UUID
from esphome import automation

CODEOWNERS = ["@latonita"]

CONF_MTU = "mtu"
CONF_ON_CONNECTED = "on_connected"
CONF_ON_DISCONNECTED = "on_disconnected"
CONF_ON_TX_COMPLETE = "on_tx_complete"
CONNECT_ACTION = "uart_nordic.connect"
DISCONNECT_ACTION = "uart_nordic.disconnect"
CONF_IDLE_TIMEOUT = "idle_timeout"
CONF_AUTOCONNECT = "autoconnect"

DEPENDENCIES = ["uart", "ble_client"]
AUTO_LOAD = ["uart", "ble_client"]

CONF_TX_UUID = "tx_uuid"
CONF_RX_UUID = "rx_uuid"

uart_nordic_ns = cg.esphome_ns.namespace("uart_nordic")
UARTNordicComponent = uart_nordic_ns.class_(
    "UARTNordicComponent", uart.UARTComponent, cg.Component
)
UARTNordicConnectAction = uart_nordic_ns.class_("UARTNordicConnectAction", automation.Action)
UARTNordicDisconnectAction = uart_nordic_ns.class_("UARTNordicDisconnectAction", automation.Action)

_UUID128_FORMAT = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"


def _uuid_128(value):
    in_value = cv.string_strict(value)
    value = in_value.upper()
    if len(value) == len(_UUID128_FORMAT):
        pattern = re.compile(r"^[A-F0-9]{8}-[A-F0-9]{4}-[A-F0-9]{4}-[A-F0-9]{4}-[A-F0-9]{12}$")
        if not pattern.match(value):
            raise cv.Invalid(f"Invalid hexadecimal value for 128-bit UUID: '{in_value}'")
        return value
    raise cv.Invalid(f"Bluetooth UUID must be in 128-bit '{_UUID128_FORMAT}' format")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UARTNordicComponent),
        cv.Optional(CONF_SERVICE_UUID, default="6E400001-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Optional(CONF_RX_UUID, default="6E400002-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Optional(CONF_TX_UUID, default="6E400003-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Required(CONF_PIN): cv.int_range(min=0, max=999999),
        cv.Optional(CONF_MTU, default=247): cv.int_range(min=23, max=517),
        cv.Optional(CONF_IDLE_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_AUTOCONNECT, default=False): cv.boolean,
        cv.Optional(CONF_ON_CONNECTED): automation.validate_automation(),
        cv.Optional(CONF_ON_DISCONNECTED): automation.validate_automation(),
        cv.Optional(CONF_ON_TX_COMPLETE): automation.validate_automation(),
    }
).extend(ble_client.BLE_CLIENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_service_uuid(config[CONF_SERVICE_UUID]))
    cg.add(var.set_rx_uuid(config[CONF_RX_UUID]))
    cg.add(var.set_tx_uuid(config[CONF_TX_UUID]))
    cg.add(var.set_passkey(config[CONF_PIN]))
    cg.add(var.set_mtu(config[CONF_MTU]))
    cg.add(var.set_idle_disconnect_timeout(config[CONF_IDLE_TIMEOUT]))
    cg.add(var.set_autoconnect_on_access(config[CONF_AUTOCONNECT]))

    if CONF_ON_CONNECTED in config:
        for conf in config[CONF_ON_CONNECTED]:
            await automation.build_automation(var.get_on_connected_trigger(), [], conf)

    if CONF_ON_DISCONNECTED in config:
        for conf in config[CONF_ON_DISCONNECTED]:
            await automation.build_automation(var.get_on_disconnected_trigger(), [], conf)

    if CONF_ON_TX_COMPLETE in config:
        for conf in config[CONF_ON_TX_COMPLETE]:
            await automation.build_automation(var.get_on_tx_complete_trigger(), [], conf)


@automation.register_action(CONNECT_ACTION, UARTNordicConnectAction, automation.maybe_simple_id({cv.GenerateID(): cv.use_id(UARTNordicComponent)}))
async def uart_nordic_connect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren)


@automation.register_action(DISCONNECT_ACTION, UARTNordicDisconnectAction, automation.maybe_simple_id({cv.GenerateID(): cv.use_id(UARTNordicComponent)}))
async def uart_nordic_disconnect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren)
