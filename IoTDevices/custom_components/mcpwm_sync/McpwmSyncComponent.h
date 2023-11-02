#pragma once

#include <Arduino.h>

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
  McpwmSyncComponent(InternalGPIOPin *pin, InternalGPIOPin *pin_comp,
    float frequency, float deadtime_rising, float deadtime_falling,
    adc::ADCSensor *sample_adc, float blank_frequency, float blank_time) :
    pin_(pin), pin_comp_(pin_comp),
    frequency_(frequency), deadtime_rising_(deadtime_rising), deadtime_falling_(deadtime_falling),
    sample_adc_(sample_adc), blank_period_ms_(1 / blank_frequency / 1e-3), blank_time_ms_(blank_time / 1e-3) {
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override {
    // based on example from https://github.com/espressif/esp-idf/issues/7321
    ESP_LOGI(TAG, "MCPWM setup 0A=%i, 0B=%i", pin_->get_pin(), pin_comp_->get_pin());
    if (mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, pin_->get_pin()) != ESP_OK ||
        mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, pin_comp_->get_pin()) != ESP_OK) {
      ESP_LOGE(TAG, "failed to init GPIO");
      status_set_error();
      return;
    }

    mcpwm_config_t pwm_config;
    pwm_config.frequency = frequency_;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    if (mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config) != ESP_OK) {
      ESP_LOGE(TAG, "failed to init MCPWM");
      status_set_error();
      return;
    }

    if (mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 
        deadtime_rising_ / 100e-9, deadtime_falling_ / 100e-9) != ESP_OK) {
      ESP_LOGE(TAG, "failed to enable deadtime");
      status_set_error();
      return;
    } 

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, 0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, 100);
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);

    ESP_LOGI(TAG, "MCPWM setup complete");
  }

  void dump_config() override {
  }

  void write_state(float state) override {
    duty_ = state * 100;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty_);
  }

  float get_state() { return duty_; }

  void loop() override {
    if (blank_time_ms_ > 0 && esphome::millis() >= nextBlankTime_) {
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, 0);  // blank
      esphome::delay(blank_time_ms_);
      if (sample_adc_ != nullptr) {
        sample_adc_->update();
      }
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty_);  // resume PWMing
      nextBlankTime_ += blank_period_ms_;
    }
  }

 protected:
  InternalGPIOPin *pin_;
  InternalGPIOPin *pin_comp_;
  float frequency_;
  float deadtime_rising_, deadtime_falling_;

  adc::ADCSensor *sample_adc_ = nullptr;
  uint32_t blank_period_ms_, blank_time_ms_;
  float duty_ = 0;

  uint32_t nextBlankTime_ = 0;  // for blanking, in millis()
};

}
