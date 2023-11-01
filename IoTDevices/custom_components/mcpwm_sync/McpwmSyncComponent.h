#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"


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

  }

  void dump_config() override {
  }

  void write_state(float state) override {

  }

 protected:
  InternalGPIOPin *pin_;
  InternalGPIOPin *pin_comp_;
  float frequency_;
  float duty_ = 0;
};

}
