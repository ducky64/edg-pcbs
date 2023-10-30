import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY
from . import Fusb302Component, CONF_HUB_ID

DEPENDENCIES = ['fusb302']

CONFIG_SCHEMA = sensor.sensor_schema(unit_of_measurement=UNIT_EMPTY, accuracy_decimals=1).extend({
    cv.GenerateID(CONF_HUB_ID): cv.use_id(Fusb302Component)
    cv.Optional('id'): sensor.sensor_schema(
      unit_of_measurement=UNIT_EMPTY
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
