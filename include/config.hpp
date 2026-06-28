#pragma once

#include <nlohmann/json.hpp>

#include "logger.hpp"

struct Config
{
    // Network Settings
    unsigned short       port         = 8080;                        // Port for incoming connections
    std::string          bind_address = "0.0.0.0";                   // IP address for binding (all interfaces)

    // Streams and pools
    size_t               threads =
        std::thread::hardware_concurrency();                         // Threads in the ThreadPool (CPU - bound tasks)
    unsigned int         io_threads              = 4;                // Streams for network I/O

    // Static files
    std::string          static_root             = "static";         // The root folder for static
    size_t               static_max_file_size_mb = 10;               // Maximum file size (mb)

    // Logging
    LogLevel             log_level = LogLevel::INFO;                 // Loging level (DEBUG, INFO, WARNING, ERROR)
    unsigned int         sample_rate = 100;                          // Log every Nth request with priority
    std::string          log_file_path;                              // Path to log file (empty = console)

    // Limits and protection
    size_t               body_limit_mb   = 10;                       // Maximum size of a POST request body (mb)
    size_t               max_queue_size  = 1000;                     // Maximum size of the computing task queue
    size_t               max_connections = 10000;                    // Maximum number of simultaneous TCP connections

    // HTTP headers
    std::string          server_name     = "PriorityHttpServer/2.0"; // Server Header
    std::string          cors_allow_origin = "*";                    // Access-Control-Allow-Origin header

    // Keep-Alive
    bool                 enable_keepalive  = true;                   // Global Keep-Alive Activation
    std::chrono::seconds keepalive_timeout_sec =
        std::chrono::seconds(30);                                  // Timeout for waiting for the next request
    size_t               max_keepalive_requests = 500;               // Max. requests within a single Keep-Alive connection

    // Completion of work
    std::chrono::seconds drain_timeout_sec =
        std::chrono::seconds(5);                                   // Timeout waiting for sessions to end when stopped

    // Serialization / Deserialization
    static nlohmann::json toJson(const Config& config);
    static Config         fromJson(const nlohmann::json& json);

    // Redefinition via command line arguments
    static Config         fromArgs(Config config, int argc, char* argv[]);
};

// Displays usage information in stdout
void printHelp();

// Checks whether the --help or -h flag is present among the arguments
bool hasHelpFLag(int argc, char* argv[]);

// Loads the configuration from a file (with auto-generation) and applies CLI arguments
Config loadConfig(int argc, char* argv[]);
