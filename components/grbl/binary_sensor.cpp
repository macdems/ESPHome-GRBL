#include "binary_sensor.h"

namespace esphome { namespace grbl {

static const char* TAG = "grbl_binary_sensor";

void GrblBinarySensor::update(int setting, float value) {
    if (setting == setting_) {
        publish_state(value != 0.0f);
    }
}

}}  // namespace esphome::grbl
