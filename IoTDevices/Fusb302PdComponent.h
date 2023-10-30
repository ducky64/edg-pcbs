#include "esphome.h"
#include "Fusb302.h"


class Fusb302PdComponent : public PollingComponent, public Sensor {
public:
  Fusb302PdComponent() : PollingComponent(1000), fusb_(Wire) {
  }

  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

  void setup() override {
  }

  void update() override {
    uint8_t id = 0;
    if (fusb_.readId(id)) {
      publish_state(id);
      ESP_LOGI("Fusb302Component", "got chip id %i", id);
    } else {
      ESP_LOGW("Fusb302Component", "failed to read chip id %i", id);
    }
  }

protected:
  Fusb302 fusb_;
};
