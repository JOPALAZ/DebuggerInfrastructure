#include "FrontEnd.h"
#include "../Logger/Logger.h"
#include "../REST/RESTapi.h"
#include <fstream>
#include <regex>
#include <sstream>

std::string SplitPort(const std::string& str)
{
    return str.substr(0,str.find(':'));
}

FrontEnd::FrontEnd(const std::filesystem::path& htmlFilePath, const std::string& listenAddress, int port)
    : htmlFilePath_(std::filesystem::absolute(htmlFilePath))   // Path to the HTML file to serve
    , listenAddress_(listenAddress) // IP address to bind to
    , port_(port)                   // Port to bind to
    , stopRequested_(false)         // Atomic flag for stopping the server
{}

FrontEnd::~FrontEnd()
{
    Stop(); // Ensure the server is stopped when the object is destroyed
}

void FrontEnd::Start()
{
    // Prevent starting the server if it's already running
    if (serverThread_.joinable()) {
        Logger::Info("Server is already running! No action taken.");
        return;
    }

    stopRequested_ = false; // Reset the stop flag

    // Launch the server in a separate thread
    serverThread_ = std::thread([this]() {
        // Set up a route to serve the HTML file
        svr_.Get("/", [this](const httplib::Request& req, httplib::Response& res) {

            Logger::Verbose("GET / => LoadFileContent({})", htmlFilePath_.generic_string());
            // Load the HTML file content
            std::string content = LoadFileContent(htmlFilePath_);

            if (!content.empty()) {
                // Dynamically replace `BASE_URL` with the REST API's address
                auto it = req.headers.find("Host");
                std::string host = (it != req.headers.end()) ? SplitPort(it->second) : "localhost";
                int port = RESTApi::GetPort();
                if(port == -1)
                {
                    throw std::runtime_error("Cannot parse RESTapi port -> no sense to start FrontEnd");
                }
                std::string restUrl = "http://" + host + ':' + std::to_string(port);

                content = std::regex_replace(
                    content,
                    std::regex(R"(const BASE_URL = 'REPLACEMEPLEASE';)"), // Pattern to replace
                    "const BASE_URL = '" + restUrl + "';"               // Replacement value
                );

                // Respond with the modified HTML content
                res.set_content(content, "text/html");
            } else {
                // Respond with a 404 error if the file is not found
                res.status = 404;
                res.set_content("index.html not found", "text/plain");
            }
        });

        // Start the server and log the event
        Logger::Info("Starting server on {}:{}", listenAddress_, port_);
        svr_.listen(listenAddress_.c_str(), port_);
        Logger::Info("Server stopped.");
    });
}

void FrontEnd::Stop()
{
    // Ensure the server is running before attempting to stop it
    if (!serverThread_.joinable()) {
        Logger::Info("Server is not running, stop request ignored.");
        return;
    }

    // Signal the server to stop and wait for the thread to join
    stopRequested_ = true;
    svr_.stop();

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

std::string FrontEnd::LoadFileContent(const std::filesystem::path& filePath)
{
    // Open the file at the specified path
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Logger::Error("Failed to open file: {}", filePath.generic_string());
        return {}; // Return an empty string if the file cannot be opened
    }

    // Read the file content into a string
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}
