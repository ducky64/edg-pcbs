#include "range.h"

using namespace esphome;
namespace range_filter {

RangeFilter::RangeFilter(size_t window_size, size_t send_every)
    : window_size_(window_size), send_every_(send_every) {}

optional<float> RangeFilter::new_value(float value) {

}

}
