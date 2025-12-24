import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_SERVICE_UUID
from esphome import automation
from esphome.components import uart

CONF_MTU = "mtu"
CONF_IDLE_TIMEOUT = "idle_timeout"
CONF_AUTOCONNECT = "autoconnect"
CONF_RX_UUID = "rx_uuid"
CONF_TX_UUID = "tx_uuid"

CONF_ON_CONNECTED = "on_connected"
CONF_ON_DISCONNECTED = "on_disconnected"
CONF_ON_SENT = "on_sent"
CONF_ON_DATA = "on_data"

START_ADVERTISING_ACTION = "ble_nus_server.start_advertising"
STOP_ADVERTISING_ACTION = "ble_nus_server.stop_advertising"
DISCONNECT_ACTION = "ble_nus_server.disconnect"

DEPENDENCIES = ["esp32_ble", "esp32_ble_server", "uart"]
AUTO_LOAD = ["uart", "esp32_ble"]

ble_nus_server_ns = cg.esphome_ns.namespace("ble_nus_server")
BLENUSServerComponent = ble_nus_server_ns.class_(
    "BLENUSServerComponent", uart.UARTComponent, cg.Component
)
StartAdvertisingAction = ble_nus_server_ns.class_("StartAdvertisingAction", automation.Action)
StopAdvertisingAction = ble_nus_server_ns.class_("StopAdvertisingAction", automation.Action)
DisconnectAction = ble_nus_server_ns.class_("DisconnectAction", automation.Action)


def _uuid_128(value):
    value = cv.string_strict(value)
    if len(value) != 36:
        raise cv.Invalid("Bluetooth UUID must be 128-bit (36 chars including hyphens)")
    return value.upper()


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLENUSServerComponent),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=999999),
        cv.Optional(CONF_SERVICE_UUID, default="6E400001-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Optional(CONF_RX_UUID, default="6E400002-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Optional(CONF_TX_UUID, default="6E400003-B5A3-F393-E0A9-E50E24DCCA9E"): _uuid_128,
        cv.Optional(CONF_MTU, default=247): cv.int_range(min=23, max=517),
        cv.Optional(CONF_IDLE_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_AUTOCONNECT, default=True): cv.boolean,
        cv.Optional(CONF_ON_CONNECTED): automation.validate_automation(),
        cv.Optional(CONF_ON_DISCONNECTED): automation.validate_automation(),
        cv.Optional(CONF_ON_SENT): automation.validate_automation(),
        cv.Optional(CONF_ON_DATA): automation.validate_automation(),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_service_uuid(config[CONF_SERVICE_UUID]))
    cg.add(var.set_rx_uuid(config[CONF_RX_UUID]))
    cg.add(var.set_tx_uuid(config[CONF_TX_UUID]))
    cg.add(var.set_passkey(config[CONF_PIN]))
    cg.add(var.set_mtu(config[CONF_MTU]))
    cg.add(var.set_idle_disconnect_timeout(config[CONF_IDLE_TIMEOUT]))
    cg.add(var.set_autoadvertise(config[CONF_AUTOCONNECT]))

    if CONF_ON_CONNECTED in config:
        for conf in config[CONF_ON_CONNECTED]:
            await automation.build_automation(var.get_on_connected_trigger(), [], conf)

    if CONF_ON_DISCONNECTED in config:
        for conf in config[CONF_ON_DISCONNECTED]:
            await automation.build_automation(var.get_on_disconnected_trigger(), [], conf)

    if CONF_ON_SENT in config:
        for conf in config[CONF_ON_SENT]:
            await automation.build_automation(var.get_on_sent_trigger(), [], conf)

    if CONF_ON_DATA in config:
        for conf in config[CONF_ON_DATA]:
            await automation.build_automation(var.get_on_data_trigger(), [], conf)


@automation.register_action(START_ADVERTISING_ACTION, StartAdvertisingAction, cv.Schema({cv.GenerateID(): cv.use_id(BLENUSServerComponent)}))
async def start_adv_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren)


@automation.register_action(STOP_ADVERTISING_ACTION, StopAdvertisingAction, cv.Schema({cv.GenerateID(): cv.use_id(BLENUSServerComponent)}))
async def stop_adv_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren)


@automation.register_action(DISCONNECT_ACTION, DisconnectAction, cv.Schema({cv.GenerateID(): cv.use_id(BLENUSServerComponent)}))
async def disconnect_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, paren)
