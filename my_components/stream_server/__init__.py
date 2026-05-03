from esphome.components import uart
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["async_tcp"]
CODEOWNERS = ["@RoganDawes"]
MULTI_CONF = True

stream_server_ns = cg.esphome_ns.namespace("stream_server")
StreamServerComponent = stream_server_ns.class_(
    "StreamServerComponent", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StreamServerComponent),
            cv.Optional(CONF_PORT, default=6638): cv.port,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
