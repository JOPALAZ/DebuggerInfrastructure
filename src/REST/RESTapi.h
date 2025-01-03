#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <iostream>
#include <chrono>
#include <mutex>

#include "..\ThirdParties\httplib.h"  // https://github.com/yhirose/cpp-httplib
class DbHandler;
/**
 * @brief A simple REST API server that runs on a separate thread,
 *        providing endpoints /data/range, /status, /enable, /disable.
 */
class RESTApi
{
public:
    /**
     * @brief Constructor. Does NOT start the server automatically.
     * @param listenAddress IP address or hostname (e.g., "0.0.0.0") to listen on.
     * @param port The port to bind to, e.g., 8080.
     */
    RESTApi(const std::string& listenAddress = "0.0.0.0", int port = 8080);

    /**
     * @brief Constructor. Does NOT start the server automatically.
     * @param db The pointer to the DbHandler instance.
     * @param listenAddress IP address or hostname (e.g., "0.0.0.0") to listen on.
     * @param port The port to bind to, e.g., 8080.
     */
    RESTApi(DbHandler* db, const std::string& listenAddress = "0.0.0.0", int port = 8080);

    /**
     * @brief Destructor. Stops the server if running.
     */
    ~RESTApi();

    /**
     * @brief Starts the server in a separate thread.
     */
    void Start();

    /**
     * @brief Stops the server and joins the thread.
     */
    void Stop();

    /**
     * @brief Returns the port being used for RESTapi.
     */
    static int GetPort()
    {
        return port_;
    }
private:
    /**
     * @brief Registers all HTTP endpoints (GET handlers).
     */
    void RegisterEndpoints();

private:
    std::string listenAddress_;
    static int port_;

    httplib::Server svr_;             ///< The HTTP server instance from cpp-httplib.
    std::thread serverThread_;        ///< The thread that runs the server.
    std::atomic<bool> stopRequested_; ///< A flag indicating that we want to stop the server.
};
