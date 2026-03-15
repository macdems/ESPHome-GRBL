#include "switch.h"

namespace esphome { namespace grbl {

void GrblSwitch::write_state(bool state) {
    if (parent_ == nullptr) return;
    parent_->set_grbl_setting<bool>(setting_, state);
    this->publish_state(state);
}

void GrblSwitch::update(int setting, float value) {
    if (setting == setting_) {
        this->publish_state(value != 0.0f);
    }
}

}}  // namespace esphome::grbl
