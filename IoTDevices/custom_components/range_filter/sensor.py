import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_WINDOW_SIZE, CONF_SEND_EVERY

from esphome.components.sensor import Filter, FILTER_REGISTRY

range_filter_ns = cg.esphome_ns.namespace("range_filter")
RangeFilterSensor = range_filter_ns.class_(
    "RangeFilterSensor", sensor.Sensor, cg.Component,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
      RangeFilterSensor
    ).extend(
        {
            cv.Required(CONF_ID): cv.declare_id(RangeFilterSensor),
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
        }
    )
)


async def to_code(config):
    # var = await sensor.new_sensor(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)


RangeFilter = range_filter_ns.class_("RangeFilter", Filter)

RANGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
        }
    ),
)

@FILTER_REGISTRY.register("range", RangeFilter, RANGE_SCHEMA)
async def min_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
    )
