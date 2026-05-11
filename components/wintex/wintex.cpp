#include "wintex.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace wintex {

static const char *TAG = "wintex";
static const int COMMAND_DELAY = 1000;

static std::vector<uint8_t> read_payload(uint32_t address, uint8_t length) {
  auto payload = std::vector<uint8_t>();
  payload.push_back((address >> 16) & 0xff);
  payload.push_back((address >> 8) & 0xff);
  payload.push_back((address) & 0xff);
  payload.push_back(length);
  return payload;
};
static std::vector<uint8_t> write_payload(uint32_t address, std::vector<uint8_t> data) {
  assert(data.size() <= 0xff);
  auto payload = read_payload(address, data.size() & 0xff);
  for (auto it = data.cbegin(); it != data.cend(); ++it) {
    payload.push_back(*it);
  }
  return payload;
};

void WintexZoneBypassSwitch::write_state(bool state) {
  uint32_t address = address_ + offset_;
  uint8_t data = (state ? 0xa0 : 0x80);
  auto payload = write_payload(address, {data});
  auto bypass = AsyncWintexCommand{
    .cmd = WintexCommandType::WRITE_VOLATILE, 
    .payload = payload, 
    .callback = callback_
  };
  this->commit_ = true;
  this->wintex_->queue_command_(bypass);
}

optional<AsyncWintexCommand> WintexZoneBypassSwitch::handle_response(WintexResponse response) {
  if (response.type != WintexResponseType::ACK) {
    ESP_LOGE(TAG, "Unexpected response to command: %d", (uint8_t) response.type);
  } else {
    if (commit_) {
      auto commit = AsyncWintexCommand{
        .cmd = WintexCommandType::COMMIT,
        .payload = {},
        .callback = this->callback_,
      };
      this->commit_= false;
      return commit;
    } else {
      // Acknowledge new state by publishing it
      publish_state(!state);
    }
  }
  return {};
}

void WintexZone::setup(Wintex *wintex, uint32_t zone_base_address, uint16_t zone_group_size, std::string zone_name) {
  if (setup_)
    return;
  setup_ = true;
  // Name was set at compile time by Python codegen; fall back to zone_name if empty
  std::string name = get_name().str();
  if (name.empty())
    name = zone_name;
  uint16_t zone_idx = this->zone_ - 1;
  // Each block holds zone_group_size zones; blocks are spaced 0x20 apart in memory.
  uint16_t block = zone_idx / zone_group_size;
  uint8_t block_offset = zone_idx % zone_group_size;
  uint32_t block_address = zone_base_address + block * 0x20;
  status = new WintexBinarySensor(block_address, zone_group_size, block_offset, 0x01);
  status->register_as_binary_sensor(name + " status", true);
  status->add_on_state_callback([this](bool state) {
    this->publish_state(state);
  });
  wintex->register_sensor(status);
  bypass = new WintexZoneBypassSwitch(wintex, block_address, zone_group_size, block_offset);
  bypass->register_as_switch(name + " bypassed");
  wintex->register_sensor(bypass);
  // Should only enable this once we are sorting the sensors by base address
  // faulty = new WintexBinarySensor(zone_base_address + 0x20, zone_group_size, zone_, 0x02);
  // faulty->register_as_binary_sensor(name + " faulty");
  // wintex->register_sensor(faulty);
}

void Wintex::setup() {
  last_command_timestamp_ = millis();
  // Step 1 of login: empty SESSION to get initial panel info.
  // On success, handle_login_() returns step 2 (UDL SESSION).
  this->login_ = AsyncWintexCommand{
    .cmd = WintexCommandType::SESSION,
    .payload = {},
    .callback = [this](WintexResponse response) {
        return this->handle_login_(response);
      },
    };
  this->queue_command_(this->login_);
}

