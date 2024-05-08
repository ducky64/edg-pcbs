#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/core/component.h"

#include "range.h"

using namespace esphome;
namespace range_filter {

class RangeFilterSensor : public Component, public sensor::Sensor {
 public:
  RangeFilterSensor(size_t window_size, size_t send_every);

  void dump_config() override;
  float get_setup_priority() const override;

protected:
  size_t window_size_;
  size_t send_every_;

  std::deque<float> queue_;
};

}
