#include "network_state_machine.h"

#include <algorithm>
#include <cstdio>

namespace hd::net {

NetworkStateMachine::NetworkStateMachine(const ReconnectConfig& config)
    : config_(config)
    , current_server_idx_(0)
    , state_(ConnectionState::Offline)
    , retry_count_(0)
    , current_backoff_ms_(config.min_backoff_ms)
    , pending_io_count_(0)
    , running_(false) {
}

void NetworkStateMachine::SetServers(const std::vector<ServerEndpoint>& servers) {
    std::lock_guard<std::mutex> lock(mutex_);
    servers_ = servers;
    if (!servers_.empty()) {
        current_server_idx_ = 0;
        for (size_t i = 0; i < servers_.size(); i++) {
            if (servers_[i].is_primary) {
                current_server_idx_ = i;
                break;
            }
        }
    }
}

void NetworkStateMachine::SetCallbacks(
    std::function<void(ConnectionState, ConnectionState)> on_state_change,
    std::function<bool(const ServerEndpoint&)> on_connect_attempt,
    std::function<void()> on_recovery_complete) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_state_change_ = std::move(on_state_change);
    on_connect_attempt_ = std::move(on_connect_attempt);
    on_recovery_complete_ = std::move(on_recovery_complete);
}

void NetworkStateMachine::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    retry_count_ = 0;
    current_backoff_ms_ = config_.min_backoff_ms;
    state_enter_time_ = std::chrono::steady_clock::now();
    last_heartbeat_ = std::chrono::steady_clock::now();

    if (!servers_.empty()) {
        TransitionTo(ConnectionState::Connected);
    } else {
        TransitionTo(ConnectionState::Offline);
    }
}

void NetworkStateMachine::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    TransitionTo(ConnectionState::Offline);
}

void NetworkStateMachine::OnHeartbeatSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_heartbeat_ = std::chrono::steady_clock::now();

    auto cur = state_.load(std::memory_order_relaxed);
    if (cur == ConnectionState::Detecting || cur == ConnectionState::Reconnecting) {
        retry_count_ = 0;
        current_backoff_ms_ = config_.min_backoff_ms;
        TransitionTo(ConnectionState::Connected);
    } else if (cur == ConnectionState::Recovering) {
        retry_count_ = 0;
        current_backoff_ms_ = config_.min_backoff_ms;
        TransitionTo(ConnectionState::Connected);
        if (on_recovery_complete_) on_recovery_complete_();
    }
}

void NetworkStateMachine::OnHeartbeatTimeout() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto cur = state_.load(std::memory_order_relaxed);

    if (cur == ConnectionState::Connected) {
        TransitionTo(ConnectionState::Detecting);
    } else if (cur == ConnectionState::Detecting) {
        TransitionTo(ConnectionState::Disconnected);
    }
}

void NetworkStateMachine::OnIoSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_io_count_ > 0) pending_io_count_--;
    last_heartbeat_ = std::chrono::steady_clock::now();
}

void NetworkStateMachine::OnIoError(int32_t error_code) {
    (void)error_code;
    std::lock_guard<std::mutex> lock(mutex_);
    pending_io_count_++;

    auto cur = state_.load(std::memory_order_relaxed);
    if (cur == ConnectionState::Connected) {
        TransitionTo(ConnectionState::Detecting);
    }
}

ConnectionState NetworkStateMachine::GetState() const {
    return state_.load(std::memory_order_relaxed);
}

ConnectionStateInfo NetworkStateMachine::GetStateInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ConnectionStateInfo info{};
    info.state = state_.load(std::memory_order_relaxed);
    info.retry_count = retry_count_;
    info.current_backoff_ms = current_backoff_ms_;

    auto now = std::chrono::steady_clock::now();
    info.last_heartbeat_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(last_heartbeat_.time_since_epoch()).count());
    info.state_enter_time_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(state_enter_time_.time_since_epoch()).count());

    if (!servers_.empty() && current_server_idx_ < servers_.size()) {
        info.active_server = servers_[current_server_idx_].host + ":" +
                             std::to_string(servers_[current_server_idx_].port);
    }
    info.pending_io_count = pending_io_count_;
    return info;
}

