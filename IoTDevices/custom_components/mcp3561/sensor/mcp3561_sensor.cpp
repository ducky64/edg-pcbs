#include "mcp3561_sensor.h"

#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561.sensor";

MCP3561Sensor::MCP3561Sensor(MCP3561::kMux channel) : channel_(channel) {}

float MCP3561Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3561Sensor::dump_config() {
  LOG_SENSOR("", "MCP3561Sensor Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->channel_);
  LOG_UPDATE_INTERVAL(this);
}

float MCP3561Sensor::sample() { return this->parent_->read_data(this->channel_); }
void MCP3561Sensor::update() { this->publish_state(this->sample()); }

}
