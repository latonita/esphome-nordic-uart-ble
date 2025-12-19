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

const LogString *UARTNordicComponent::state_to_string(FsmState s) const {
  switch (s) {
    case FsmState::IDLE:
      return LOG_STR("IDLE");
    case FsmState::CONNECTING:
      return LOG_STR("CONNECTING");
    case FsmState::DISCOVERING:
      return LOG_STR("DISCOVERING");
    case FsmState::ENABLING_NOTIF:
      return LOG_STR("ENABLING_NOTIF");
    case FsmState::UART_LINK_ESTABLISHED:
      return LOG_STR("UART_LINK_ESTABLISHED");
    case FsmState::DISCONNECTING:
      return LOG_STR("DISCONNECTING");
    case FsmState::ERROR:
      return LOG_STR("ERROR");
    default:
      return LOG_STR("UNKNOWN");
  }
}

bool UARTNordicComponent::connect() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "BLE client parent not configured");
    this->set_state_(FsmState::ERROR);
    return false;
  }
  if (this->state_ == FsmState::CONNECTING || this->state_ == FsmState::DISCOVERING ||
      this->state_ == FsmState::ENABLING_NOTIF || this->state_ == FsmState::UART_LINK_ESTABLISHED) {
    ESP_LOGV(TAG, "Connect requested but already connecting/connected (state=%d)", static_cast<int>(this->state_));
    return false;
  }
  ESP_LOGI(TAG, "Starting BLE connection");

  this->auth_completed_ = false;
  this->discovered_chars_ = false;
  this->notifications_enabled_ = false;
  this->services_discovered_ = false;
  this->chr_commands_handle_ = 0;
  this->chr_responses_handle_ = 0;
  this->chr_cccd_handle_ = 0;

  // MTU
  ESP_LOGV(TAG, "Setting desired MTU to %d", this->desired_mtu_);
  esp_ble_gatt_set_local_mtu(this->desired_mtu_);

  // Security parameters
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

  esp_ble_io_cap_t iocap = ESP_IO_CAP_IN;
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

  uint8_t key_size = 16;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

  uint8_t oob_support = ESP_BLE_OOB_DISABLE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));

  uint8_t resp_key = ESP_BLE_ID_KEY_MASK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &resp_key, sizeof(resp_key));

  // Address type can be adjusted if needed
  this->parent_->set_remote_addr_type(BLE_ADDR_TYPE_RANDOM);

  this->set_state_(FsmState::CONNECTING);
  this->parent_->connect();
  return true;
}

void UARTNordicComponent::disconnect() {
  if (this->parent_ == nullptr) {
    return;
  }
  ESP_LOGI(TAG, "Disconnecting BLE");
  this->set_state_(FsmState::DISCONNECTING);
  this->parent_->disconnect();
}

void UARTNordicComponent::write_array(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0 || this->tx_buffer_ == nullptr) {
    return;
  }
  size_t written = this->tx_buffer_->write_without_replacement(data, len, 0, true);
  if (written < len) {
    ESP_LOGW(TAG, "TX buffer overflow, dropped %zu bytes", len - written);
  }
  if (!this->tx_in_progress_ && this->state_ == FsmState::UART_LINK_ESTABLISHED) {
    this->tx_in_progress_ = true;
    this->defer_in_ble_([this]() { this->send_next_chunk_in_ble_(); });
  }
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

void UARTNordicComponent::set_state_(FsmState state) {
  if (this->state_ != state) {
    ESP_LOGV(TAG, "FSM state: %s -> %s", LOG_STR_ARG(this->state_to_string(this->state_)),
             LOG_STR_ARG(this->state_to_string(state)));
    this->state_ = state;
  }
  this->state_enter_ms_ = millis();
}

void UARTNordicComponent::send_next_chunk_in_ble_() {
  if (this->state_ != FsmState::UART_LINK_ESTABLISHED || this->tx_buffer_ == nullptr ||
      this->chr_commands_handle_ == 0) {
    this->tx_in_progress_ = false;
    ESP_LOGV(TAG, "send_next_chunk_in_ble_ , safeguard finish");
    return;
  }

  size_t pending = this->tx_buffer_->available();
  if (pending == 0) {
    this->tx_in_progress_ = false;
    ESP_LOGV(TAG, "send_next_chunk_in_ble_ , no more data to send");
    return;
  }

  size_t max_payload = this->mtu_ > 3 ? (this->mtu_ - 3) : 20;
  std::vector<uint8_t> chunk(std::min(pending, max_payload));
  size_t pulled = this->tx_buffer_->read(chunk.data(), chunk.size(), 0);
  if (pulled == 0) {
    this->tx_in_progress_ = false;
    return;
  }

  esp_err_t err =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->chr_commands_handle_,
                               pulled, chunk.data(), ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  ESP_LOGVV(TAG, "TX: %s", format_hex_pretty(chunk.data(), pulled).c_str());
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to write TX characteristic: %d", err);
    this->tx_in_progress_ = false;
  }
}

