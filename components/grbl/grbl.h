/* Copyright (C) 2026 Maciek Dems
 *
 * TCP-UART bridge based on serial server by github.com/thegroove
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

#include <map>
#include <memory>
#include <optional>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#ifdef ARDUINO_ARCH_ESP8266
#    include <ESPAsyncTCP.h>
#else
#    include <AsyncTCP.h>
#endif

namespace esphome { namespace grbl {

class Grbl : public uart::UARTDevice, public Component {
  public:
    void setup() override;
    void loop() override;
    void dump_config() override;
    void on_shutdown() override;

    float get_setup_priority() const override { return esphome::setup_priority::LATE; }

    void set_port(uint16_t port) { this->port_ = port; }
    void set_allow_commands_when_connected(bool allow) { this->allow_commands_when_connected_ = allow; }
    void register_connection_sensor(binary_sensor::BinarySensor* connection_sensor) {
        this->connection_sensor_ = connection_sensor;
    }

    bool client_connected() const { return this->client_ && this->client_->connected; }

    void send_command(const std::string& command);
    void send_jog_command(float dist_x = 0, float dist_y = 0, float dist_z = 0, int speed = 1000);
    void cancel_jog();
    void send_reset();
    void release_state();
    void set_home(bool xy = true, bool z = true);
    void run_homing_cycle();
    void probe_z(float distance = 30.0, float seek_rate = 100.0, float feed_rate = 10.0, float offset = 0.0, float retract = 5.0);

    template <typename T> std::optional<T> get_grbl_setting(int setting_number) {
        auto it = this->grbl_settings_.find(setting_number);
        if (it != this->grbl_settings_.end())
            return static_cast<T>(it->second);
        else
            return std::nullopt;
    }
    template <typename T> void set_grbl_setting(int setting_number, T value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "$%d=%d\n$$", setting_number, value);
        this->send_command(buf);
    }

    class Listener {
      public:
        virtual void update(int setting, float value) = 0;
        virtual ~Listener() = default;
    };

    void register_listener(Listener* listener) {
        this->listeners_.push_back(listener);
    }

  protected:
    AsyncServer server_{0};

    struct Client {
        Client(AsyncClient* client, std::vector<uint8_t>& recv_buf);
        ~Client();

        std::string format_ip() const {
            uint32_t ip_addr = tcp_client->getRemoteAddress();
            uint8_t* ip = reinterpret_cast<uint8_t*>(&ip_addr);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            return std::string(buf);
        }

        AsyncClient* tcp_client{nullptr};
        std::string identifier{};
        bool connected{true};
    };
    std::vector<uint8_t> recv_buf_{};
    uint16_t port_;
    binary_sensor::BinarySensor* connection_sensor_{nullptr};
    std::unique_ptr<Client> client_{nullptr};

    bool allow_commands_when_connected_;

    std::string line_buffer_{};
    std::map<int, float> grbl_settings_{};
    std::vector<Listener*> listeners_{};

    void cleanup_();

    void serial_read_();
    void serial_write_();
    void update_connection_sensor_();

    void parse_grbl_response_(const std::string& line);
};

template <> inline void Grbl::set_grbl_setting<float>(int setting_number, float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "$%d=%.3f\n$$", setting_number, value);
    this->send_command(buf);
}

template <> inline void Grbl::set_grbl_setting<bool>(int setting_number, bool value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "$%d=%d\n", setting_number, static_cast<int>(value));
    this->send_command(buf);
}

}}  // namespace esphome::grbl

#include "automation.h"
