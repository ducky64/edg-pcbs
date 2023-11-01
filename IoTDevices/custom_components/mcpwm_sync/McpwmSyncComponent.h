#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#include "driver/mcpwm.h"


namespace mcpwm_sync {

using namespace esphome;

static const char* TAG = "McpwmSyncComponent";

class McpwmSyncComponent : public output::FloatOutput, public Component {
public:
  McpwmSyncComponent(InternalGPIOPin *pin, InternalGPIOPin *pin_comp) : pin_(pin), pin_comp_(pin_comp) {
  }

  void set_frequency(float frequency) { frequency_ = frequency; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override {
    // based on example from https://github.com/espressif/esp-idf/issues/7321
    if (mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, pin_->get_pin()) != ESP_OK ||
        mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, pin_comp_->get_pin()) != ESP_OK) {
      ESP_LOGE(TAG, "failed to init GPIO");
      mark_failed();
      return;
    }

    mcpwm_config_t pwm_config;
    pwm_config.frequency = 100000;  // TODO configurable frequency
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    if (mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 10, 10) != ESP_OK) {
      ESP_LOGE(TAG, "failed to enable deadtime");
      mark_failed();
      return;
    } 

    if (mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config) != ESP_OK) {
      ESP_LOGE(TAG, "failed to init MCPWM");
      mark_failed();
      return;
    }
  }

  void dump_config() override {
  }

  void write_state(float state) override {
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, 100);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty);

    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
  }

 protected:
  InternalGPIOPin *pin_;
  InternalGPIOPin *pin_comp_;
  float frequency_;
  float duty_ = 0;
};

}
