#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "grbl.h"

namespace esphome {
namespace grbl {

class GrblSwitch : public switch_::Switch, public Component, public Grbl::Listener {
 public:
  void set_parent(Grbl *parent) { parent_ = parent; }
  void set_setting(int setting) { setting_ = setting; }

  void write_state(bool state) override;
  void update(int setting, float value) override;

 protected:
  Grbl *parent_{nullptr};
  int setting_{-1};
};

}} // namespace esphome::grbl