optional<AsyncWintexCommand> Wintex::handle_login_(WintexResponse response) {
  if (response.type != WintexResponseType::SESSION || response.len == 0) {
    ESP_LOGW(TAG, "Login failed (type=0x%02X len=%u)", (uint8_t) response.type, response.len);
    return {};
  }

  // Check if the response is printable ASCII (authenticated product name)
  // vs binary panel info (initial handshake, UDL auth still needed).
  bool is_ascii = true;
  for (size_t i = 0; i < response.len; i++) {
    if (!std::isprint(response.data[i])) { is_ascii = false; break; }
  }

  if (!is_ascii) {
    // Step 1 response: binary panel info. Now send step 2 — UDL SESSION.
    // UDL digits must be sent as raw numeric values, not ASCII characters.
    std::vector<uint8_t> udl_payload;
    for (char c : udl_) {
      if (c >= '0' && c <= '9')
        udl_payload.push_back(c - '0');
    }
    ESP_LOGV(TAG, "Initial SESSION OK — sending UDL SESSION (%u digits)", udl_payload.size());
    return AsyncWintexCommand{
      .cmd = WintexCommandType::SESSION,
      .payload = udl_payload,
      .callback = [this](WintexResponse r) { return this->handle_login_(r); },
    };
  }

  // Step 2 response: ASCII product name — fully authenticated.
  product_ = std::string(reinterpret_cast<const char *>(response.data), response.len);
  ESP_LOGI(TAG, "Authenticated! Product: [%s]", product_.c_str());
  init_state_ = WintexInitState::AUTH;
  setup_zones_();
  this->update_sensors_();
  return {};
}

optional<AsyncWintexCommand> Wintex::handle_heartbeat_(WintexResponse response) {
  if (response.type == WintexResponseType::SESSION 
    && response.len == 0x09) {
      ESP_LOGV("Heartbeat response [%s]", format_hex_pretty(response.data, response.len).c_str());
  } else {
    ESP_LOGE(TAG, "Unexpected heartbeat response: %d", (uint8_t) response.type);
  }
  return {};
}

void Wintex::loop() {
  while (available()) {
    uint8_t c;
    read_byte(&c);
    handle_char_(c);
  }
  if (millis() - last_command_timestamp_ > 10000) {
    if (!rx_message_.empty()) {
      ESP_LOGW(TAG, "Command timeout — partial/stuck bytes in rx buffer: [%s]",
               format_hex_pretty(&rx_message_[0], rx_message_.size()).c_str());
    } else {
      ESP_LOGW(TAG, "Command timeout — rx buffer empty (panel sent nothing after ACK)");
    }
    ESP_LOGW(TAG, "Clearing queue and re-attempting login");
    this->current_command_ = {};
    this->command_queue_.clear();
    this->rx_message_.clear();
    last_command_timestamp_ = millis();
    queue_command_(login_);
  }
  process_command_queue_();
}

void Wintex::dump_config() {
  ESP_LOGCONFIG(TAG, "Wintex:");
  if (init_state_ != WintexInitState::INIT_DONE) {
    ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                  static_cast<uint8_t>(init_state_));
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Wintex device.");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Product: '%s'", product_.c_str());
}

optional<WintexResponse> Wintex::parse_response_() {
  if (rx_message_.empty())
    return {};

  size_t length = rx_message_[0];

  if (length < 3) {
    // Minimum valid message is 3 bytes (length + type + checksum).
    // A length of 0-2 means garbage/noise on the RX line — discard it.
    rx_message_.clear();
    return {};
  }

  if (rx_message_.size() < length)
    return {};

  ESP_LOGVV(TAG, "Received message DATA=[%s]", format_hex_pretty(&rx_message_[0], length).c_str());

  // validate checksum
  // Byte LEN: CHECKSUM - sum of all bytes (including header) ^ 0xFF
  uint8_t rx_checksum = rx_message_[length-1];
  uint8_t calc_checksum = 0;
  for (uint8_t i = 0; i < length - 1; i++)
    calc_checksum += rx_message_[i];
  calc_checksum ^= 0xFF;

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Wintex Received invalid message checksum DATA=[%s] Checksum: %02X!=%02X", 
      format_hex_pretty(&rx_message_[0], length).c_str(), rx_checksum, calc_checksum);
    this->rx_message_.clear();
    return {};
  }

  // valid message
  WintexResponseType type = (WintexResponseType) rx_message_[1];
  WintexResponse response = WintexResponse{
    .type = type,
    .len = length - 3,
    .data = &rx_message_[2]
  };
  ESP_LOGV(TAG, "Received Response: type=0x%02X DATA=[%s]", static_cast<uint8_t>(response.type),
           format_hex_pretty(response.data, response.len).c_str());
  return response;
}

