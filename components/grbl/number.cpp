#include "number.h"

namespace esphome { namespace grbl {

static const char* TAG = "grbl_number";

void GrblNumber::control(float value) {
    if (parent_ == nullptr) return;
    parent_->set_grbl_setting<float>(setting_, value);
    this->publish_state(value);
}

void GrblNumber::update(int setting, float value) {
    if (setting == setting_) {
        publish_state(value);
    }
}

}}  // namespace esphome::grbl
