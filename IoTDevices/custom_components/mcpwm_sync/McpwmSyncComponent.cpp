#include "McpwmSyncComponent.h"


namespace mcpwm_sync {

using namespace esphome;

static const char* TAG = "McpwmSyncComponent";

McpwmSyncComponent::McpwmSyncComponent(InternalGPIOPin *pin, InternalGPIOPin *pin_comp,
  float frequency, float deadtime_rising, float deadtime_falling, float max_duty,
  adc::ADCSensor *sample_adc, float blank_frequency, float blank_time) :
  pin_(pin), pin_comp_(pin_comp), frequency_(frequency),
  deadtime_rising_(deadtime_rising), deadtime_falling_(deadtime_falling), max_duty_(max_duty),
  sample_adc_(sample_adc), blank_period_ms_(1 / blank_frequency / 1e-3), blank_time_ms_(blank_time / 1e-3) {
}

float McpwmSyncComponent::get_setup_priority() const {
  return setup_priority::HARDWARE; 
}

void McpwmSyncComponent::setup() {
  const uint8_t kResolutionFactor = 16;

  if (McpwmSyncComponent::nextMcpwmUnit_ == 0) {
    ESP_LOGI(TAG, "allocate MCPWM 0");
    mcpwmUnit_ = MCPWM_UNIT_0;
    McpwmSyncComponent::nextMcpwmUnit_++;
  } else if (McpwmSyncComponent::nextMcpwmUnit_ == 1) {
    ESP_LOGI(TAG, "allocate MCPWM 1");
    mcpwmUnit_ = MCPWM_UNIT_1;
    McpwmSyncComponent::nextMcpwmUnit_++;
  } else {
    ESP_LOGE(TAG, "no mcpwm to allocate: %i", mcpwmUnit_);
    status_set_error();
    return;
  }

  // based on example from https://github.com/espressif/esp-idf/issues/7321
  ESP_LOGI(TAG, "MCPWM setup 0A=%i, 0B=%i", pin_->get_pin(), pin_comp_->get_pin());
  if (mcpwm_gpio_init(mcpwmUnit_, MCPWM0A, pin_->get_pin()) != ESP_OK ||
      mcpwm_gpio_init(mcpwmUnit_, MCPWM0B, pin_comp_->get_pin()) != ESP_OK) {
    ESP_LOGE(TAG, "failed to init GPIO");
    status_set_error();
    return;
  }

  mcpwm_config_t pwm_config;
  pwm_config.frequency = frequency_;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  if (mcpwm_group_set_resolution(mcpwmUnit_, 10000000 * kResolutionFactor) != ESP_OK ||
      mcpwm_timer_set_resolution(mcpwmUnit_, MCPWM_TIMER_0, 1000000 * kResolutionFactor) != ESP_OK ||
      mcpwm_timer_set_resolution(mcpwmUnit_, MCPWM_TIMER_1, 1000000 * kResolutionFactor) != ESP_OK ||
      mcpwm_timer_set_resolution(mcpwmUnit_, MCPWM_TIMER_2, 1000000 * kResolutionFactor) != ESP_OK ||
      mcpwm_init(mcpwmUnit_, MCPWM_TIMER_0, &pwm_config) != ESP_OK) {
    ESP_LOGE(TAG, "failed to init MCPWM");
    status_set_error();
    return;
  }

  if (mcpwm_deadtime_enable(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 
      deadtime_rising_ / 100e-9 * kResolutionFactor, deadtime_falling_ / 100e-9 * kResolutionFactor) != ESP_OK) {
    ESP_LOGE(TAG, "failed to enable deadtime");
    status_set_error();
    return;
  }

  deadtime_duty_comp_ = (deadtime_rising_ / (1 / frequency_)) * 100;
  if (deadtime_duty_comp_ > 100) {
    ESP_LOGE(TAG, "invalid deadtime duty comp %f", deadtime_duty_comp_);
    status_set_error();
    return;
  }

  mcpwm_set_duty(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_GEN_A, 0);
  mcpwm_set_duty(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_GEN_B, 100);
  mcpwm_start(mcpwmUnit_, MCPWM_TIMER_0);

  ESP_LOGI(TAG, "MCPWM setup complete");
}

void McpwmSyncComponent::dump_config() {
}

void McpwmSyncComponent::write_state(float state) {
  if (mcpwmUnit_ >= MCPWM_UNIT_MAX) {
    ESP_LOGE(TAG, "invalid mcpwm unit");
    return;
  }

  duty_ = state;
  if (duty_ < 0) {
    ESP_LOGE(TAG, "invalid duty: %f", duty_);
    duty_ = 0;
  } else if (duty_ > max_duty_) {
    ESP_LOGE(TAG, "duty clamped to max: %f", duty_);
    duty_ = max_duty_;
  }
  duty_ = duty_ * 100;  // format conversion for ESP API
  if (duty_ > 0) {  // compensate for rising deadtime which is rolled into the high duty cycle
    duty_ += deadtime_duty_comp_;
  }
  if (duty_ > 100 - deadtime_duty_comp_) {  // ensure it keeps pulsing
    duty_ = 100 - deadtime_duty_comp_;
  }
  mcpwm_set_duty(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_GEN_A, duty_);
}

float McpwmSyncComponent::get_state() { return duty_; }

void McpwmSyncComponent::loop() {
  if (blank_time_ms_ > 0 && esphome::millis() >= nextBlankTime_) {
    mcpwm_set_duty(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_GEN_A, 0);  // blank
    esphome::delay(blank_time_ms_);
    if (sample_adc_ != nullptr) {
      sample_adc_->update();
    }
    mcpwm_set_duty(mcpwmUnit_, MCPWM_TIMER_0, MCPWM_GEN_A, duty_);  // resume PWMing
    nextBlankTime_ += blank_period_ms_;
  }
}

uint8_t McpwmSyncComponent::nextMcpwmUnit_ = 0;

}