void Wintex::handle_char_(uint8_t c) {
  rx_message_.push_back(c);
  optional<WintexResponse> response = this->parse_response_();
  if (response.has_value() && this->current_command_.has_value()) {
    // The panel sends an ACK (0x06) before the actual SESSION response.
    // Ignore it and keep current_command_ alive so we catch the real response.
    if (response.value().type == WintexResponseType::ACK
        && this->current_command_->cmd == WintexCommandType::SESSION) {
      ESP_LOGV(TAG, "ACK received for SESSION command — waiting for SESSION response");
      rx_message_.clear();
      return;
    }
    std::function<optional<AsyncWintexCommand>(WintexResponse)> callback = this->current_command_->callback;
    this->current_command_ = {};
    optional<AsyncWintexCommand> command = callback(response.value());
    rx_message_.clear();
    if (command.has_value()) {
      this->send_command_now_(command.value());
    }
  }
}

void Wintex::update_sensors_() {
  this->current_sensor_ = 0;
  auto sensor = sensors_[current_sensor_];
  auto read_sensor = AsyncWintexCommand{
    .cmd = WintexCommandType::READ_VOLATILE,
    .payload = read_payload(sensor->get_address(), sensor->get_length()),
    .callback = [this](WintexResponse response) {
      return this->handle_sensors_(response);
    }
  };
  queue_command_(read_sensor);
}

optional<AsyncWintexCommand> Wintex::handle_sensors_(WintexResponse response) {
  if (response.type != WintexResponseType::READ_VOLATILE || response.len < 4) {
    ESP_LOGW(TAG, "Unexpected sensor response: type=0x%02X len=%u — advancing to next sensor",
             (uint8_t) response.type, response.len);
    current_sensor_ = (current_sensor_ + 1) % std::max((size_t)1, sensors_.size());
    auto sensor = sensors_[current_sensor_];
    return AsyncWintexCommand{
      .cmd = WintexCommandType::READ_VOLATILE,
      .payload = read_payload(sensor->get_address(), sensor->get_length()),
      .callback = [this](WintexResponse r) { return this->handle_sensors_(r); }
    };
  }
  uint32_t address = (response.data[0] << 16) | (response.data[1] << 8) | response.data[2];
  uint8_t length = response.data[3];
  const uint8_t *data = &response.data[4];
  if (sensors_.size() == 0) {
    ESP_LOGV(TAG, "No sensors to update");
    return {};
  }
  while (current_sensor_ < sensors_.size()) {
    auto sensor = sensors_[current_sensor_];
    if (address == sensor->get_address() && length == sensor->get_length()) {
      sensor->update_state(data);
      current_sensor_++;
    } else {
      ESP_LOGV(TAG, "Changing requested address at sensor %d", current_sensor_);
      break;
    }
  }
  current_sensor_ = current_sensor_ % sensors_.size();
/*
  if (current_sensor_  == sensors_.size()) {
    current_sensor_ = 0;
    return;
  }
*/
  auto sensor = sensors_[current_sensor_];
  auto read_sensor = AsyncWintexCommand{
    .cmd = WintexCommandType::READ_VOLATILE,
    .payload = read_payload(sensor->get_address(), sensor->get_length()),
    .callback = [this](WintexResponse response) {
      return this->handle_sensors_(response);
    }
  };
  queue_command_(read_sensor);
  return {};
}

