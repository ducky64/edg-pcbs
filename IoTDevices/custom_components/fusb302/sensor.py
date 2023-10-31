import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY
from . import Fusb302Component, CONF_FUSB302_ID

DEPENDENCIES = ['fusb302']

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_FUSB302_ID): cv.use_id(Fusb302Component),
    cv.Optional('id'): sensor.sensor_schema(
      unit_of_measurement=UNIT_EMPTY
    ),
})


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))

async def to_code(config):
    hub = await cg.get_variable(config[CONF_FUSB302_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
