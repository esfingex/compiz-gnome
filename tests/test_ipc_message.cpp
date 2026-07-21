#include "ipc_server.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

int main() {
    std::cout << "[TEST] Running IPC Ancillary Payload unit tests..." << std::endl;

    compiz::core::ipc::AncillaryFdPayload payload;
    payload.data = {'P', 'I', 'N', 'G'};
    payload.fds = {10, 11};

    assert(payload.data.size() == 4);
    assert(payload.fds.size() == 2);
    assert(payload.fds[0] == 10);
    assert(payload.fds[1] == 11);

    std::cout << "  ✓ Payload payload data size: " << payload.data.size() << " bytes" << std::endl;
    std::cout << "  ✓ Ancillary FDs count: " << payload.fds.size() << std::endl;

    std::cout << "[PASS] IPC payload unit tests passed successfully!" << std::endl;
    return 0;
}
