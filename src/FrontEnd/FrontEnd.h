#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>

#include "..\ThirdParties\httplib.h" // https://github.com/yhirose/cpp-httplib

/**
 * @brief A simple HTTP server class to host a static HTML file (index.html).
 */
class FrontEnd
{
public:
    /**
     * @brief Constructor. Does NOT start the server automatically.
     * @param htmlFilePath Path to the index.html file.
     * @param listenAddress IP address or hostname (e.g., "0.0.0.0") to listen on.
     * @param port The port to bind to, e.g., 8080.
     */
    FrontEnd(const std::filesystem::path& htmlFilePath, const std::string& listenAddress = "0.0.0.0", int port = 8080);

    /**
     * @brief Destructor. Stops the server if running.
     */
    ~FrontEnd();

    /**
     * @brief Starts the server in a separate thread.
     */
    void Start();

    /**
     * @brief Stops the server and joins the thread.
     */
    void Stop();

private:
    std::string LoadFileContent(const std::filesystem::path& filePath);

private:
    std::filesystem::path htmlFilePath_;
    std::string listenAddress_;
    int port_;

    httplib::Server svr_;             ///< The HTTP server instance from cpp-httplib.
    std::thread serverThread_;        ///< The thread that runs the server.
    std::atomic<bool> stopRequested_; ///< A flag indicating that we want to stop the server.
};