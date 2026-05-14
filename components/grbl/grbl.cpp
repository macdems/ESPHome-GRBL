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

static const char* state_names[] = {
    "Idle", "Run", "Hold", "Jog", "Alarm", "Door", "Check", "Home", "Sleep", "Unknown",
};

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
    this->send_command("$$");        // Request GRBL settings update on startup
    this->update_status();           // Request GRBL status update on startup

    // We wait a bit more before starting the server to give GRBL time to reset and be ready to accept commands
    this->set_timeout(500, [this]() { this->server_.begin(); });
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

void Grbl::update_status_sensors_() {
    if (this->state_text_sensor_ != nullptr && this->status_.state != STATE_UNKNOWN) {
        std::string state_str = state_names[this->status_.state];
        state_str[0] += 32;  // convert to lowercase for consistency in Home Assistant
        if (state_str != this->state_text_sensor_->state) this->state_text_sensor_->publish_state(state_str);
    }
    for (int i = 0; i < 3; i++) {
        if (this->mpos_sensors_[i] != nullptr && !std::isnan(this->status_.mpos[i])) {
            if (this->mpos_sensors_[i]->state != this->status_.mpos[i])
                this->mpos_sensors_[i]->publish_state(this->status_.mpos[i]);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (this->wpos_sensors_[i] != nullptr && !std::isnan(this->status_.wpos[i])) {
            if (this->wpos_sensors_[i]->state != this->status_.wpos[i])
                this->wpos_sensors_[i]->publish_state(this->status_.wpos[i]);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (this->wco_sensors_[i] != nullptr && !std::isnan(this->status_.wco[i])) {
            if (this->wco_sensors_[i]->state != this->status_.wco[i]) this->wco_sensors_[i]->publish_state(this->status_.wco[i]);
        }
    }
    if (this->fs_feed_sensor_ != nullptr && this->status_.fs_feed >= 0) {
        if (this->fs_feed_sensor_->state != this->status_.fs_feed) this->fs_feed_sensor_->publish_state(this->status_.fs_feed);
    }
    if (this->fs_speed_sensor_ != nullptr && this->status_.fs_speed >= 0) {
        if (this->fs_speed_sensor_->state != this->status_.fs_speed) this->fs_speed_sensor_->publish_state(this->status_.fs_speed);
    }
    for (int i = 0; i < 3; i++) {
        if (this->limit_sensors_[i] != nullptr) {
            if (this->limit_sensors_[i]->state != (this->status_.limits >> i) & 1)
                this->limit_sensors_[i]->publish_state((this->status_.limits >> i) & 1);
        }
    }
    if (this->probe_sensor_ != nullptr) {
        if (this->probe_sensor_->state != (this->status_.limits & 8)) this->probe_sensor_->publish_state(this->status_.limits & 8);
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

void Grbl::update_status() {
    this->write_byte('?');  // Request GRBL status update
}

std::string Grbl::Status::str() const {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "State: %s, MPos: (%.3f, %.3f, %.3f), WPos: (%.3f, %.3f, %.3f), WCO: (%.3f, %.3f, %.3f), FS: (%d, %d), "
             "Limits: (%s%s%s%s)",
             state_names[this->state], this->mpos[0], this->mpos[1], this->mpos[2], this->wpos[0], this->wpos[1], this->wpos[2],
             this->wco[0], this->wco[1], this->wco[2], this->fs_feed, this->fs_speed, this->limits & 1 ? "X" : "",
             this->limits & 2 ? "Y" : "", this->limits & 4 ? "Z" : "", this->limits & 8 ? "P" : "");
    return std::string(buf);
}

void Grbl::parse_grbl_response_(const std::string& line) {
    int consumed = 0;

    int setting_id = 0;
    float setting_value = 0.0f;

    if (line[0] == '<' && line[line.size() - 2] == '>') {
        // Status report line, e.g. "<Idle|MPos:5.000,10.000,0.000|FS:0,0>"
        // We could parse this to get the current position and state of the machine, but for now we just log it.
        ESP_LOGV(TAG, "Received GRBL status report: %s", line.c_str());
        size_t start = 1;  // Skip the initial '<'
        while (start < line.size()) {
            size_t end = line.find('|', start);
            if (end == std::string::npos) end = line.size() - 1;  // Skip the final '>'
            const std::string part = line.substr(start, end - start);
            ESP_LOGVV(TAG, "Status part: %s", part.c_str());
            if (start == 1) {
                // The first part of the status report is the state, e.g. "Idle", "Run", "Hold", etc.
                for (size_t i = 0; i < 9; i++) {
                    if (part == state_names[i]) {
                        this->status_.state = static_cast<State>(i);
                        break;
                    }
                }
            } else if (!part.empty()) {
                bool has_mpos = false;
                bool has_wpos = false;
                if (part.compare(0, 5, "MPos:") == 0) {
                    sscanf(part.c_str(), "MPos:%f,%f,%f", &this->status_.mpos[0], &this->status_.mpos[1], &this->status_.mpos[2]);
                    has_mpos = true;
                } else if (part.compare(0, 5, "WPos:") == 0) {
                    sscanf(part.c_str(), "WPos:%f,%f,%f", &this->status_.wpos[0], &this->status_.wpos[1], &this->status_.wpos[2]);
                    has_wpos = true;
                } else if (part.compare(0, 4, "WCO:") == 0) {
                    sscanf(part.c_str(), "WCO:%f,%f,%f", &this->status_.wco[0], &this->status_.wco[1], &this->status_.wco[2]);
                } else if (part.compare(0, 3, "FS:") == 0) {
                    sscanf(part.c_str(), "FS:%d,%d", &this->status_.fs_feed, &this->status_.fs_speed);
                } else if (part.compare(0, 2, "F:") == 0) {
                    sscanf(part.c_str(), "F:%d", &this->status_.fs_feed);
                } else if (part.compare(0, 3, "Pn:") == 0) {
                    this->status_.limits = 0;
                    for (int i = 0; i < 3; i++) {
                        this->status_.limits |= (part.find('X' + i, 3) != std::string::npos) << i;
                    }
                    this->status_.limits |= part.find('P', 3) != std::string::npos << 3;
                }
                // Update MPos/WPos
                if (has_mpos) {
                    for (int i = 0; i < 3; i++) {
                        this->status_.wpos[i] = this->status_.mpos[i] - this->status_.wco[i];
                    }
                } else if (has_wpos) {
                    for (int i = 0; i < 3; i++) {
                        this->status_.mpos[i] = this->status_.wpos[i] + this->status_.wco[i];
                    }
                }
            }
            start = end + 1;
        }
        ESP_LOGD(TAG, "Got GRBL status: %s", this->status_.str().c_str());
        this->update_status_sensors_();

    } else if (sscanf(line.c_str(), "$%d=%f%n", &setting_id, &setting_value, &consumed) == 2 &&
               consumed == static_cast<int>(line.size()) - 1) {
        // This is a GRBL setting line, e.g. "$100=250.000"
        this->grbl_settings_[setting_id] = setting_value;
        ESP_LOGV(TAG, "Parsed GRBL setting: $%d=%f", setting_id, setting_value);
        for (auto* listener : this->listeners_) {
            listener->update(setting_id, setting_value);
        }
    }
}

std::string Grbl::set_coords_command_(Coords coords) {
    switch (coords) {
        case COORDS_G92:
            return "G92";
        default:
            std::string result = "G10 L20 P1";
            result[result.size() - 1] += static_cast<char>(coords);
            return result;
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

void Grbl::set_home(bool xy, bool z, Coords coords) {
    if (!xy && !z) return;  // Nothing to set as home
    std::string cmd = "G90\n";
    cmd += this->set_coords_command_(coords);
    if (xy) cmd += " X0 Y0";
    if (z) cmd += " Z0";
    this->send_command(cmd);
    this->set_timeout(
        100, [this]() { this->update_status(); });  // Update status after a short delay to get the new position after setting home
}

void Grbl::run_homing_cycle() {
    this->send_command("$H");  // GRBL command to run homing cycle
}

void Grbl::probe_z(float distance, float seek_rate, float feed_rate, float offset, float retract, Coords coords) {
    char cmd[256];
    if (distance < 0) {
        float z = this->status_.mpos[2];
        auto max_z = this->get_grbl_setting<float>(132);
        if (max_z && !std::isnan(z)) {
            distance = *max_z + z - 0.001;  // z is negative
        } else {
            ESP_LOGW(TAG, "Cannot probe Z because current Z position is unknown");
            return;
        }
        ESP_LOGD(TAG, "Calculated probe distance: %.3f (current Z: %.3f, max Z: %.3f)", distance, z, *max_z);
    }
    std::string cords_cmd = this->set_coords_command_(coords);
    snprintf(cmd, sizeof(cmd),
             "G21 G91\n"
             "G38.2 Z-%.3f F%.1f\n"
             "G0 Z1\n"
             "G38.2 Z-2 F%.3f\n"
             "%s Z%.3f\n"
             "G91 G0 Z%.3f\n"
             "G90",
             distance, seek_rate, feed_rate, cords_cmd.c_str(), offset, retract);
    send_command(cmd);
}

}}  // namespace esphome::grbl
