#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

namespace hd::net {

enum class ConnectionState : uint8_t {
    Connected     = 0,
    Detecting     = 1,
    Disconnected  = 2,
    Reconnecting  = 3,
    Recovering    = 4,
    Offline       = 5,
};

struct ReconnectConfig {
    uint32_t detect_timeout_ms    = 5000;
    uint32_t min_backoff_ms       = 1000;
    uint32_t max_backoff_ms       = 30000;
    double   backoff_multiplier   = 2.0;
    uint32_t max_retry_count      = 10;
    uint32_t recovery_timeout_ms  = 30000;
};

struct ServerEndpoint {
    std::string host;
    uint16_t    port;
    bool        is_primary;
};

struct ConnectionStateInfo {
    ConnectionState     state;
    uint32_t            retry_count;
    uint32_t            current_backoff_ms;
    uint64_t            last_heartbeat_ms;
    uint64_t            state_enter_time_ms;
    std::string         active_server;
    uint32_t            pending_io_count;
};

class NetworkStateMachine {
public:
    explicit NetworkStateMachine(const ReconnectConfig& config = ReconnectConfig{});
    ~NetworkStateMachine() = default;

    void SetServers(const std::vector<ServerEndpoint>& servers);
    void SetCallbacks(
        std::function<void(ConnectionState, ConnectionState)> on_state_change,
        std::function<bool(const ServerEndpoint&)> on_connect_attempt,
        std::function<void()> on_recovery_complete
    );

    void Start();
    void Stop();

    void OnHeartbeatSuccess();
    void OnHeartbeatTimeout();
    void OnIoSuccess();
    void OnIoError(int32_t error_code);

    ConnectionState GetState() const;
    ConnectionStateInfo GetStateInfo() const;
    const ServerEndpoint& GetActiveServer() const;

    bool IsConnected() const;
    bool CanPerformIo() const;

    void Tick();

private:
    void TransitionTo(ConnectionState new_state);
    void TryReconnect();
    bool TryNextServer();
    uint32_t ComputeBackoff() const;

    ReconnectConfig config_;
    std::vector<ServerEndpoint> servers_;
    size_t current_server_idx_;

    std::atomic<ConnectionState> state_;
    uint32_t retry_count_;
    uint32_t current_backoff_ms_;
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::chrono::steady_clock::time_point state_enter_time_;
    std::chrono::steady_clock::time_point last_reconnect_attempt_;
    uint32_t pending_io_count_;

    std::function<void(ConnectionState, ConnectionState)> on_state_change_;
    std::function<bool(const ServerEndpoint&)> on_connect_attempt_;
    std::function<void()> on_recovery_complete_;

    mutable std::mutex mutex_;
    bool running_;
};

} // namespace hd::net
