#include "esphome.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "Fusb302.h"


class Fusb302Component : public PollingComponent {
public:
  Sensor sensor_id;
  text_sensor::TextSensor sensor_status;

  Fusb302Component() : PollingComponent(1000), fusb_(Wire) {
  }

  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

  void setup() override {
    if (fusb_.readId(id_)) {
      sensor_id.publish_state(id_);
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
  uint8_t id_;  // device id, if read successful
};
