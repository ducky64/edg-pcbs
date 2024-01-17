#include "mcp3561.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561";

MCP3561::MCP3561(kMux inn_channel, kOsr osr, uint8_t device_address) :
  inn_channel_(inn_channel), osr_(osr), device_address_(device_address) {}

float MCP3561::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3561::setup() {
  this->spi_setup();

  uint8_t reservedVal = readReg8(kRegister::RESERVED);  // TODO should be 16b read
  if (reservedVal == 0x000c) {
    ESP_LOGCONFIG(TAG, "Detected MCP3561");
    ESP_LOGI(TAG, "Detected MCP3561");
  } else if (reservedVal == 0x000d) {
    ESP_LOGCONFIG(TAG, "Detected MCP3562");
    ESP_LOGI(TAG, "Detected MCP3562");
  } else if (reservedVal == 0x000e) {
    ESP_LOGCONFIG(TAG, "Detected MCP3564");
    ESP_LOGI(TAG, "Detected MCP3564");
  } else {
    this->mark_failed();
    return;
  }

  writeReg8(kRegister::CONFIG0, 0xE2);  // internal VREF, internal clock w/ no CLK out, ADC standby
  writeReg8(kRegister::CONFIG1, (this->osr_ & 0xf) << 2);
  writeReg8(kRegister::CONFIG3, 0x80);  // one-shot conversion into standby, 24b encoding
  writeReg8(kRegister::IRQ, 0x07);  // enable fast command and start-conversion IRQ, IRQ logic high)
}

void MCP3561::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3561:");
  LOG_PIN("  CS Pin:", this->cs_);
  ESP_LOGCONFIG(TAG, "  IN-: %u", this->inn_channel_);
  ESP_LOGCONFIG(TAG, "  OSR: %u", this->osr_);
}

// writes 8 bits into a single register, returning the status code
uint8_t MCP3561::writeReg8(uint8_t regAddr, uint8_t data) {
  this->enable();
  uint8_t result = this->transfer_byte(((this->device_address_ & 0x3) << 6) | ((regAddr & 0xf) << 2) | kCommandType::kIncrementalWrite);
  this->transfer_byte(data);
  this->disable();
  return result;
}

// reads 8 bits from a register, returning the read data
uint8_t MCP3561::readReg8(uint8_t regAddr) {
  this->enable();
  this->transfer_byte(((this->device_address_ & 0x3) << 6) | ((regAddr & 0xf) << 2) | kCommandType::kStaticRead);
  uint8_t result = this->transfer_byte(0);
  this->disable();
  return result;
}


int32_t MCP3561::read_data(uint8_t channel) {
  // this->transfer_byte();
  return 0;
}

}
