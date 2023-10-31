#include <sstream>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "Fusb302.h"
#include "UsbPdStateMachine.h"


namespace fusb302 {

using namespace esphome;

static const char* TAG = "Fusb302Component";

class Fusb302Component : public Component {
public:
  sensor::Sensor* sensor_cc_ = nullptr;
  void set_cc_sensor(sensor::Sensor* that) { sensor_cc_ = that; }
  sensor::Sensor* sensor_vbus_ = nullptr;
  void set_vbus_sensor(sensor::Sensor* that) { sensor_vbus_ = that; }

  text_sensor::TextSensor* sensor_status_ = nullptr;
  void set_status_text_sensor(text_sensor::TextSensor* that) { sensor_status_ = that; }
  text_sensor::TextSensor* sensor_capabilities_ = nullptr;
  void set_capabilities_text_sensor(text_sensor::TextSensor* that) { sensor_capabilities_ = that; }

  Fusb302Component() : fusb_(Wire), pd_fsm_(fusb_) {
  }

  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

  void setup() override {
    if (fusb_.readId(id_)) {
      ESP_LOGCONFIG(TAG, "got chip id 0x%02x", id_);
    } else {
      ESP_LOGCONFIG(TAG, "failed to read chip id");
      mark_failed();
    }
  }

  void loop() override {
    uint16_t vbusMv;
    if (pd_fsm_.updateVbus(vbusMv)) {
      sensor_vbus_->publish_state((float)vbusMv / 1000);
    }

    UsbPdStateMachine::UsbPdState state = pd_fsm_.update();
    if (state != last_state_) {
      switch (state) {
        case UsbPdStateMachine::kStart: sensor_status_->publish_state("Start"); break;
        case UsbPdStateMachine::kDetectCc: sensor_status_->publish_state("Detect CC"); break;
        case UsbPdStateMachine::kEnableTransceiver: sensor_status_->publish_state("Enable Transceiver"); break;
        case UsbPdStateMachine::kWaitSourceCapabilities: sensor_status_->publish_state("Wait Capabilities"); break;
        case UsbPdStateMachine::kConnected: sensor_status_->publish_state("Connected"); break;
      }
    }

    if (last_state_ < UsbPdStateMachine::kEnableTransceiver && state >= UsbPdStateMachine::kEnableTransceiver) {  // cc available
      sensor_cc_->publish_state(pd_fsm_.getCcPin());
    } else if (last_state_ >= UsbPdStateMachine::kEnableTransceiver && state < UsbPdStateMachine::kEnableTransceiver) {  // disconnected
      sensor_cc_->publish_state(0);
    }

    if (last_state_ < UsbPdStateMachine::kConnected && state >= UsbPdStateMachine::kConnected) {  // capabilities now available
      std::stringstream ss;
      UsbPd::Capability::Unpacked capabilities[UsbPd::Capability::kMaxCapabilities];
      uint8_t capabilitiesCount = pd_fsm_.getCapabilities(capabilities);
      for (uint8_t capabilityIndex=0; capabilityIndex<capabilitiesCount; capabilityIndex++) {
        UsbPd::Capability::Unpacked& capability = capabilities[capabilityIndex];
        if (capabilityIndex > 0) {
          ss << "; ";
        }
        if (capability.capabilitiesType == UsbPd::Capability::Type::kFixedSupply) {
          ss << capability.voltageMv / 1000 << "." << (capability.voltageMv / 100) % 10 <<  "V " 
            << capability.maxCurrentMa / 1000 << "." << (capability.maxCurrentMa / 100) % 10 << "A" ;
          if (capability.dualRolePower) {
            ss << " dual-role";
          }
          if (!capability.unconstrainedPower) {
            ss << " constrained";
          }
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kVariable) {
          ss << "unknown variable";
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kAugmented) {
          ss << "unknown augmented";
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kBattery) {
          ss << "unknown battery";
        }
      }
      sensor_capabilities_->publish_state(ss.str());
    } else if (last_state_ >= UsbPdStateMachine::kConnected && state < UsbPdStateMachine::kConnected) {  // disconnected
      sensor_capabilities_->publish_state("");
    }

    last_state_ = state;
  }

protected:
  Fusb302 fusb_;
  UsbPdStateMachine pd_fsm_;
  UsbPdStateMachine::UsbPdState last_state_ = UsbPdStateMachine::kStart;

  uint8_t id_;  // device id, if read successful
};

}
