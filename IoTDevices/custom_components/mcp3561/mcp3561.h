#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

using namespace esphome;
namespace mcp3561 {

// note: device compatible with SPI Modes 0,0 and 1,1
class MCP3561 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_20MHZ> {
 public:
  enum kRegister {
    ADCDATA = 0x0,
    CONFIG0 = 0x1,
    CONFIG1 = 0x2,
    CONFIG2 = 0x3,
    CONFIG3 = 0x4,
    IRQ = 0x5,
    MUX = 0x6,
    SCAN = 0x7,
    TIMER = 0x8,
    OFFSETCAL = 0x9,
    GAINCAL = 0xA,
    LOCK = 0xD,
    CRCCFG = 0xF
  };

  enum kCommandType {
    kFastCommand = 0,
    kStaticRead = 1,
    kIncrementalWrite = 2,
    kIncrementalRead = 3
  };

  enum kFastCommand {
    kStartConversion = 0xA,  // ADC_MODE[1:0] = 0b11
    kStandbyMode = 0xB,  // ADC_MODE[1:0] = 0b10
    kShutdownMode = 0xC,  // ADC_MODE[1:0] = 0b00
    kFullShutdownMode = 0xD,  // CONFIG0[7:0] = 0x00
    kDeviceFullReset = 0xE  // resets entire register map
  };

  enum kOsr {
    k98304 = 0xf,
    k81920 = 0xe,
    k49152 = 0xd,
    k40960 = 0xc,
    k24576 = 0xb,
    k20480 = 0xa,
    k16384 = 0x9,
    k8192 = 0x8,
    k4096 = 0x7,
    k2048 = 0x6,
    k1024 = 0x5,
    k512 = 0x4,
    k256 = 0x3,  // default
    k128 = 0x2,
    k64 = 0x1,
    k32 = 0x0
  };

  enum kMux {
    kCh0 = 0x0,
    kCh1 = 0x1,
    kCh2 = 0x2,
    kCh3 = 0x3,
    kCh4 = 0x4,
    kCh5 = 0x5,
    kCh6 = 0x6,
    kCh7 = 0x7,
    kAGnd = 0x8,
    kVdd = 0x9,
    // reserved "do not use" 0xa
    kRefInP = 0xb,
    kRefInN = 0xc,
    kTempDiodeP = 0xd,
    kTempDiodeM = 0xe,
    kVCm = 0xf,
  };

  MCP3561(kMux inn_channel, kOsr osr);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  int32_t read_data(uint8_t channel);  // reads data as a 24-bit signed value

protected:
  kMux inn_channel_;
  kOsr osr_;
};

}
