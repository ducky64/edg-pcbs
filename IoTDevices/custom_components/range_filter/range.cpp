#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/core/component.h"

#include "range.h"

using namespace esphome;
namespace range_filter {

RangeFilterSensor::RangeFilterSensor(size_t window_size, size_t send_every)
    : window_size_(window_size), send_every_(send_every) {}


float RangeFilterSensor::get_setup_priority() const { return setup_priority::DATA; }

void RangeFilterSensor::dump_config() {
  LOG_SENSOR("", "RangeFilterSensor Sensor", this);
}

}
