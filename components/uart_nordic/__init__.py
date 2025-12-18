import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PIN, CONF_SERVICE_UUID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["uart"]

CONF_TX_UUID = "tx_uuid"
CONF_RX_UUID = "rx_uuid"

uart_nordic_ns = cg.esphome_ns.namespace("uart_nordic")
UARTNordicComponent = uart_nordic_ns.class_(
    "UARTNordicComponent", uart.UARTComponent, cg.Component
)

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
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_service_uuid(config[CONF_SERVICE_UUID]))
    cg.add(var.set_rx_uuid(config[CONF_RX_UUID]))
    cg.add(var.set_tx_uuid(config[CONF_TX_UUID]))
    cg.add(var.set_passkey(config[CONF_PIN]))
