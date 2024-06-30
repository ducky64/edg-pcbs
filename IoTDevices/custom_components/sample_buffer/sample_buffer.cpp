#include "sample_buffer.h"
#include "esphome/core/application.h"

namespace sample_buffer {

bool SampleBuffer::canHandle(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    if (request->url() == "/samples")
      return true;
  }

  return false;
}

void SampleBuffer::handleRequest(AsyncWebServerRequest *req) {
  AsyncResponseStream *stream = req->beginResponseStream("text/plain; version=0.0.4; charset=utf-8");

  req->send(stream);
}

void SampleBuffer::add_source(sensor::Sensor *source, const std::string &name) {
  // TODO: should name be copied to have a local copy?
  source->add_on_state_callback(
    [this, name](float value) -> void { 
      uint32_t timestamp = esphome::millis();
      this->defer("update", [this, name, timestamp, value]() { 
        this->new_value(timestamp, value, name); 
      }); 
    }
  );
}

void SampleBuffer::new_value(uint32_t millis, float value, const std::string &source_name) {

}

}
