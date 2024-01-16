#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"

#include "../mcp3561.h"

using namespace esphome;
namespace mcp3561 {

class MCP3561Sensor : public PollingComponent,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler {
 public:
  MCP3561Sensor(uint8_t channel);

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  uint8_t channel_;
};

}
