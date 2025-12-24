#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/components/esp32_ble_server/ble_service.h"
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/core/automation.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace uart_nordic_server {

class UARTNordicServerComponent : public uart::UARTComponent, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // UART interface
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override;
  void flush() override;
  void check_logger_conflict() override {}

  // Config setters
  void set_service_uuid(const char *uuid) { this->service_uuid_ = esp32_ble::ESPBTUUID::from_raw(uuid); }
  void set_rx_uuid(const char *uuid) { this->rx_uuid_ = esp32_ble::ESPBTUUID::from_raw(uuid); }
  void set_tx_uuid(const char *uuid) { this->tx_uuid_ = esp32_ble::ESPBTUUID::from_raw(uuid); }
  void set_passkey(uint32_t pin) { this->passkey_ = pin % 1000000U; }
  void set_mtu(uint16_t mtu) { this->desired_mtu_ = mtu; }
  void set_idle_disconnect_timeout(uint32_t timeout_ms) { this->idle_disconnect_timeout_ms_ = timeout_ms; }
  void set_autoadvertise(bool enabled) { this->auto_advertise_ = enabled; }

  Trigger<> *get_on_connected_trigger() { return &this->on_connected_; }
  Trigger<> *get_on_disconnected_trigger() { return &this->on_disconnected_; }

  // Actions
  void start_advertising();
  void stop_advertising();
  void disconnect();
  bool is_connected() const { return this->connected_; }

 protected:
  void handle_idle_();
  void publish_notifications_();
  void handle_rx_write_(const uint8_t *data, uint16_t len);
  void init_gatt_();
  void on_connect_(uint16_t conn_id);
  void on_disconnect_(uint16_t conn_id);

  bool connected_{false};
  bool notifications_enabled_{false};
  bool auto_advertise_{true};

  uint16_t mtu_{23};
  uint16_t desired_mtu_{247};
  uint16_t conn_id_{0};

  uint16_t chr_rx_handle_{0};
  uint16_t chr_tx_handle_{0};
  uint16_t chr_cccd_handle_{0};

  static constexpr size_t RX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> rx_buffer_;

  static constexpr size_t TX_BUFFER_CAPACITY = 512;
  std::unique_ptr<esphome::RingBuffer> tx_buffer_;

  bool peek_valid_{false};
  uint8_t peek_byte_{0};

  bool tx_in_progress_{false};
  uint32_t tx_flush_timeout_ms_{2000};

  uint32_t last_activity_ms_{0};
  uint32_t idle_disconnect_timeout_ms_{0};
  uint32_t passkey_{0};

  esp32_ble::ESPBTUUID service_uuid_;
  esp32_ble::ESPBTUUID rx_uuid_;
  esp32_ble::ESPBTUUID tx_uuid_;
  esp32_ble_server::BLEServer *server_{nullptr};
  esp32_ble_server::BLEService *service_{nullptr};
  esp32_ble_server::BLECharacteristic *rx_char_{nullptr};
  esp32_ble_server::BLECharacteristic *tx_char_{nullptr};

  Trigger<> on_connected_;
  Trigger<> on_disconnected_;
};

class StartAdvertisingAction : public Action<> {
 public:
  explicit StartAdvertisingAction(UARTNordicServerComponent *parent) : parent_(parent) {}
  void play() override { parent_->start_advertising(); }

 protected:
  UARTNordicServerComponent *parent_;
};

class StopAdvertisingAction : public Action<> {
 public:
  explicit StopAdvertisingAction(UARTNordicServerComponent *parent) : parent_(parent) {}
  void play() override { parent_->stop_advertising(); }

 protected:
  UARTNordicServerComponent *parent_;
};

class DisconnectAction : public Action<> {
 public:
  explicit DisconnectAction(UARTNordicServerComponent *parent) : parent_(parent) {}
  void play() override { parent_->disconnect(); }

 protected:
  UARTNordicServerComponent *parent_;
};

}  // namespace uart_nordic_server
}  // namespace esphome
