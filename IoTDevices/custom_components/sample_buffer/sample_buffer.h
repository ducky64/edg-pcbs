#pragma once

#include <map>
#include <utility>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"

using namespace esphome;

namespace sample_buffer {

class SampleBuffer : public AsyncWebHandler, public Component {
 public:
  SampleBuffer(web_server_base::WebServerBase *base) : base_(base) {}

  // Add a sensor source to this sample buffer
  void add_source(sensor::Sensor *source, const std::string &name);

  bool canHandle(AsyncWebServerRequest *request);

  void handleRequest(AsyncWebServerRequest *req) override;

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }
  float get_setup_priority() const override {
    // After WiFi
    return setup_priority::WIFI - 1.0f;
  }

 protected:
  web_server_base::WebServerBase *base_;

  void new_value(uint32_t millis, float value, const std::string &source_name);
};

}
