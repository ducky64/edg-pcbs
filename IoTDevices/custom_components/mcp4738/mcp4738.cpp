#include "mcp4738.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp4738 {

static const char *const TAG = "mcp4738";

MCP4738::MCP4738() {
}

float MCP4738::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP4738::setup() {
  // TBD
}

void MCP4738::dump_config() {
  // TBD
}

MCP4738Output::MCP4738Output(uint8_t channel) : channel_(channel) {};

void MCP4738Output::write_state(float state) {
  // TBD
}

}
