import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

AUTO_LOAD = ["output"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

mcp4738_ns = cg.esphome_ns.namespace("mcp4738")
MCP4738 = mcp4738_ns.class_("MCP4738", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP4738),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x60))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    return var