void Wintex::send_command_now_(AsyncWintexCommand command) {
  last_command_timestamp_ = millis();
  if (this->current_command_.has_value()) {
    ESP_LOGE(TAG, "Sending command while another is in progress, this may not end well!!");
  }
  this->current_command_ = command;

  uint8_t len = (uint8_t)(command.payload.size()) + 3;

  ESP_LOGV(TAG, "%d: Sending Wintex: CMD=0x%02X DATA=[%s]", last_command_timestamp_, static_cast<uint8_t>(command.cmd),
           format_hex_pretty(command.payload).c_str());

  write_array({len, (uint8_t) command.cmd});
  if (!command.payload.empty())
    write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = len + (uint8_t) (command.cmd);
  for (auto &data : command.payload)
    checksum += data;
  checksum ^= 0xFF;

  write_byte(checksum);
}

void Wintex::process_command_queue_() {
  if (!this->current_command_.has_value() && !command_queue_.empty()) {
    uint32_t delay = millis() - last_command_timestamp_;
    if (delay < COMMAND_DELAY)
      return;
    send_command_now_(command_queue_.front());
    command_queue_.erase(command_queue_.begin());
  }
}

void Wintex::register_sensor(WintexSensorBase *sensor){
  sensors_.push_back(sensor);
}

void Wintex::register_zone(WintexZone *zone){
  zones_.push_back(zone);
}

void Wintex::setup_zones_(){
  for (WintexZone *zone: this->zones_) {
    zone->setup(this, (uint32_t) 0x4EC, (uint16_t) 8, "");
  }
  setup_panel_sensors_();
}

void Wintex::setup_panel_sensors_() {
  // Panel Status flags at 0x00EC
  static const struct { uint8_t mask; const char *name; } status_flags[] = {
    { 0x80, "Panel Box Tamper Open" },
    { 0x10, "Auxiliary Input Active" },
    { 0x04, "Auxiliary 12V Fuse Blown" },
    { 0x02, "Local Expander Offline" },
  };
  for (auto &f : status_flags) {
    auto *s = new WintexBinarySensor(0x00EC, 1, 0, f.mask);
    s->register_as_binary_sensor(f.name);
    register_sensor(s);
  }

  // Panel Outputs 1-8 at 0x0D58 (bit 0 = output 1, bit 7 = output 8)
  // Confirmed via log analysis: 0xA0 when Stay Armed (bits 5+7), 0x00 disarmed
  static const char *output_names[] = {
    "Partition 1 2 Bell",          // bit 0 - Output 1
    "Partition 1 2 Strobe",        // bit 1 - Output 2
    "Partition 1 2 Bell 2",        // bit 2 - Output 3
    "Partition 1 2 Duress Alarm",  // bit 3 - Output 4
    "Partition 1 2 Bell 3",        // bit 4 - Output 5
    "Partition 1 2 Armed Alarm",   // bit 5 - Output 6
    "Partition 1 2 Away Armed",    // bit 6 - Output 7
    "Partition 1 2 Stay Armed",    // bit 7 - Output 8
  };
  for (int i = 0; i < 8; i++) {
    auto *s = new WintexBinarySensor(0x0D58, 1, 0, 1 << i);
    s->register_as_binary_sensor(output_names[i]);
    register_sensor(s);
  }

  // System and Battery Voltages at 0x0E78 (offsets 0 and 2)
  auto *sys_v = new WintexSensor(0x0E78, 4, 0);
  sys_v->set_accuracy_decimals(2);
  sys_v->register_as_sensor("System Voltage");
  register_sensor(sys_v);

  auto *bat_v = new WintexSensor(0x0E78, 4, 2);
  bat_v->set_accuracy_decimals(2);
  bat_v->register_as_sensor("Battery Voltage");
  register_sensor(bat_v);
}

void Wintex::queue_command_(AsyncWintexCommand command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

}  // namespace wintex
}  // namespace esphome
