#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

namespace compiz::core::ipc {

struct AncillaryFdPayload {
    std::vector<uint8_t> data;
    std::vector<int> fds;
};

using MessageCallback = std::function<void(int client_fd, const AncillaryFdPayload& payload)>;

class IpcServer {
public:
    explicit IpcServer(const std::string& socket_path);
    ~IpcServer();

    bool start();
    void stop();

    // Envía datos binarios + FDs mediante SCM_RIGHTS
    static bool send_with_fds(int client_fd, const void* data, size_t size, const std::vector<int>& fds);

    // Recibe datos binarios + FDs mediante SCM_RIGHTS
    static AncillaryFdPayload recv_with_fds(int client_fd);

    void set_message_callback(MessageCallback cb) { callback_ = std::move(cb); }

private:
    void listen_loop();

    std::string socket_path_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread listen_thread_;
    MessageCallback callback_;
};

} // namespace compiz::core::ipc