void UARTNordicComponent::defer_in_ble_(const std::function<void()> &fn) {
  this->ble_defer_fn_ = fn;
  if (this->parent_ != nullptr) {
    esp_ble_gap_read_rssi(this->parent_->get_remote_bda());
  }
}

void UARTNordicComponent::watchdog_() {
  switch (this->state_) {
    case FsmState::CONNECTING:
    case FsmState::DISCOVERING:
    case FsmState::ENABLING_NOTIF:
    case FsmState::DISCONNECTING:
    case FsmState::ERROR:
      if (millis() - this->state_enter_ms_ > this->state_timeout_ms_) {
        ESP_LOGW(TAG, "State %s timed out, resetting to IDLE", LOG_STR_ARG(this->state_to_string(this->state_)));
        if (this->parent_ != nullptr) {
          this->parent_->disconnect();
        }
        this->set_state_(FsmState::IDLE);
      }
      break;
    default:
      break;
  }
}

void UARTNordicComponent::handle_state_() {
  if (this->state_ != this->last_reported_state_) {
    ESP_LOGV(TAG, "FSM state: %s", LOG_STR_ARG(this->state_to_string(this->state_)));
    this->last_reported_state_ = this->state_;
  }

  this->watchdog_();

  switch (this->state_) {
    case FsmState::IDLE:
      // TODO: trigger connect when configured
      break;
    case FsmState::CONNECTING:
      // Wait for GATTC events
      break;
    case FsmState::DISCOVERING:
      // Discovery driven by SEARCH_CMPL
      break;
    case FsmState::ENABLING_NOTIF:
      if (this->auth_completed_ && this->discovered_chars_ && this->notifications_enabled_) {
        this->set_state_(FsmState::UART_LINK_ESTABLISHED);
        this->on_connected_.trigger();
      }
      break;
    case FsmState::UART_LINK_ESTABLISHED:
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

bool UARTNordicComponent::discover_characteristics_() {
  auto chr_commands = this->parent_->get_characteristic(this->service_uuid_, this->rx_uuid_for_commands_);
  auto chr_responses = this->parent_->get_characteristic(this->service_uuid_, this->tx_uuid_for_responses_);

  if (chr_commands == nullptr || chr_responses == nullptr) {
    ESP_LOGW(TAG, "Required characteristics not found");
    return false;
  }

  this->chr_commands_handle_ = chr_commands->handle;
  this->chr_responses_handle_ = chr_responses->handle;

  auto desc = this->parent_->get_config_descriptor(this->chr_responses_handle_);

  // auto desc = this->parent_->get_descriptor(this->service_uuid_, this->tx_uuid_,
  //                                           espbt::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG));
  if (desc == nullptr) {
    ESP_LOGW(TAG, "No CCCD descriptor found for TX characteristic");
    return false;
  }
  this->chr_cccd_handle_ = desc->handle;

  this->discovered_chars_ = true;
  return true;
}

void UARTNordicComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  
  if (this->parent_ == nullptr) {
    ESP_LOGV(TAG, "gattc_event_handler called but no parent");
    return;
  }

  ESP_LOGI(TAG, "GATTC event: %d", event);
  
  // if (event == ESP_GATTC_OPEN_EVT) {
  //   if (!this->parent_->check_addr(param->open.remote_bda))
  //     return;
  // } else {
  //   if (param->cfg_mtu.conn_id != 0 && param->cfg_mtu.conn_id != this->parent_->get_conn_id() &&
  //       event != ESP_GATTC_DISCONNECT_EVT)
  //     return;
  // }
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        esp_ble_gattc_send_mtu_req(this->parent_->get_gattc_if(), this->parent_->get_conn_id());
      } else {
        ESP_LOGW(TAG, "GATTC open failed: %d", param->open.status);
        this->set_state_(FsmState::ERROR);
      }
    } break;
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.status == ESP_GATT_OK) {
        this->mtu_ = param->cfg_mtu.mtu;
        ESP_LOGI(TAG, "MTU configured: %u", this->mtu_);
      } else {
        ESP_LOGW(TAG, "MTU config failed: %d", param->cfg_mtu.status);
      }
    } break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.conn_id != this->parent_->get_conn_id())
        break;

      if (this->services_discovered_) {
        ESP_LOGV(TAG, "Services already processed, ignoring duplicate SEARCH_CMPL");
        break;
      }

      if (this->notifications_enabled_) {
        ESP_LOGV(TAG, "Notifications already enabled, skipping re-subscription");
        break;
      }

      if (!this->discover_characteristics_()) {
        this->set_state_(FsmState::ERROR);
        this->parent_->disconnect();
        break;
      }

      // register for notify on TX characteristic (NUS TX -> notifications)
      if (this->chr_responses_handle_ != 0) {
        auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                        this->chr_responses_handle_);
        if (status != ESP_OK) {
          ESP_LOGW(TAG, "Register for notify failed: %d", status);
        }
      }

      this->services_discovered_ = true;

      uint16_t notify_en = 0x0001;
      auto err = esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                this->chr_cccd_handle_, sizeof(notify_en), (uint8_t *) &notify_en,
                                                ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);

      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable notifications on handle 0x%04X: %d", this->chr_cccd_handle_, err);
        this->set_state_(FsmState::ERROR);
        return;
      }

      this->set_state_(FsmState::ENABLING_NOTIF);
    } break;

    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.conn_id != this->parent_->get_conn_id())
        break;
      if (param->write.status == ESP_GATT_OK) {
        if (param->write.handle == this->chr_cccd_handle_) {
          this->notifications_enabled_ = true;
          ESP_LOGI(TAG, "Notifications enabled (CCCD write ok)");
        } else {
          ESP_LOGI(TAG, "ESP_GATTC_WRITE_DESCR_EVT not for CCCD.. (handle = %u)", param->write.handle);
        }
      } else {
        ESP_LOGW(TAG, "CCCD write failed: status=%d", param->write.status);
        this->set_state_(FsmState::ERROR);
      }
    } break;

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.conn_id != this->parent_->get_conn_id())
        break;
      if (param->write.status == ESP_GATT_OK) {
        size_t pending = this->tx_buffer_->available();
        if (pending == 0) {
          this->tx_in_progress_ = false;
          ESP_LOGV(TAG, "TX completed: no more data to send");
          return;
        } else {
          // we are already in BLE thread, no need to defer next portion
          this->send_next_chunk_in_ble_();
          // we probably need to wait to read reply first, then send next 
        }
      } else {
        ESP_LOGW(TAG, "TX write failed: status=%d", param->write.status);
        this->tx_in_progress_ = false;
      }
    } break;
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGV(TAG, "Notification received (handle = 0x%04X, value_len = %d, conn_id = %d, our conn_id = %d)", 
        param->notify.handle, param->notify.value_len, param->notify.conn_id, this->parent_->get_conn_id());
      if (param->notify.conn_id != this->parent_->get_conn_id())
        break;

      if (!param->notify.is_notify) {
        ESP_LOGW(TAG, "Indication received instead of notification, not supported");
        break;
      }

      if (!param->notify.value || param->notify.value_len == 0) {
        ESP_LOGW(TAG, "Notification with empty payload received");
        break;
      }

      ESP_LOGVV(TAG, "RX: %s", format_hex_pretty(param->notify.value, param->notify.value_len).c_str());

      if (this->rx_buffer_ != nullptr) {
        size_t written = this->rx_buffer_->write(param->notify.value, param->notify.value_len);
        if (written < param->notify.value_len) {
          ESP_LOGW(TAG, "RX buffer overflow, dropped %d bytes", param->notify.value_len - (int) written);
        }
      }
    } break;
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "GATTC disconnected, reason=0x%02X", param->disconnect.reason);
      this->set_state_(FsmState::IDLE);
      this->on_disconnected_.trigger();
    } break;
    default:
      break;
  }
}

