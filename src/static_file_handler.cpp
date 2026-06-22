#include "static_file_handler.hpp"

#include <fstream>
#include <sstream>

#include "utils.hpp"

namespace fs = std::filesystem;

StaticFileHandler::StaticFileHandler(const std::string& rootPath) : m_rootPath(rootPath)
{
    std::error_code ec;
    m_rootCanonical = fs::weakly_canonical(m_rootPath, ec);

    if (ec)
    {
        throw std::runtime_error("Failed to canonicalize static root: " + ec.message());
    }
}

std::string StaticFileHandler::getMimeType(const std::string& extension)
{
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".html", "text/html"},        {".htm", "text/html"},
        {".css", "text/css"},          {".js", "application/javascript"},
        {".json", "application/json"}, {".png", "image/png"},
        {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},         {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},      {".txt", "text/plain"},
        {".xml", "application/xml"},   {".pdf", "application/pdf"},
        {".zip", "application/zip"}};

    auto it = mimeMap.find(extension);
    return (it != mimeMap.end()) ? it->second : "application/octet-stream";
}

void StaticFileHandler::handle(const http::request<http::string_body>& req,
                               http::response<http::string_body>&      res) const
{
    // 1. Checking the prefix /static/
    std::string       target = std::string(req.target());
    const std::string prefix = "/static/";
    if (target.rfind(prefix, 0) != 0)
    {
        utils::sendNotFound(res);
        return;
    }

    // 2. Relative path
    std::string relativePath = target.substr(prefix.length());
    if (relativePath.empty())
    {
        utils::sendNotFound(res);
        return;
    }

    // 3. Full path (source)
    fs::path fullPath = m_rootPath / relativePath;

    // 4. Canonization of the full path
    std::error_code ec;
    fs::path        canonicalFull = fs::weakly_canonical(fullPath, ec);
    if (ec)
    {
        utils::sendNotFound(res);
        return;
    }

    // 5. Checking that the file is inside the root (prefix comparison)
    auto [rootIt, fullIt] = std::mismatch(m_rootCanonical.begin(), m_rootCanonical.end(),
                                          canonicalFull.begin(), canonicalFull.end());
    if (rootIt != m_rootCanonical.end())
    {
        utils::sendNotFound(res);
        return;
    }

    // 6. Checking the existence and that it is a regular file
    if (!fs::exists(fullPath, ec) || !fs::is_regular_file(fullPath, ec))
    {
        utils::sendNotFound(res);
        return;
    }

    // 7. Reading a file
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open())
    {
        utils::sendNotFound(res);
        return;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();

    // 8. MIME - type
    std::string extension = fullPath.extension().string();
    std::string mimeType  = getMimeType(extension);

    // 9. Answer
    utils::make_response(res, http::status::ok, content, mimeType);
}
