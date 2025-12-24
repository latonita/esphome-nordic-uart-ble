#include "uart_nordic_server.h"

#include "esphome/core/log.h"

namespace esphome {
namespace uart_nordic_server {

static const char *const TAG = "uart_nordic_server";

void UARTNordicServerComponent::setup() {
  this->rx_buffer_ = esphome::RingBuffer::create(RX_BUFFER_CAPACITY);
  this->tx_buffer_ = esphome::RingBuffer::create(TX_BUFFER_CAPACITY);
  this->peek_valid_ = false;
  this->init_gatt_();
  if (this->auto_advertise_) {
    this->start_advertising();
  }
}

void UARTNordicServerComponent::loop() {
  this->handle_idle_();
  this->publish_notifications_();
}

void UARTNordicServerComponent::dump_config() { ESP_LOGCONFIG(TAG, "UART Nordic Server (BLE NUS)"); }

void UARTNordicServerComponent::handle_idle_() {
  if (!this->connected_ || this->idle_disconnect_timeout_ms_ == 0) {
    return;
  }
  if (millis() - this->last_activity_ms_ > this->idle_disconnect_timeout_ms_) {
    ESP_LOGI(TAG, "Idle timeout reached, disconnecting");
    this->disconnect();
  }
}

void UARTNordicServerComponent::publish_notifications_() {
  if (!this->connected_ || !this->notifications_enabled_ || this->tx_buffer_ == nullptr || this->chr_tx_handle_ == 0) {
    return;
  }
  if (this->tx_in_progress_) {
    return;
  }
  size_t pending = this->tx_buffer_->available();
  if (pending == 0) {
    return;
  }

  size_t max_payload = this->mtu_ > 3 ? (this->mtu_ - 3) : 20;
  std::vector<uint8_t> chunk(std::min(pending, max_payload));
  size_t pulled = this->tx_buffer_->read(chunk.data(), chunk.size(), 0);
  if (pulled == 0) {
    return;
  }

  this->tx_in_progress_ = true;
  this->tx_char_->set_value(std::vector<uint8_t>(chunk.begin(), chunk.end()));
  this->tx_char_->notify();
  ESP_LOGVV(TAG, "TX notify: %s", format_hex_pretty(chunk.data(), pulled).c_str());
  this->last_activity_ms_ = millis();
  this->tx_in_progress_ = false;
}

void UARTNordicServerComponent::write_array(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0 || this->tx_buffer_ == nullptr) {
    return;
  }
  size_t written = this->tx_buffer_->write_without_replacement(data, len, 0, true);
  if (written < len) {
    ESP_LOGW(TAG, "TX buffer overflow, dropped %zu bytes", len - written);
  }
  this->last_activity_ms_ = millis();
}

bool UARTNordicServerComponent::peek_byte(uint8_t *data) {
  if (this->peek_valid_) {
    if (data != nullptr) {
      *data = this->peek_byte_;
    }
    this->last_activity_ms_ = millis();
    return true;
  }

  if (this->rx_buffer_ == nullptr || this->rx_buffer_->available() == 0) {
    return false;
  }
  uint8_t tmp{0};
  size_t read = this->rx_buffer_->read(&tmp, 1, 0);
  if (read == 0) {
    return false;
  }
  this->peek_byte_ = tmp;
  this->peek_valid_ = true;
  if (data != nullptr) {
    *data = tmp;
  }
  this->last_activity_ms_ = millis();
  return true;
}

bool UARTNordicServerComponent::read_array(uint8_t *data, size_t len) {
  if (data == nullptr || len == 0) {
    return true;
  }
  size_t remaining = len;
  size_t offset = 0;

  if (this->peek_valid_) {
    data[0] = this->peek_byte_;
    this->peek_valid_ = false;
    remaining--;
    offset = 1;
  }

  if (remaining == 0) {
    this->last_activity_ms_ = millis();
    return true;
  }

  if (this->rx_buffer_ == nullptr || this->rx_buffer_->available() < remaining) {
    return false;
  }

  size_t read = this->rx_buffer_->read(data + offset, remaining, 0);
  if (read == remaining) {
    this->last_activity_ms_ = millis();
    return true;
  }
  return false;
}

int UARTNordicServerComponent::available() {
  if (this->rx_buffer_ == nullptr) {
    return 0;
  }
  return static_cast<int>(this->rx_buffer_->available() + (this->peek_valid_ ? 1 : 0));
}

void UARTNordicServerComponent::flush() {
  const uint32_t start = millis();
  while (this->tx_in_progress_ || (this->tx_buffer_ != nullptr && this->tx_buffer_->available() > 0)) {
    if (millis() - start > this->tx_flush_timeout_ms_) {
      ESP_LOGW(TAG, "Flush timeout (%u ms) with %zu bytes pending", this->tx_flush_timeout_ms_,
               this->tx_buffer_ != nullptr ? this->tx_buffer_->available() : 0);
      break;
    }
    delay(5);
    yield();
  }
}

void UARTNordicServerComponent::start_advertising() {
  if (this->server_ == nullptr) {
    ESP_LOGW(TAG, "BLE server not initialized, cannot advertise");
    return;
  }
  ESP_LOGI(TAG, "Starting BLE advertising");
  this->service_->start();
}

void UARTNordicServerComponent::stop_advertising() {
  if (this->service_ == nullptr) {
    return;
  }
  ESP_LOGI(TAG, "Stopping BLE advertising");
  this->service_->stop();
}

void UARTNordicServerComponent::disconnect() {
  if (this->server_ == nullptr) {
    return;
  }
  ESP_LOGI(TAG, "Disconnecting BLE clients");
  this->service_->stop();
  this->connected_ = false;
  this->notifications_enabled_ = false;
  this->on_disconnected_.trigger();
}

void UARTNordicServerComponent::handle_rx_write_(const uint8_t *data, uint16_t len) {
  if (data == nullptr || len == 0 || this->rx_buffer_ == nullptr) {
    return;
  }
  size_t written = this->rx_buffer_->write(data, len);
  if (written < len) {
    ESP_LOGW(TAG, "RX buffer overflow, dropped %u bytes", static_cast<unsigned>(len - written));
  }
  this->last_activity_ms_ = millis();
}

void UARTNordicServerComponent::init_gatt_() {
  this->server_ = esp32_ble_server::global_ble_server;
  if (this->server_ == nullptr) {
    ESP_LOGE(TAG, "esp32_ble_server not available");
    return;
  }

  this->server_->on_connect([this](uint16_t conn_id) { this->on_connect_(conn_id); });
  this->server_->on_disconnect([this](uint16_t conn_id) { this->on_disconnect_(conn_id); });

  this->service_ = this->server_->create_service(this->service_uuid_, true, 15);
  if (this->service_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create NUS service");
    return;
  }

  this->rx_char_ = this->service_->create_characteristic(this->rx_uuid_,
                                                         esp32_ble_server::BLECharacteristic::PROPERTY_WRITE |
                                                             esp32_ble_server::BLECharacteristic::PROPERTY_WRITE_NR);
  this->tx_char_ = this->service_->create_characteristic(this->tx_uuid_,
                                                         esp32_ble_server::BLECharacteristic::PROPERTY_NOTIFY);

  if (this->tx_char_ != nullptr) {
    auto cccd = new esp32_ble_server::BLEDescriptor(esp32_ble::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG));
    this->tx_char_->add_descriptor(cccd);
  }

  if (this->rx_char_ != nullptr) {
    this->rx_char_->on_write([this](std::span<const uint8_t> data, uint16_t) {
      this->handle_rx_write_(data.data(), data.size());
    });
  }

  this->server_->enqueue_start_service(this->service_);
}

void UARTNordicServerComponent::on_connect_(uint16_t conn_id) {
  ESP_LOGI(TAG, "Client connected (conn_id=%u)", conn_id);
  this->connected_ = true;
  this->conn_id_ = conn_id;
  this->notifications_enabled_ = true;  // assume CCCD written by client; adjust if needed
  this->last_activity_ms_ = millis();
  this->on_connected_.trigger();
}

void UARTNordicServerComponent::on_disconnect_(uint16_t conn_id) {
  ESP_LOGI(TAG, "Client disconnected (conn_id=%u)", conn_id);
  this->connected_ = false;
  this->notifications_enabled_ = false;
  this->on_disconnected_.trigger();
  if (this->auto_advertise_) {
    this->start_advertising();
  }
}

}  // namespace uart_nordic_server
}  // namespace esphome