void UARTNordicComponent::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      if (!this->parent_->check_addr(param->ble_security.ble_req.bd_addr)) {
        break;
      }
      ESP_LOGV(TAG, "Supplying PIN %06u", this->passkey_);
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, this->passkey_);
      break;
    }
    case ESP_GAP_BLE_SEC_REQ_EVT: {
      if (!this->parent_->check_addr(param->ble_security.ble_req.bd_addr)) {
        break;
      }
      auto auth_cmpl = param->ble_security.auth_cmpl;
      ESP_LOGV(TAG, "ESP_GAP_BLE_SEC_REQ_EVT success: %d, fail reason: %d, auth mode: %d", auth_cmpl.success,
               auth_cmpl.fail_reason, auth_cmpl.auth_mode);
      esp_err_t sec_rsp = esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      ESP_LOGV(TAG, "esp_ble_gap_security_rsp result: %d", sec_rsp);
    } break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
        break;
      }
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG, "Pairing completed (auth mode %d)", param->ble_security.auth_cmpl.auth_mode);
        this->auth_completed_ = true;
      } else {
        ESP_LOGW(TAG, "Pairing failed, reason=%d", param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    }
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT: {
      if (!this->parent_->check_addr(param->read_rssi_cmpl.remote_addr)) {
        break;
      }
      this->rssi_ = param->read_rssi_cmpl.rssi;
      if (this->ble_defer_fn_ != nullptr) {
        auto fn = this->ble_defer_fn_;
        this->ble_defer_fn_ = nullptr;
        fn();
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace uart_nordic
}  // namespace esphome
