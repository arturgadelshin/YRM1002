#include "yrm1002.h"
#include "esphome/core/log.h"

namespace esphome {
namespace yrm1002 {

static const char *const TAG = "yrm1002";

void YRM1002::setup() {
  ESP_LOGCONFIG(TAG, "Setting up YRM1002...");
  delay(100);
  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
  }

  this->send_command_(CMD_SET_REGION, {REGION_EU});
  delay(300);

  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
  }

  this->setup_done_ = true;
  ESP_LOGCONFIG(TAG, "YRM1002 setup complete");
}

void YRM1002::update() {
  if (!this->setup_done_)
    return;

  for (auto *sensor : this->binary_sensors_)
    sensor->on_scan_end();

  prev_tags_ = current_tags_;
  current_tags_.clear();
  this->rx_buffer_.clear();

  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
  }

  this->send_command_(CMD_READ_MULTI, {0x22, 0x00, this->repeat_count_});
  this->scanning_ = true;
  this->scan_start_time_ = millis();
}

void YRM1002::loop() {
  if (!this->scanning_) {
    while (this->available()) {
      uint8_t b;
      this->read_byte(&b);
    }
    return;
  }

  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
    this->rx_buffer_.push_back(b);
  }

  this->parse_frames_();

  if (millis() - this->scan_start_time_ > this->scan_duration_) {
    this->send_command_(CMD_STOP_MULTI);
    this->scanning_ = false;

    uint32_t t = millis();
    while (millis() - t < 100) {
      while (this->available()) {
        uint8_t b;
        this->read_byte(&b);
        this->rx_buffer_.push_back(b);
      }
      this->parse_frames_();
      delay(10);
    }
    this->rx_buffer_.clear();

    ESP_LOGD(TAG, "Scan: %zu tags", this->current_tags_.size());

    if (this->tag_present_sensor_ != nullptr) {
      this->tag_present_sensor_->publish_state(!this->current_tags_.empty());
    }

    for (auto it = this->prev_tags_.begin(); it != this->prev_tags_.end(); ++it) {
      if (this->current_tags_.find(*it) == this->current_tags_.end()) {
        for (auto *trigger : this->on_tag_removed_triggers_) {
          trigger->trigger(*it);
        }
      }
    }
  }
}

void YRM1002::dump_config() {
  ESP_LOGCONFIG(TAG, "YRM1002 UHF RFID Reader:");
  ESP_LOGCONFIG(TAG, "  Scan duration: %" PRIu32 "ms", this->scan_duration_);
  ESP_LOGCONFIG(TAG, "  Repeat count: %u", this->repeat_count_);
  LOG_UPDATE_INTERVAL(this);
  for (auto *sensor : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", sensor);
  }
}

uint8_t YRM1002::calc_checksum_(const uint8_t *data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return (uint8_t)(sum & 0xFF);
}

void YRM1002::send_command_(uint8_t cmd, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame;
  frame.push_back(FRAME_BEGIN);

  uint8_t type = FRAME_TYPE_CMD;
  frame.push_back(type);
  frame.push_back(cmd);

  uint16_t length = data.size();
  frame.push_back((length >> 8) & 0xFF);
  frame.push_back(length & 0xFF);

  for (uint8_t b : data) {
    frame.push_back(b);
  }

  std::vector<uint8_t> checksum_data;
  checksum_data.push_back(type);
  checksum_data.push_back(cmd);
  checksum_data.push_back((length >> 8) & 0xFF);
  checksum_data.push_back(length & 0xFF);
  for (uint8_t b : data) {
    checksum_data.push_back(b);
  }
  frame.push_back(calc_checksum_(checksum_data.data(), checksum_data.size()));

  frame.push_back(FRAME_END);

  this->write_array(frame);
  this->flush();
  ESP_LOGD(TAG, "TX: %s", format_hex_pretty(frame).c_str());
}

void YRM1002::parse_frames_() {
  size_t i = 0;
  while (i < this->rx_buffer_.size()) {
    if (this->rx_buffer_[i] != FRAME_BEGIN) {
      i++;
      continue;
    }

    if (i + 5 >= this->rx_buffer_.size()) {
      break;
    }

    uint16_t data_len = ((uint16_t)this->rx_buffer_[i + 3] << 8) | this->rx_buffer_[i + 4];
    size_t frame_len = 1 + 1 + 1 + 2 + data_len + 1 + 1;

    if (i + frame_len > this->rx_buffer_.size()) {
      break;
    }

    if (this->rx_buffer_[i + frame_len - 1] != FRAME_END) {
      i++;
      continue;
    }

    std::vector<uint8_t> frame(this->rx_buffer_.begin() + i, this->rx_buffer_.begin() + i + frame_len);

    uint8_t checksum = calc_checksum_(&frame[1], frame.size() - 3);
    if (checksum != frame[frame.size() - 2]) {
      i++;
      continue;
    }

    uint8_t msg_type = frame[1];
    uint8_t cmd = frame[2];
    uint16_t payload_len = ((uint16_t)frame[3] << 8) | frame[4];
    const uint8_t *payload = &frame[5];

    if (cmd == CMD_INVENTORY && (msg_type == FRAME_TYPE_ANS || msg_type == FRAME_TYPE_INFO)) {
      this->handle_inventory_(payload, payload_len);
    } else if (cmd == CMD_EXE_FAILED) {
      ESP_LOGV(TAG, "Cmd failed 0x%02X", payload_len > 0 ? payload[0] : 0);
    }

    i += frame_len;
  }

  if (i > 0) {
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + i);
  }
}

void YRM1002::handle_inventory_(const uint8_t *payload, size_t len) {
  if (len < 4) {
    return;
  }

  uint8_t rssi = payload[0];
  uint16_t pc = ((uint16_t)payload[1] << 8) | payload[2];

  size_t epc_len = len - 4;
  if (epc_len < 2) {
    return;
  }

  std::string epc;
  char buf[3];
  for (size_t i = 3; i < len - 2; i++) {
    snprintf(buf, sizeof(buf), "%02X", payload[i]);
    epc += buf;
  }

  uint16_t crc = ((uint16_t)payload[len - 2] << 8) | payload[len - 1];

  current_tags_.insert(epc);

  TagInfo tag_info;
  tag_info.epc = epc;
  tag_info.rssi = rssi;
  tag_info.pc = pc;
  tag_info.crc = crc;

  bool is_new = prev_tags_.find(epc) == prev_tags_.end();

  for (auto *sensor : this->binary_sensors_) {
    sensor->process(epc);
  }

  if (this->text_sensor_ != nullptr) {
    this->text_sensor_->publish_state(epc);
  }

  if (is_new) {
    ESP_LOGD(TAG, "TAG: %s RSSI=%d", epc.c_str(), rssi);
    for (auto *trigger : this->on_tag_triggers_) {
      trigger->trigger(epc, tag_info);
    }
  }
}

}  // namespace yrm1002
}  // namespace esphome
