#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "grbl.h"

namespace esphome {
namespace grbl {

class GrblNumber : public number::Number, public Component, public Grbl::Listener {
 public:
  void set_parent(Grbl *parent) { parent_ = parent; }
  void set_setting(int setting) { setting_ = setting; }

  void control(float value) override;
  void update(int setting, float value) override;

 protected:
  Grbl *parent_{nullptr};
  int setting_{-1};
};

}} // namespace esphome::grbl
