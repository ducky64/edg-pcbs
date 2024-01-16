#include "mcp3561.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561";

float MCP3561::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3561::setup() {
  // TODO IMPLEMENT ME
  this->spi_setup();
}

void MCP3561::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3561:");
  LOG_PIN("  CS Pin:", this->cs_);
}

uint16_t MCP3561::read_data(uint8_t channel) {
  // TODO IMPLEMENT ME
  return 0;
}

}
