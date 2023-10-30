import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import Fusb302Component, CONF_HUB_ID

DEPENDENCIES = ['fusb302']

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(CONF_HUB_ID): cv.use_id(Fusb302Component),
    cv.Optional('status'): text_sensor.text_sensor_schema(
      icon=ICON_ACCURACY
    ),
}).extend(cv.COMPONENT_SCHEMA)

async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME680_BSEC_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
