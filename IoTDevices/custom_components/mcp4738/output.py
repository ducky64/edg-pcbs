import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import MCP4738, mcp4738_ns

DEPENDENCIES = ["mcp4738"]

MCP4738Output = mcp4738_ns.class_("MCP4738Output", output.FloatOutput)
CONF_MCP4738_ID = "mcp4738_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MCP4738Output),
        cv.GenerateID(CONF_MCP4738_ID): cv.use_id(MCP4738),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(
      config[CONF_ID],
      config[CONF_CHANNEL]
      )
    await cg.register_parented(var, config[CONF_MCP4738_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)