/* Copyright (C) 2026 Maciek Dems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "esphome/core/automation.h"
#include "grbl.h"

namespace esphome { namespace grbl {

template <typename... Ts> class GrblCancelJogAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    void play(Ts... x) override { this->parent_->cancel_jog(); }
};

template <typename... Ts> class GrblSendResetAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    void play(Ts... x) override { this->parent_->send_reset(); }
};

template <typename... Ts> class GrblReleaseStateAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    void play(Ts... x) override { this->parent_->release_state(); }
};

template <typename... Ts> class GrblSendCommandAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    TEMPLATABLE_VALUE(std::string, command)

    void play(Ts... x) override {
        auto cmd = this->command_.value(x...);
        this->parent_->send_command(cmd);
    }

  protected:
    Grbl* grbl_;
};

template <typename... Ts> class GrblJogAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    TEMPLATABLE_VALUE(float, x)
    TEMPLATABLE_VALUE(float, y)
    TEMPLATABLE_VALUE(float, z)
    TEMPLATABLE_VALUE(int, speed)

    void play(Ts... x) override {
        float dx = this->x_.value(x...);
        float dy = this->y_.value(x...);
        float dz = this->z_.value(x...);
        int sp = this->speed_.value(x...);
        this->parent_->send_jog_command(dx, dy, dz, sp);
    }
};

template <typename... Ts> class GrblSetHomeAction : public Action<Ts...>, public Parented<Grbl> {
  public:
    TEMPLATABLE_VALUE(float, xy)
    TEMPLATABLE_VALUE(float, z)

    void play(Ts... x) override {
        float xy = this->xy_.value(x...);
        float z = this->z_.value(x...);
        this->parent_->set_home(xy, z);
    }
};

}}  // namespace esphome::grbl


#include "automation.h"
