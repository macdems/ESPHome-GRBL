#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "grbl.h"

namespace esphome {
namespace grbl {

class GrblBinarySensor : public binary_sensor::BinarySensor, public Component, public Grbl::Listener {
 public:
  void set_parent(Grbl *parent) { parent_ = parent; }
  void set_setting(int setting) { setting_ = setting; }

  void update(int setting, float value) override;

 protected:
  Grbl *parent_{nullptr};
  int setting_{-1};
};

}} // namespace esphome::grbl
