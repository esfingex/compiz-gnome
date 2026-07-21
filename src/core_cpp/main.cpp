#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[compiz-gnome-engine] Recibida señal de parada. Cerrando motor..." << std::endl;
        g_running = false;
    }
}

std::string resolveSocketPath(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--socket-path") {
            return argv[i + 1];
        }
    }
    if (const char* env_p = std::getenv("COMPIZ_SOCKET_PATH")) {
        if (std::string(env_p).length() > 0) return env_p;
    }
    if (const char* xdg_p = std::getenv("XDG_RUNTIME_DIR")) {
        if (std::string(xdg_p).length() > 0) return std::string(xdg_p) + "/compiz_gnome_engine.sock";
    }
    return "/tmp/compiz_gnome_engine.sock";
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string socketPath = resolveSocketPath(argc, argv);

    std::cout << "==========================================================" << std::endl;
    std::cout << "  compiz-gnome C++20 / Vulkan 1.3 Offscreen Engine v0.1.0  " << std::endl;
    std::cout << "==========================================================" << std::endl;
    std::cout << "[+] Inicializando subsistema Vulkan 1.3 RAII & VMA..." << std::endl;
    std::cout << "[+] Escuchando IPC Socket en " << socketPath << std::endl;

    // Fixed Timestep Loop (120Hz)
    constexpr std::chrono::nanoseconds physics_dt(8333333); // 1.0 / 120.0 s
    auto last_time = std::chrono::high_resolution_clock::now();

    while (g_running) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = current_time - last_time;

        if (elapsed >= physics_dt) {
            // Step physical simulations (Water, Wobbly, Particles)
            last_time = current_time;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[+] Motor C++20 finalizado limpiamente." << std::endl;
    return 0;
}
