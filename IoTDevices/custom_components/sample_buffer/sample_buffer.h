#pragma once

#include <map>
#include <utility>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"

using namespace esphome;

namespace sample_buffer {

class SampleBuffer : public AsyncWebHandler, public Component {
 public:
  SampleBuffer(web_server_base::WebServerBase *base) : base_(base) {}

  /** Add the value for an entity's "id" label.
   *
   * @param obj The entity for which to set the "id" label
   * @param value The value for the "id" label
   */
  void add_label_id(EntityBase *obj, const std::string &value) {
   }

  /** Add the value for an entity's "name" label.
   *
   * @param obj The entity for which to set the "name" label
   * @param value The value for the "name" label
   */
  void add_label_name(EntityBase *obj, const std::string &value) { 
  }

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
};

}