const ServerEndpoint& NetworkStateMachine::GetActiveServer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return servers_[current_server_idx_];
}

bool NetworkStateMachine::IsConnected() const {
    return state_.load(std::memory_order_relaxed) == ConnectionState::Connected;
}

bool NetworkStateMachine::CanPerformIo() const {
    auto s = state_.load(std::memory_order_relaxed);
    return s == ConnectionState::Connected || s == ConnectionState::Detecting;
}

void NetworkStateMachine::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;

    auto cur = state_.load(std::memory_order_relaxed);
    auto now = std::chrono::steady_clock::now();

    if (cur == ConnectionState::Connected) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat_).count();
        if (elapsed > config_.detect_timeout_ms) {
            TransitionTo(ConnectionState::Detecting);
        }
    } else if (cur == ConnectionState::Detecting) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_enter_time_).count();
        if (elapsed > config_.detect_timeout_ms) {
            TransitionTo(ConnectionState::Disconnected);
        }
    } else if (cur == ConnectionState::Disconnected) {
        TryReconnect();
    } else if (cur == ConnectionState::Reconnecting) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reconnect_attempt_).count();
        if (elapsed >= current_backoff_ms_) {
            if (on_connect_attempt_ && !servers_.empty()) {
                bool ok = on_connect_attempt_(servers_[current_server_idx_]);
                if (ok) {
                    retry_count_ = 0;
                    current_backoff_ms_ = config_.min_backoff_ms;
                    TransitionTo(ConnectionState::Recovering);
                } else {
                    retry_count_++;
                    if (retry_count_ >= config_.max_retry_count) {
                        if (!TryNextServer()) {
                            TransitionTo(ConnectionState::Offline);
                        } else {
                            retry_count_ = 0;
                            TransitionTo(ConnectionState::Reconnecting);
                        }
                    } else {
                        current_backoff_ms_ = ComputeBackoff();
                        last_reconnect_attempt_ = now;
                    }
                }
            }
        }
    } else if (cur == ConnectionState::Recovering) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_enter_time_).count();
        if (elapsed > config_.recovery_timeout_ms) {
            TransitionTo(ConnectionState::Disconnected);
        }
    }
}

void NetworkStateMachine::TransitionTo(ConnectionState new_state) {
    auto old_state = state_.load(std::memory_order_relaxed);
    if (old_state == new_state) return;

    state_.store(new_state, std::memory_order_relaxed);
    state_enter_time_ = std::chrono::steady_clock::now();

    if (new_state == ConnectionState::Reconnecting) {
        last_reconnect_attempt_ = std::chrono::steady_clock::now();
    }

    if (on_state_change_) on_state_change_(old_state, new_state);
}

void NetworkStateMachine::TryReconnect() {
    retry_count_ = 0;
    current_backoff_ms_ = config_.min_backoff_ms;
    TransitionTo(ConnectionState::Reconnecting);
}

bool NetworkStateMachine::TryNextServer() {
    if (servers_.size() <= 1) return false;

    for (size_t i = 0; i < servers_.size(); i++) {
        size_t next = (current_server_idx_ + 1 + i) % servers_.size();
        if (next == current_server_idx_) continue;
        current_server_idx_ = next;
        return true;
    }
    return false;
}

uint32_t NetworkStateMachine::ComputeBackoff() const {
    uint32_t backoff = config_.min_backoff_ms;
    for (uint32_t i = 0; i < retry_count_; i++) {
        backoff = static_cast<uint32_t>(backoff * config_.backoff_multiplier);
        if (backoff >= config_.max_backoff_ms) {
            backoff = config_.max_backoff_ms;
            break;
        }
    }
    return backoff;
}

} // namespace hd::net
