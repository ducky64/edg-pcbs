import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ['sensor','text_sensor', 'binary_sensor']
MULTI_CONF = True

CONF_HUB_ID = 'fusb302_id'

fusb302_ns = cg.esphome_ns.namespace('fusb302')

Fusb302Component = fusb302_ns.class_('Fusb302Component', cg.Component)

CONFIG_SCHEMA = cv.All(
  cv.Schema({
    cv.GenerateID(): cv.declare_id(Fusb302Component),
  }).extend(cv.COMPONENT_SCHEMA),
  cv.only_with_arduino
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
