#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "Fusb302.h"
#include "UsbPdStateMachine.h"

using namespace esphome;

namespace fusb302 {

class Fusb302Component : public PollingComponent {
public:
  sensor::Sensor* sensor_id_ = nullptr;
  void set_id_sensor(sensor::Sensor* that) { sensor_id_ = that; }

  text_sensor::TextSensor* sensor_status_ = nullptr;
  void set_status_text_sensor(text_sensor::TextSensor* that) { sensor_status_ = that; }
  
  Fusb302Component() : PollingComponent(1000), fusb_(Wire), pd_fsm_(fusb_) {
  }

  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

  void setup() override {
    if (fusb_.readId(id_)) {
      sensor_id_->publish_state(id_);
      ESP_LOGI("Fusb302Component", "got chip id 0x%02x", id_);
    } else {
      ESP_LOGW("Fusb302Component", "failed to read chip id");
      mark_failed();
    }
  }

  void update() override {
    

  }

protected:
  Fusb302 fusb_;
  UsbPdStateMachine pd_fsm_;

  uint8_t id_;  // device id, if read successful
};

}
