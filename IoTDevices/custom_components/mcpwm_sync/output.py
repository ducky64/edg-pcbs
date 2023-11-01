from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_CHANNEL,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PIN,
)

DEPENDENCIES = ["esp32"]

CONF_PIN_COMP = 'pin_comp'  # complementary pin


mcpwm_sync_ns = cg.esphome_ns.namespace("mcpwm_sync")
McpwmSyncOutput = mcpwm_sync_ns.class_("McpwmSyncComponent", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(McpwmSyncOutput),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_COMP): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): cv.frequency,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    gpio = await cg.gpio_pin_expression(config[CONF_PIN])
    gpio_comp = await cg.gpio_pin_expression(config[CONF_PIN_COMP])
    var = cg.new_Pvariable(config[CONF_ID], gpio, gpio_comp)

    await cg.register_component(var, config)
    await output.register_output(var, config)
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
