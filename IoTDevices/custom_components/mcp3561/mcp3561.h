#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

using namespace esphome;
namespace mcp3561 {

class MCP3561 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_10MHZ> {
 public:
  MCP3561() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  uint16_t read_data(uint8_t channel);
};

}
