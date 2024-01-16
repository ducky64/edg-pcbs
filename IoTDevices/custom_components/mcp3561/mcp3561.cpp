#include "mcp3561.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561";

MCP3561::MCP3561(uint8_t inn_channel, uint8_t osr) : inn_channel_(inn_channel), osr_(osr) {}

float MCP3561::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3561::setup() {
  this->spi_setup();
}

void MCP3561::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3561:");
  LOG_PIN("  CS Pin:", this->cs_);
}

int32_t MCP3561::read_data(uint8_t channel) {
  // this->transfer_byte();
  return 0;
}

}
