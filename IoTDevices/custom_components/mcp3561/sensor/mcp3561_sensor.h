#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"

#include "../mcp3561.h"

using namespace esphome;
namespace mcp3561 {

class MCP3561Sensor : public PollingComponent,
                      public sensor::Sensor,
                      public voltage_sampler::VoltageSampler,
                      public Parented<MCP3561> {
 public:
  MCP3561Sensor(MCP3561::Mux channel, MCP3561::Mux channel_neg = MCP3561::kAGnd);

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  MCP3561::Mux channel_;
  MCP3561::Mux channel_neg_;
};

}
