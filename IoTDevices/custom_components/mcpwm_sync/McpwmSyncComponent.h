#pragma once

#include <Arduino.h>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/adc/adc_sensor.h"

#include "driver/mcpwm.h"


namespace mcpwm_sync {

using namespace esphome;

class McpwmSyncComponent : public output::FloatOutput, public Component {
public:
  McpwmSyncComponent(InternalGPIOPin *pin, InternalGPIOPin *pin_comp,
    float frequency, float deadtime_rising, float deadtime_falling, float max_duty,
    adc::ADCSensor *sample_adc, float blank_frequency, float blank_time);

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;

  float get_state();

  void loop() override;

 protected:
  mcpwm_unit_t mcpwmUnit_ = MCPWM_UNIT_MAX;
  InternalGPIOPin *pin_;
  InternalGPIOPin *pin_comp_;
  float frequency_;
  float deadtime_rising_, deadtime_falling_, deadtime_duty_comp_;
  float max_duty_;

  adc::ADCSensor *sample_adc_ = nullptr;
  uint32_t blank_period_ms_, blank_time_ms_;
  float duty_ = 0;

  uint32_t nextBlankTime_ = 0;  // for blanking, in millis()

  static uint8_t nextMcpwmUnit_;
};

}
