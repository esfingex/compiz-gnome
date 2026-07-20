#include "ipc_server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

namespace compiz::core::ipc {

IpcServer::IpcServer(const std::string& socket_path)
    : socket_path_(socket_path) {}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start() {
    unlink(socket_path_.c_str());

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[IPC] Error al crear socket AF_UNIX: " << strerror(errno) << std::endl;
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[IPC] Error al hacer bind en " << socket_path_ << ": " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 8) < 0) {
        std::cerr << "[IPC] Error en listen(): " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    listen_thread_ = std::thread(&IpcServer::listen_loop, this);
    std::cout << "[IPC] Servidor Socket escuchando en " << socket_path_ << std::endl;
    return true;
}

void IpcServer::stop() {
    if (running_) {
        running_ = false;
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
        if (listen_thread_.joinable()) {
            listen_thread_.join();
        }
        unlink(socket_path_.c_str());
    }
}

void IpcServer::listen_loop() {
    while (running_) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (!running_) break;
            continue;
        }

        if (callback_) {
            AncillaryFdPayload payload = recv_with_fds(client_fd);
            callback_(client_fd, payload);
        }
        close(client_fd);
    }
}

bool IpcServer::send_with_fds(int client_fd, const void* data, size_t size, const std::vector<int>& fds) {
    msghdr msg{};
    iovec iov{};

    iov.iov_base = const_cast<void*>(data);
    iov.iov_len = size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int) * 16)];
    } control_un;

    if (!fds.empty()) {
        msg.msg_control = control_un.control;
        msg.msg_controllen = CMSG_LEN(sizeof(int) * fds.size());

        cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
        cmptr->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmptr), fds.data(), sizeof(int) * fds.size());
    }

    return sendmsg(client_fd, &msg, 0) >= 0;
}

AncillaryFdPayload IpcServer::recv_with_fds(int client_fd) {
    AncillaryFdPayload payload{};
    payload.data.resize(4096);

    msghdr msg{};
    iovec iov{};
    iov.iov_base = payload.data.data();
    iov.iov_len = payload.data.size();
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int) * 16)];
    } control_un;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    ssize_t bytes_received = recvmsg(client_fd, &msg, 0);
    if (bytes_received > 0) {
        payload.data.resize(bytes_received);

        for (cmsghdr* cmptr = CMSG_FIRSTHDR(&msg); cmptr != nullptr; cmptr = CMSG_NXTHDR(&msg, cmptr)) {
            if (cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS) {
                int num_fds = (cmptr->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                int* fds_ptr = reinterpret_cast<int*>(CMSG_DATA(cmptr));
                for (int i = 0; i < num_fds; ++i) {
                    payload.fds.push_back(fds_ptr[i]);
                }
            }
        }
    } else {
        payload.data.clear();
    }

    return payload;
}

} // namespace compiz::core::ipc
