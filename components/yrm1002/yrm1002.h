#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cstdint>
#include <string>
#include <vector>
#include <set>

namespace esphome {
namespace yrm1002 {

static const uint8_t FRAME_BEGIN = 0xBB;
static const uint8_t FRAME_END = 0x7E;

static const uint8_t FRAME_TYPE_CMD = 0x00;
static const uint8_t FRAME_TYPE_ANS = 0x01;
static const uint8_t FRAME_TYPE_INFO = 0x02;

static const uint8_t CMD_SET_REGION = 0x07;
static const uint8_t CMD_INVENTORY = 0x22;
static const uint8_t CMD_READ_MULTI = 0x27;
static const uint8_t CMD_STOP_MULTI = 0x28;
static const uint8_t CMD_EXE_FAILED = 0xFF;

static const uint8_t REGION_EU = 0x03;

struct TagInfo {
  std::string epc;
  uint8_t rssi;
  uint16_t pc;
  uint16_t crc;
};

class YRM1002BinarySensor;

class YRM1002TagTrigger : public Trigger<std::string, TagInfo> {};
class YRM1002TagRemovedTrigger : public Trigger<std::string> {};

class YRM1002 : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void set_scan_duration(uint32_t duration) { scan_duration_ = duration; }
  void set_repeat_count(uint8_t count) { repeat_count_ = count; }
  void set_tag_present_sensor(binary_sensor::BinarySensor *sensor) { tag_present_sensor_ = sensor; }
  void register_tag(YRM1002BinarySensor *sensor) { binary_sensors_.push_back(sensor); }
  void register_ontag_trigger(YRM1002TagTrigger *trigger) { on_tag_triggers_.push_back(trigger); }
  void register_ontagremoved_trigger(YRM1002TagRemovedTrigger *trigger) {
    on_tag_removed_triggers_.push_back(trigger);
  }
  void set_text_sensor(text_sensor::TextSensor *sensor) { text_sensor_ = sensor; }

 protected:
  void send_command_(uint8_t cmd, const std::vector<uint8_t> &data = {});
  uint8_t calc_checksum_(const uint8_t *data, size_t len);
  void parse_frames_();
  void handle_inventory_(const uint8_t *payload, size_t len);

  std::vector<YRM1002BinarySensor *> binary_sensors_;
  std::vector<YRM1002TagTrigger *> on_tag_triggers_;
  std::vector<YRM1002TagRemovedTrigger *> on_tag_removed_triggers_;
  text_sensor::TextSensor *text_sensor_{nullptr};
  binary_sensor::BinarySensor *tag_present_sensor_{nullptr};

  std::vector<uint8_t> rx_buffer_;
  std::set<std::string> prev_tags_;
  std::set<std::string> current_tags_;
  bool scanning_{false};
  uint32_t scan_duration_;
  uint8_t repeat_count_{10};
  uint32_t scan_start_time_{0};
  bool setup_done_{false};
};

class YRM1002BinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const std::string &uid) { uid_ = uid; }
  bool process(const std::string &epc) {
    if (epc == uid_) {
      this->publish_state(true);
      found_ = true;
      return true;
    }
    return false;
  }
  void on_scan_end() {
    if (!found_) {
      this->publish_state(false);
    }
    found_ = false;
  }

 protected:
  std::string uid_;
  bool found_{false};
};

}  // namespace yrm1002
}  // namespace esphome
