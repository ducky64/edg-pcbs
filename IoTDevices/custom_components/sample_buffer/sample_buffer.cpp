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

}
