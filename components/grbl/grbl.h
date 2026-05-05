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
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
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

    void register_state_text_sensor(text_sensor::TextSensor* sensor) { this->state_text_sensor_ = sensor; }
    void register_mpos_sensor(const char* axis, sensor::Sensor* sensor) {
        assert(*axis >= 'X' && *axis <= 'Z');
        this->mpos_sensors_[*axis - 'X'] = sensor;
    }
    void register_wpos_sensor(const char* axis, sensor::Sensor* sensor) {
        assert(*axis >= 'X' && *axis <= 'Z');
        this->wpos_sensors_[*axis - 'X'] = sensor;
    }
    void register_wco_sensor(const char* axis, sensor::Sensor* sensor) {
        assert(*axis >= 'X' && *axis <= 'Z');
        this->wco_sensors_[*axis - 'X'] = sensor;
    }
    void register_fs_feed_sensor(sensor::Sensor* sensor) { this->fs_feed_sensor_ = sensor; }
    void register_fs_speed_sensor(sensor::Sensor* sensor) { this->fs_speed_sensor_ = sensor; }
    void register_limit_sensor(const char* axis, binary_sensor::BinarySensor* sensor) {
        assert(*axis >= 'X' && *axis <= 'Z');
        this->limit_sensors_[*axis - 'X'] = sensor;
    }
    void register_probe_sensor(binary_sensor::BinarySensor* sensor) { this->probe_sensor_ = sensor; }

    bool client_connected() const { return this->client_ && this->client_->connected; }

    void send_command(const std::string& command);
    void send_jog_command(float dist_x = 0, float dist_y = 0, float dist_z = 0, int speed = 1000);
    void cancel_jog();
    void send_reset();
    void release_state();
    void set_home(bool xy = true, bool z = true);
    void run_homing_cycle();
    void probe_z(float distance = 30.0, float seek_rate = 100.0, float feed_rate = 10.0, float offset = 0.0, float retract = 5.0);

    void update_status();

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

    void register_listener(Listener* listener) { this->listeners_.push_back(listener); }

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

    enum State {
        STATE_IDLE = 0,
        STATE_RUN = 1,
        STATE_HOLD = 2,
        STATE_JOG = 3,
        STATE_ALARM = 4,
        STATE_DOOR = 5,
        STATE_CHECK = 6,
        STATE_HOME = 7,
        STATE_SLEEP = 8,
        STATE_UNKNOWN = 9
    };

    text_sensor::TextSensor* state_text_sensor_{nullptr};

    binary_sensor::BinarySensor* limit_sensors_[3]{nullptr, nullptr, nullptr};
    binary_sensor::BinarySensor* probe_sensor_{nullptr};

    sensor::Sensor* mpos_sensors_[3]{nullptr, nullptr, nullptr};
    sensor::Sensor* wpos_sensors_[3]{nullptr, nullptr, nullptr};
    sensor::Sensor* wco_sensors_[3]{nullptr, nullptr, nullptr};
    sensor::Sensor* fs_feed_sensor_{nullptr};
    sensor::Sensor* fs_speed_sensor_{nullptr};

    struct Status {
        State state = STATE_UNKNOWN;
        float mpos[3] = {NAN, NAN, NAN};
        float wpos[3] = {NAN, NAN, NAN};
        float wco[3] = {NAN, NAN, NAN};
        int fs_feed = -1;
        int fs_speed = -1;
        uint8_t limits = 0;
        std::string str() const;
    } status_{};

    std::string line_buffer_{};
    std::map<int, float> grbl_settings_{};

    std::vector<Listener*> listeners_{};

    void cleanup_();

    void serial_read_();
    void serial_write_();
    void update_connection_sensor_();

    void update_status_sensors_();

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
