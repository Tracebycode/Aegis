#include "aegis/net/server.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::unique_ptr<aegis::Server> g_server;

void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\nReceived signal " << sig << ", shutting down Aegis gracefully...\n";
        g_server->stop();
    }
}

void print_banner(const aegis::ServerConfig& config) {
    std::cout << R"(
    ___    ______ _____  ____ _____
   /   |  / ____// ___/ /  _// ___/
  / /| | / __/  / __ \  / /  \__ \
 / ___ |/ /___ / /_/ /_/ /  ___/ /
/_/  |_/_____/ \____//___/ /____/
)" << "\n";
    std::cout << "  Aegis High-Performance In-Memory Key-Value Store (v0.1.0)\n";
    std::cout << "  Host: " << config.host << " | Port: " << config.port << "\n";
    std::cout << "  Worker Threads: " << config.worker_threads << " | Shards: " << config.num_shards << "\n";
    std::cout << "  AOF: " << (config.aof_enabled ? "Enabled (" + config.aof_filepath + ")" : "Disabled") << "\n";
    std::cout << "  Snapshot: " << config.snapshot_filepath << "\n";
    std::cout << "  Ready to accept connections.\n\n";
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>        TCP port to bind (default: 6379)\n"
              << "  -h, --host <ip>          Bind address (default: 0.0.0.0)\n"
              << "  -w, --workers <num>      Worker thread count (default: hardware concurrency)\n"
              << "  -s, --shards <num>       Storage shard count (default: 64)\n"
              << "  --aof [file]             Enable Append-Only File persistence\n"
              << "  --snapshot <file>        RDB snapshot filename (default: aegis.rdb)\n"
              << "  --help                   Display this help message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    aegis::ServerConfig config;
    config.worker_threads = std::max(2u, std::thread::hardware_concurrency());

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-w" || arg == "--workers") && i + 1 < argc) {
            config.worker_threads = static_cast<size_t>(std::stoul(argv[++i]));
        } else if ((arg == "-s" || arg == "--shards") && i + 1 < argc) {
            config.num_shards = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--aof") {
            config.aof_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config.aof_filepath = argv[++i];
            }
        } else if (arg == "--snapshot" && i + 1 < argc) {
            config.snapshot_filepath = argv[++i];
        } else if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    print_banner(config);

    g_server = std::make_unique<aegis::Server>(config);
    if (!g_server->start()) {
        std::cerr << "Failed to start Aegis server on " << config.host << ":" << config.port << "\n";
        return 1;
    }

    g_server->wait();

    std::cout << "Aegis server stopped gracefully.\n";
    return 0;
}
