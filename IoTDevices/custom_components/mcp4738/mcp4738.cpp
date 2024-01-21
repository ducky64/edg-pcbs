#include "mcp4738.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp4738 {

static const char *const TAG = "mcp4738";

MCP4738::MCP4738() {
}

float MCP4738::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP4738::setup() {
  uint8_t buf[1];
  if (!this->read_bytes_raw(buf, 1)) {
    ESP_LOGE(TAG, "device read failed");
    this->mark_failed();
  }
}

void MCP4738::dump_config() {
  // no config to be dumped
}

void MCP4738::writeChannel(uint8_t channel, uint16_t data, bool upload, Reference ref, bool gain, PowerDown power) {
  if (channel > 3) {
    ESP_LOGE(TAG, "invalid channel %i", channel);
    return;
  }
  if (data > 4095) {
    ESP_LOGE(TAG, "data out of range, clamping: %u", data);
   data = 4095;
  }
  this->write_byte_16((kMultiWriteDac << 3) | (channel << 1) | !upload,
    (ref << 15) | (power << 13) | (gain << 12) | data);
}

MCP4738Output::MCP4738Output(uint8_t channel) : channel_(channel) {};

void MCP4738Output::write_state(float state) {
  uint16_t data = state * 4095;
  if (data > 4095) {
    ESP_LOGE(TAG, "data out of range %f", state);
  }
  parent_->writeChannel(channel_, data, true);
}

}
