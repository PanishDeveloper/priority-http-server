#pragma once

#include <filesystem>
#include <string>

#include "router.hpp"

class StaticFileHandler : public Handler
{
public:
    explicit StaticFileHandler (const std::string& rootPath);
    void handle (const boost::beast::http::request<boost::beast::http::string_body>& req,
                 boost::beast::http::response<boost::beast::http::string_body>& res) const override;

private:
    std::filesystem::path            m_rootPath;
    std::filesystem::path            m_rootCanonical;
    [[nodiscard]] static std::string getMimeType (const std::string& extension);
};
