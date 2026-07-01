#include "config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

nlohmann::json Config::toJson(const Config& config)
{
    nlohmann::json json;
    json["port"]                    = config.port;
    json["bind_address"]            = config.bind_address;
    json["threads"]                 = config.threads;
    json["io_threads"]              = config.io_threads;
    json["static_root"]             = config.static_root;
    json["static_max_file_size_mb"] = config.static_max_file_size_mb;
    json["log_level"]               = logLevelToStr(config.log_level);
    json["sample_rate"]             = config.sample_rate;
    json["log_file_path"]           = config.log_file_path;
    json["body_limit_mb"]           = config.body_limit_mb;
    json["max_queue_size"]          = config.max_queue_size;
    json["max_connections"]         = config.max_connections;
    json["server_name"]             = config.server_name;
    json["cors_allow_origin"]       = config.cors_allow_origin;
    json["enable_keepalive"]        = config.enable_keepalive;
    json["keepalive_timeout_sec"]   = config.keepalive_timeout_sec.count();
    json["max_keepalive_requests"]  = config.max_keepalive_requests;
    json["drain_timeout_sec"]       = config.drain_timeout_sec.count();

    return json;
}

Config Config::fromJson(const nlohmann::json& json)
{
    Config config;

    auto safeSet = [&](const char* key, auto& field, auto converter)
    {
        if (json.contains(key))
        {
            try
            {
                field = converter(json[key]);
            }
            catch (...)
            {
                std::cerr << "[Config] Warning: invalid type for key '" << key
                          << "', using default value." << std::endl;
            }
        }
    };

    safeSet("port", config.port,
            [](const nlohmann::json& v) { return v.get<unsigned short>(); });
    safeSet("threads", config.threads,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("io_threads", config.io_threads,
            [](const nlohmann::json& v) { return v.get<unsigned int>(); });
    safeSet("static_max_file_size_mb", config.static_max_file_size_mb,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("sample_rate", config.sample_rate,
            [](const nlohmann::json& v) { return v.get<unsigned int>(); });
    safeSet("body_limit_mb", config.body_limit_mb,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("max_queue_size", config.max_queue_size,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("max_connections", config.max_connections,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("max_keepalive_requests", config.max_keepalive_requests,
            [](const nlohmann::json& v) { return v.get<size_t>(); });
    safeSet("enable_keepalive", config.enable_keepalive,
            [](const nlohmann::json& v) { return v.get<bool>(); });

    safeSet("bind_address", config.bind_address,
            [](const nlohmann::json& v) { return v.get<std::string>(); });
    safeSet("static_root", config.static_root,
            [](const nlohmann::json& v) { return v.get<std::string>(); });
    safeSet("log_file_path", config.log_file_path,
            [](const nlohmann::json& v) { return v.get<std::string>(); });
    safeSet("server_name", config.server_name,
            [](const nlohmann::json& v) { return v.get<std::string>(); });
    safeSet("cors_allow_origin", config.cors_allow_origin,
            [](const nlohmann::json& v) { return v.get<std::string>(); });

    if (json.contains("log_level") && json["log_level"].is_string())
        config.log_level = stringToLogLevel(json["log_level"].get<std::string>());
    if (json.contains("keepalive_timeout_sec") && json["keepalive_timeout_sec"].is_number())
        config.keepalive_timeout_sec =
            std::chrono::seconds(json["keepalive_timeout_sec"].get<long long>());
    if (json.contains("drain_timeout_sec") && json["drain_timeout_sec"].is_number())
        config.drain_timeout_sec = std::chrono::seconds(json["drain_timeout_sec"].get<long long>());
    if (config.threads == 0)
    {
        config.threads = 1;
        std::cerr << "[Config] Warning: threads was 0, set to 1" << std::endl;
    }

    return config;
}

static bool hasNext(int i, int argc)
{
    return (i + 1 < argc);
}

Config Config::fromArgs(Config config, int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--port")
        {
            if (hasNext(i, argc))
                config.port = static_cast<unsigned short>(std::stoi(argv[++i], nullptr, 10));
        }
        else if (arg == "-b" || arg == "--bind-address")
        {
            if (hasNext(i, argc))
                config.bind_address = argv[++i];
        }
        else if (arg == "-t" || arg == "--threads")
        {
            if (hasNext(i, argc))
                config.threads = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "-i" || arg == "--io-threads")
        {
            if (hasNext(i, argc))
                config.io_threads = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "-l" || arg == "--log-level")
        {
            if (hasNext(i, argc))
            {
                config.log_level = stringToLogLevel(argv[++i]);
            }
        }
        else if (arg == "-s" || arg == "--static-root")
        {
            if (hasNext(i, argc))
                config.static_root = argv[++i];
        }
        else if (arg == "--body-limit")
        {
            if (hasNext(i, argc))
                config.body_limit_mb = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--max-queue-size")
        {
            if (hasNext(i, argc))
                config.max_queue_size = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--max-connections")
        {
            if (hasNext(i, argc))
                config.max_connections = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--server-name")
        {
            if (hasNext(i, argc))
                config.server_name = argv[++i];
        }
        else if (arg == "--sample-rate")
        {
            if (hasNext(i, argc))
                config.sample_rate = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--enable-keepalive")
        {
            if (hasNext(i, argc))
            {
                std::string val         = argv[++i];
                config.enable_keepalive = (val == "true" || val == "1");
            }
        }
        else if (arg == "--keepalive-timeout")
        {
            if (hasNext(i, argc))
            {
                config.keepalive_timeout_sec =
                    std::chrono::seconds(std::stoul(argv[++i], nullptr, 10));
            }
        }
        else if (arg == "--max-keepalive-requests")
        {
            if (hasNext(i, argc))
                config.max_keepalive_requests = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--drain-timeout")
        {
            if (hasNext(i, argc))
            {
                config.drain_timeout_sec = std::chrono::seconds(std::stoul(argv[++i], nullptr, 10));
            }
        }
        else if (arg == "--static-max-file-size")
        {
            if (hasNext(i, argc))
                config.static_max_file_size_mb = std::stoul(argv[++i], nullptr, 10);
        }
        else if (arg == "--cors-allow-origin")
        {
            if (hasNext(i, argc))
                config.cors_allow_origin = argv[++i];
        }
        else if (arg == "--log-file")
        {
            if (hasNext(i, argc))
                config.log_file_path = argv[++i];
        }
    }

    if (config.threads == 0)
    {
        config.threads = 1;
        std::cerr << "[Config] Warning: threads was 0, set to 1" << std::endl;
    }

    return config;
}

bool hasHelpFLag(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
            return true;
    }
    return false;
}

void printHelp()
{
    std::cout << R"(Usage: prserver [OPTIONS]

Options:
  -c, --config <file>         Path to config file (default: config.json)
  -p, --port <port>           Port to listen on (default: 8080)
  -b, --bind-address <addr>   IP address to bind to (default: 0.0.0.0)
  -t, --threads <num>         Number of worker threads (default: CPU cores)
  -i, --io-threads <num>      Number of I/O threads (default: 4)
  -s, --static-root <dir>     Static files root directory (default: static)
  -l, --log-level <level>     Log level (DEBUG, INFO, WARNING, ERROR) (default: INFO)
      --log-file <path>       Log file path (empty = console) (default: empty)
      --body-limit <mb>       Max POST body size in MB (default: 10)
      --max-queue-size <num>  Max task queue size (default: 1000)
      --max-connections <num> Max simultaneous connections (default: 10000)
      --server-name <name>    Server header value (default: PriorityHttpServer/2.0)
      --sample-rate <num>     Log every N-th priority request (default: 100)
      --enable-keepalive <bool>  Enable Keep-Alive globally (true/false) (default: true)
      --keepalive-timeout <sec>  Keep-Alive timeout in seconds (default: 30)
      --max-keepalive-requests <num> Max requests per Keep-Alive connection (default: 500)
      --drain-timeout <sec>   Timeout for draining connections on shutdown (default: 5)
      --static-max-file-size <mb> Max static file size in MB (default: 10)
      --cors-allow-origin <origin> CORS header value (default: *)
  -h, --help                  Show this help message and exit
)";
}

Config loadConfig(int argc, char* argv[])
{
    // Define the path to the config (default: config.json)
    std::string configPath = "config.json";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "-c" || std::string(argv[i]) == "--config")
        {
            if (i + 1 < argc)
            {
                configPath = argv[++i];
                break;
            }
        }
    }

    // Trying to download an existing file
    if (std::filesystem::exists(configPath))
    {
        try
        {
            std::ifstream file(configPath);
            if (file.is_open())
            {
                nlohmann::json j;
                file >> j;
                auto config = Config::fromJson(j);
                return Config::fromArgs(config, argc, argv);
            }
        }
        catch (const nlohmann::json::parse_error& e)
        {
            std::cerr << "[Config] Fatal: config file is corrupted: " << e.what() << std::endl;
            std::string brokenPath = configPath + ".broken";
            try
            {
                std::filesystem::rename(configPath, brokenPath);
                std::cerr << "[Config] Renamed broken config to: " << brokenPath << std::endl;
            }
            catch (const std::exception& renameErr)
            {
                std::cerr << "[Config] Failed to rename broken config: " << renameErr.what()
                          << std::endl;
            }
            // Creating a new file with defaults
            Config        defaultConfig;
            auto          json = Config::toJson(defaultConfig);
            std::ofstream newFile(configPath);
            if (newFile.is_open())
            {
                newFile << json.dump(4) << std::endl;
                newFile.close();
                std::cout << "[Config] Created fresh default config: " << configPath << std::endl;
            }
            else
            {
                std::cerr << "[Config] Could not create new config file. Using defaults."
                          << std::endl;
            }
            return Config::fromArgs(defaultConfig, argc, argv);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Config] Error reading config: " << e.what() << ". Using defaults."
                      << std::endl;
            Config defaultConfig;
            return Config::fromArgs(defaultConfig, argc, argv);
        }
    }

    // The file does not exist, so create a new one.
    std::cout << "[Config] Config file not found, creating default: " << configPath << std::endl;
    Config        defaultConfig;
    auto          json = Config::toJson(defaultConfig);
    std::ofstream file(configPath);
    if (file.is_open())
    {
        file << json.dump(4) << std::endl;
        file.close();
    }
    else
    {
        std::cerr << "[Config] Could not create config file. Using defaults." << std::endl;
    }
    return Config::fromArgs(defaultConfig, argc, argv);
}