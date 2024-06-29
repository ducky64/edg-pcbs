import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_INCLUDE_INTERNAL,
    CONF_RELABEL,
)
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base
from esphome.cpp_types import EntityBase

AUTO_LOAD = ["web_server_base"]

sample_buffer_ns = cg.esphome_ns.namespace("sample_buffer")
SampleBuffer = sample_buffer_ns.class_("SampleBuffer", cg.Component)

CUSTOMIZED_ENTITY = cv.Schema(
    {
        cv.Optional(CONF_ID): cv.string_strict,
        cv.Optional(CONF_NAME): cv.string_strict,
    },
    cv.has_at_least_one_key,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SampleBuffer),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Optional(CONF_RELABEL, default={}): cv.Schema(
            {
                cv.use_id(EntityBase): CUSTOMIZED_ENTITY,
            }
        ),
    },
    # cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    for key, value in config[CONF_RELABEL].items():
        entity = await cg.get_variable(key)
        if CONF_ID in value:
            cg.add(var.add_label_id(entity, value[CONF_ID]))
        if CONF_NAME in value:
            cg.add(var.add_label_name(entity, value[CONF_NAME]))
