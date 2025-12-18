#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace uart_nordic {

namespace espbt = esphome::esp32_ble_tracker;

class UARTNordicComponent : public uart::UARTComponent, public Component {
 public:
  enum class FsmState : uint8_t {
    IDLE,
    CONNECTING,
    DISCOVERING,
    ENABLING_NOTIF,
    LINK_ESTABLISHED,
    DISCONNECTING,
    ERROR,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;

  // BLE control
  void connect();
  void disconnect();

  // uart::UARTComponent interface
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override;
  void flush() override;

  void check_logger_conflict() override {}

  void set_service_uuid(const char *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_tx_uuid(const char *uuid) { this->tx_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_rx_uuid(const char *uuid) { this->rx_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_passkey(uint32_t pin) { this->passkey_ = pin % 1000000U; }

  bool is_connected() const { return this->state_ == FsmState::LINK_ESTABLISHED; }

 protected:
  void set_state_(FsmState state);
  void handle_state_();
  FsmState state_{FsmState::IDLE};
  FsmState last_reported_state_{FsmState::IDLE};

  static constexpr size_t RX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> rx_buffer_;

  static constexpr size_t TX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> tx_buffer_;

  // single-byte peek cache
  bool peek_valid_{false};
  uint8_t peek_byte_{0};

  std::vector<uint8_t> tx_queue_;
  bool tx_in_progress_{false};

  int last_error_{0};

  uint32_t last_activity_ms_{0};
  uint32_t reconnect_backoff_ms_{0};

  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID tx_uuid_;
  espbt::ESPBTUUID rx_uuid_;
  uint32_t passkey_{0};
};

}  // namespace uart_nordic
}  // namespace esphome
