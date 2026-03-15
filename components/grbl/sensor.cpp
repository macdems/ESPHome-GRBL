#include "sensor.h"

namespace esphome {
namespace grbl {

void GrblSensor::update(int setting, float value) {
    if (setting == setting_) {
        publish_state(value);
    }
}

}}  // namespace esphome::grbl
