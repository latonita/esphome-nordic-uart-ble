#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/automation.h"
#include "esp_gatt_defs.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace uart_nordic {

namespace espbt = esphome::esp32_ble_tracker;

class UARTNordicComponent : public uart::UARTComponent, public ble_client::BLEClientNode, public Component {
 public:
  enum class FsmState : uint8_t {
    IDLE,
    CONNECTING,
    DISCOVERING,
    ENABLING_NOTIF,
    UART_LINK_ESTABLISHED,
    DISCONNECTING,
    ERROR,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; };

  // BLE control
  bool connect();
  bool is_connected() const { return this->state_ == FsmState::UART_LINK_ESTABLISHED; }
  void disconnect();

  const LogString *state_to_string(FsmState s) const;

  // BLE client callbacks
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // uart::UARTComponent interface
  void write_array(const uint8_t *data, size_t len) override;
  void write_byte(uint8_t data);
  bool read_byte(uint8_t *data);
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override;
  void flush() override;

  void check_logger_conflict() override {}

  void set_service_uuid(const char *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_rx_uuid(const char *uuid) { this->rx_uuid_for_commands_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_tx_uuid(const char *uuid) { this->tx_uuid_for_responses_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_passkey(uint32_t pin) { this->passkey_ = pin % 1000000U; }
  void set_mtu(uint16_t mtu) { this->desired_mtu_ = mtu; }
  void set_flush_timeout(uint32_t timeout_ms) { this->tx_flush_timeout_ms_ = timeout_ms; }
  void set_idle_disconnect_timeout(uint32_t timeout_ms) { this->idle_disconnect_timeout_ms_ = timeout_ms; }
  void set_autoconnect_on_access(bool enabled) { this->autoconnect_on_access_ = enabled; }

  Trigger<> *get_on_connected_trigger() { return &this->on_connected_; }
  Trigger<> *get_on_disconnected_trigger() { return &this->on_disconnected_; }
  Trigger<> *get_on_tx_complete_trigger() { return &this->on_tx_complete_; }

 protected:
  void set_state_(FsmState state);
  void handle_state_();
  void send_next_chunk_in_ble_();
  void defer_in_ble_(const std::function<void()> &fn);
  void watchdog_();
  bool maybe_autoconnect_();
  bool discover_characteristics_();
  FsmState state_{FsmState::IDLE};
  FsmState last_reported_state_{FsmState::IDLE};

  bool auth_completed_{false};
  bool discovered_chars_{false};
  bool notifications_enabled_{false};
  bool services_discovered_{false};

  uint16_t mtu_{23};
  uint16_t desired_mtu_{247};

  std::function<void()> ble_defer_fn_{nullptr};

  Trigger<> on_connected_;
  Trigger<> on_disconnected_;
  Trigger<> on_tx_complete_;

  uint16_t chr_commands_handle_{0};
  uint16_t chr_responses_handle_{0};
  uint16_t chr_cccd_handle_{0};

  static constexpr size_t RX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> rx_buffer_;

  static constexpr size_t TX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> tx_buffer_;

  // single-byte peek cache
  bool peek_valid_{false};
  uint8_t peek_byte_{0};

  std::vector<uint8_t> tx_queue_;
  bool tx_in_progress_{false};
  uint32_t tx_flush_timeout_ms_{2000};

  int last_error_{0};

  uint32_t last_activity_ms_{0};
  uint32_t idle_disconnect_timeout_ms_{0};
  bool autoconnect_on_access_{false};
  uint32_t last_autoconnect_attempt_ms_{0};
  uint32_t reconnect_backoff_ms_{0};

  uint32_t state_enter_ms_{0};
  uint32_t state_timeout_ms_{5000};

  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID rx_uuid_for_commands_;
  espbt::ESPBTUUID tx_uuid_for_responses_;
  uint32_t passkey_{0};
  int8_t rssi_{0};
};

class UARTNordicConnectAction : public Action<> {
 public:
  explicit UARTNordicConnectAction(UARTNordicComponent *parent) : parent_(parent) {}
  void play() override { parent_->connect(); }

 protected:
  UARTNordicComponent *parent_;
};

class UARTNordicDisconnectAction : public Action<> {
 public:
  explicit UARTNordicDisconnectAction(UARTNordicComponent *parent) : parent_(parent) {}
  void play() override { parent_->disconnect(); }

 protected:
  UARTNordicComponent *parent_;
};

}  // namespace uart_nordic
}  // namespace esphome
