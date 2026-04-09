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

#include "grbl.h"

#include "esphome/core/log.h"
#include "esphome/core/util.h"

static const char* TAG = "GRBL";

namespace esphome { namespace grbl {

Grbl::Client::Client(AsyncClient* client, std::vector<uint8_t>& recv_buf)
    : tcp_client{client}, identifier{this->format_ip()}, connected{true} {
    ESP_LOGD(TAG, "New client connected from %s", this->identifier.c_str());

    this->tcp_client->onError([this](void* h, AsyncClient* client, int8_t error) {
        ESP_LOGE(TAG, "Client %s error: %d", this->identifier.c_str(), error);
        this->connected = false;
        delete this->tcp_client;
        this->tcp_client = nullptr;
    });
    this->tcp_client->onDisconnect([this](void* h, AsyncClient* client) {
        ESP_LOGD(TAG, "Client %s disconnected", this->identifier.c_str());
        this->connected = false;
        delete this->tcp_client;
        this->tcp_client = nullptr;
    });

    this->tcp_client->onTimeout([this](void* h, AsyncClient* client, uint32_t time) {
        ESP_LOGW(TAG, "Client %s timeout", this->identifier.c_str());
        this->connected = false;
        delete this->tcp_client;
        this->tcp_client = nullptr;
    });

    this->tcp_client->onData(
        [&](void* h, AsyncClient* client, void* data, size_t len) {
            if (len == 0 || data == nullptr) return;

            auto buf = static_cast<uint8_t*>(data);
            recv_buf.insert(recv_buf.end(), buf, buf + len);
        },
        nullptr);
}

Grbl::Client::~Client() { delete this->tcp_client; }

void Grbl::setup() {
    ESP_LOGCONFIG(TAG, "Setting up GRBL...");
    this->recv_buf_.reserve(128);

    this->server_ = AsyncServer(this->port_);
    this->server_.onClient(
        [this](void* h, AsyncClient* tcp_client) {
            if (tcp_client == nullptr) return;

            if (this->client_connected()) {
                ESP_LOGD(TAG, "Not accepting new connection from %s, only one client allowed", this->client_->format_ip().c_str());
                // We send proper GRBL response to indicate that the client is connected but busy, instead of just closing
                // the connection without explanation.
                // GRBL doesn't have a specific error code for "already connected", so we use code 8 ("Not idle") to indicate
                // that the machine is currently busy with another client. However, I wonder whether code 9 ("G-code lockout")
                // wouldn't be more appropriate here.
                tcp_client->write("error:8 (another connection in progress)\n");
                tcp_client->send();
                tcp_client->close();
                return;
            }

            this->client_ = std::make_unique<Client>(tcp_client, this->recv_buf_);
        },
        this);

    this->send_reset();
    this->send_command("S0\nM5\n");  // Ensure spindle/laser is off
    this->send_command("$$");  // Request GRBL settings update on startup

    // We wait a bit more before starting the server to give GRBL time to reset and be ready to accept commands
    this->set_timeout(500, [this]() {
        this->server_.begin();
    });
}

void Grbl::loop() {
    this->cleanup_();
    this->update_connection_sensor_();
    this->serial_read_();
    this->serial_write_();
}

void Grbl::update_connection_sensor_() {
    if (this->connection_sensor_ != nullptr) {
        this->connection_sensor_->publish_state(this->client_connected());
    }
}

void Grbl::cleanup_() {
    if (this->client_ && this->client_->tcp_client == nullptr) this->client_.reset();
}

void Grbl::serial_read_() {
    int len;
    while ((len = this->available()) > 0) {
        uint8_t buf[128];
        len = std::min(len, 128);
        if (!this->read_array(buf, len)) {
            ESP_LOGE(TAG, "Error reading from UART");
            return;
        }

        // Parse lines to get useful information about the state of the machine and report it via sensors
        for (size_t i = 0, ls = 0; i <= len; i++) {
            if (i == len || buf[i] == '\n') {
                this->line_buffer_.append(reinterpret_cast<const char*>(buf + ls), i - ls);
                if (buf[i] == '\n') {
                    this->parse_grbl_response_(this->line_buffer_);
                    this->line_buffer_.clear();
                }
                ls = i + 1;
            }
        }

        if (this->client_connected()) {
            this->client_->tcp_client->write(reinterpret_cast<const char*>(buf), len);
        }
    }
}

void Grbl::serial_write_() {
    size_t len;
    while ((len = this->recv_buf_.size()) > 0) {
        this->write_array(this->recv_buf_.data(), len);
        this->recv_buf_.erase(this->recv_buf_.begin(), this->recv_buf_.begin() + len);
    }
}

void Grbl::dump_config() {
    ESP_LOGCONFIG(TAG, "GRBL:");
    ESP_LOGCONFIG(TAG, "  TCP Port: %u", this->port_);
    ESP_LOGCONFIG(TAG, "  Commands when client connected: %s", YESNO(this->allow_commands_when_connected_));
    ESP_LOGCONFIG(TAG, "  GRBL Settings:");
    for (auto setting : this->grbl_settings_) {
        ESP_LOGCONFIG(TAG, "    $%d=%g", setting.first, setting.second);
    }
}

void Grbl::on_shutdown() {
    if (this->client_ && this->client_->tcp_client) {
        ESP_LOGD(TAG, "Shutting down GRBL, closing client connection");
        this->client_->tcp_client->close(true);
    }
}

void Grbl::parse_grbl_response_(const std::string& line) {
    // Expected format: $<number>=<int_or_float> (e.g. $32=0, $110=500.0)
    int setting_id = 0;
    float setting_value = 0.0f;
    int consumed = 0;

    if (sscanf(line.c_str(), "$%d=%f%n", &setting_id, &setting_value, &consumed) == 2 &&
        consumed == static_cast<int>(line.size()) - 1) {
        this->grbl_settings_[setting_id] = setting_value;
        ESP_LOGV(TAG, "Parsed GRBL setting: $%d=%f", setting_id, setting_value);
        for (auto* listener : this->listeners_) {
            listener->update(setting_id, setting_value);
        }
    }
}

void Grbl::send_command(const std::string& command) {
    if (this->client_connected() && !this->allow_commands_when_connected_) {
        ESP_LOGW(TAG, "Not sending command '%s' because a client is connected", command.c_str());
        return;
    }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    std::string log_command = command;
    size_t i;
    while ((i = log_command.find('\n')) != std::string::npos) {
        ESP_LOGD(TAG, "Sending GRBL command: %s", log_command.substr(0, i).c_str());
        log_command.erase(0, i + 1);
    }
    if (!log_command.empty()) ESP_LOGD(TAG, "Sending GRBL command: %s", log_command.c_str());
#endif
    this->write_str(command.c_str());
    if (command.back() != '\n') this->write_byte('\n');  // Ensure command is terminated with newline
}

void Grbl::send_jog_command(float dist_x, float dist_y, float dist_z, int speed) {
    std::string cmd = "$J=G91";

    char buf[16];
    if (dist_x != 0) {
        snprintf(buf, sizeof(buf), " X%.3f", dist_x);
        cmd += buf;
    }
    if (dist_y != 0) {
        snprintf(buf, sizeof(buf), " Y%.3f", dist_y);
        cmd += buf;
    }
    if (dist_z != 0) {
        snprintf(buf, sizeof(buf), " Z%.3f", dist_z);
        cmd += buf;
    }
    snprintf(buf, sizeof(buf), " F%d\n", speed);
    cmd += buf;
    this->send_command(cmd);
}

void Grbl::send_reset() {
    ESP_LOGD(TAG, "Sending GRBL command: ^X");
    this->write_byte(0x18);  // Send Ctrl-X to reset GRBL
}

void Grbl::cancel_jog() {
    ESP_LOGD(TAG, "Sending GRBL command: ^U");
    this->write_byte(0x85);  // Send Ctrl-U to cancel jogging in GRBL
}

void Grbl::release_state() {
    this->send_command("$X\n");  // GRBL command to release from alarm state, allowing new commands to be accepted
}

void Grbl::set_home(bool xy, bool z) {
    if (!xy && !z) return;  // Nothing to set as home
    std::string cmd = "G92";
    if (xy) cmd += " X0 Y0";
    if (z) cmd += " Z0";
    this->send_command(cmd.c_str());
}

}}  // namespace esphome::grbl
