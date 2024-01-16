import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_IN_NEG = "in_neg"
CONF_OSR = "osr"

mcp3561_ns = cg.esphome_ns.namespace("mcp3561")
MCP3561 = mcp3561_ns.class_("MCP3561", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MCP3561),
    }
).extend(spi.spi_device_schema(cs_pin_required=True)
).extend(
  {
        cv.Optional(CONF_IN_NEG, default=8): cv.int_,
        cv.Optional(CONF_OSR, default=256): cv.int_,
  }
)


async def to_code(config):
    var = cg.new_Pvariable(
      config[CONF_ID],
      config[CONF_IN_NEG],
      config[CONF_OSR],
    )
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
