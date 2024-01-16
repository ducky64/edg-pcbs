import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import CONF_ID, CONF_CHANNEL, UNIT_VOLT, STATE_CLASS_MEASUREMENT, DEVICE_CLASS_VOLTAGE

from .. import mcp3561_ns, MCP3561

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["mcp3561"]

MCP3561Sensor = mcp3561_ns.class_(
    "MCP3561Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
)
CONF_MCP3561_ID = "mcp3561_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
      MCP3561Sensor,
      unit_of_measurement=UNIT_VOLT,
      state_class=STATE_CLASS_MEASUREMENT,
      device_class=DEVICE_CLASS_VOLTAGE,
    ).extend(
        {
            cv.GenerateID(CONF_MCP3561_ID): cv.use_id(MCP3561),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CHANNEL],
    )
    await cg.register_parented(var, config[CONF_MCP3561_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
