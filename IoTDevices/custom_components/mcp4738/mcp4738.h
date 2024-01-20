#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

using namespace esphome;
namespace mcp4738 {

class MCP4738 : public Component, public i2c::I2CDevice {
 public:
  MCP4738();

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  
protected:
  
};

class MCP4738Output : public output::FloatOutput,
                      public Parented<MCP4738> {
 public:
  MCP4738Output(uint8_t channel);

  void write_state(float state) override;

 protected:
  uint8_t channel_;
};

}
