#include "uart_nordic.h"

#include "esphome/core/log.h"

namespace esphome {
namespace uart_nordic {

static const char *const TAG = "uart_nordic";

void UARTNordicComponent::setup() {
  this->rx_buffer_ = esphome::RingBuffer::create(RX_BUFFER_CAPACITY);
  this->tx_buffer_ = esphome::RingBuffer::create(TX_BUFFER_CAPACITY);
  this->peek_valid_ = false;
  this->set_state_(FsmState::IDLE);
}

void UARTNordicComponent::loop() { this->handle_state_(); }

void UARTNordicComponent::dump_config() { ESP_LOGCONFIG(TAG, "UART Nordic Component (stub)"); }

void UARTNordicComponent::connect() {
  // TODO: initiate BLE connection
  this->set_state_(FsmState::CONNECTING);
}

void UARTNordicComponent::disconnect() {
  // TODO: initiate BLE disconnect and cleanup
  this->set_state_(FsmState::DISCONNECTING);
}

void UARTNordicComponent::write_array(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0 || this->tx_buffer_ == nullptr) {
    return;
  }
  size_t written = this->tx_buffer_->write_without_replacement(data, len, 0, true);
  if (written < len) {
    ESP_LOGW(TAG, "TX buffer overflow, dropped %zu bytes", len - written);
  }
  // TODO: schedule BLE NUS TX from tx_buffer_
}

bool UARTNordicComponent::peek_byte(uint8_t *data) {
  if (this->peek_valid_) {
    if (data != nullptr) {
      *data = this->peek_byte_;
    }
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
  return true;
}

bool UARTNordicComponent::read_array(uint8_t *data, size_t len) {
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
    return true;
  }

  if (this->rx_buffer_ == nullptr || this->rx_buffer_->available() < remaining) {
    // restore peek_valid_? we already consumed it; treat as failure but keep consumed byte
    return false;
  }

  size_t read = this->rx_buffer_->read(data + offset, remaining, 0);
  return read == remaining;
}

int UARTNordicComponent::available() {
  if (this->rx_buffer_ == nullptr) {
    return 0;
  }
  return static_cast<int>(this->rx_buffer_->available() + (this->peek_valid_ ? 1 : 0));
}

void UARTNordicComponent::flush() {
  // TODO: block until pending TX is sent
}

void UARTNordicComponent::set_state_(FsmState state) { this->state_ = state; }

void UARTNordicComponent::handle_state_() {
  if (this->state_ != this->last_reported_state_) {
    ESP_LOGV(TAG, "FSM state: %d -> %d", static_cast<int>(this->last_reported_state_),
             static_cast<int>(this->state_));
    this->last_reported_state_ = this->state_;
  }

  switch (this->state_) {
    case FsmState::IDLE:
      // TODO: trigger connect when configured
      break;
    case FsmState::CONNECTING:
      // TODO: handle GAP connect progress
      break;
    case FsmState::DISCOVERING:
      // TODO: discover NUS characteristics
      break;
    case FsmState::ENABLING_NOTIF:
      // TODO: enable notifications / negotiate MTU
      break;
    case FsmState::LINK_ESTABLISHED:
      // TODO: process RX/TX in ready state
      break;
    case FsmState::DISCONNECTING:
      // TODO: clean up and return to IDLE
      break;
    case FsmState::ERROR:
      // TODO: backoff and retry
      break;
  }
}

}  // namespace uart_nordic
}  // namespace esphome
