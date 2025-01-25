#include "RESTapi.h"
#include "../Logger/Logger.h"
#include "../Common/DbHandler.h"
#include "../ThirdParties/nlohmann/json.hpp"  // nlohmann::json
#include "../LaserHandler/LaserHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../AimHandler/AimHandler.h"

// A static pointer to a single DbHandler instance.
static DbHandler* dbHandler = nullptr;
int RESTApi::port_ = -1;
// We'll use this for convenience:
using nlohmann::json;

RESTApi::RESTApi(const std::string& listenAddress, int port)
    : listenAddress_(listenAddress)
    , serverThread_()
    , stopRequested_(false)
{
    if(port_ == -1)
    {
        port_ = port;
    }
    // Initialize or acquire DbHandler instance here
    static DbHandler dbInstance; 
    dbHandler = &dbInstance; 
}

RESTApi::RESTApi(DbHandler* db, const std::string& listenAddress, int port)
    : listenAddress_(listenAddress)
    , serverThread_()
    , stopRequested_(false)
{
    if(port_ == -1)
    {
        port_ = port;
    }
    dbHandler = db;
}

RESTApi::~RESTApi()
{
    Stop();
}

void RESTApi::Start()
{
    if (serverThread_.joinable()) {
        Logger::Info("RESTApi is already running! No action taken.");
        return;
    }

    stopRequested_ = false;

    svr_.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");

        if (req.method == "OPTIONS") {
            res.status = 200;
            res.set_content("", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });


    // Launch the server in a separate thread
    serverThread_ = std::thread([this]() {
        RegisterEndpoints();

        Logger::Info("Starting RESTApi on {}:{}", listenAddress_, port_);
        // This call blocks until svr_.stop() is called
        svr_.listen(listenAddress_.c_str(), port_);
        Logger::Info("RESTApi stopped.");
    });
}

void RESTApi::Stop()
{
    port_ = -1;
    if (!serverThread_.joinable()) {
        Logger::Info("RESTApi is not running, stop request ignored.");
        return;
    }

    stopRequested_ = true;
    svr_.stop();

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void RESTApi::RegisterEndpoints()
{
    /**
     * GET /data
     *
     * Possible scenarios based on query params:
     *   - no params => returns all data
     *   - start & end => returns data in [start, end]
     *   - only start => returns data where time > start
     *   - only end => returns data where time < end
     *
     * Returns JSON array of objects:
     * [
     *   {
     *     "time": <number>,
     *     "event": <number>,
     *     "className": <string>,
     *     "outcome": <string>
     *   },
     *   ...
     * ]
     */
    svr_.Get("/data", [&](const httplib::Request& req, httplib::Response& res) {
        bool hasStart = req.has_param("start");
        bool hasEnd   = req.has_param("end");

        // Helper lambda to parse a query param as int64_t
        auto parseParam = [&](const std::string& paramName) -> std::optional<int64_t> {
            try {
                return std::stoll(req.get_param_value(paramName));
            } catch (...) {
                return {};
            }
        };

        std::vector<RecordData> records;

        if (!hasStart && !hasEnd) {
            // 1) No params => all data
            Logger::Verbose("GET /data => ReadData()");
            records = dbHandler->ReadData();
        }
        else if (hasStart && hasEnd) {
            // 2) both start & end => range
            auto maybeStart = parseParam("start");
            auto maybeEnd   = parseParam("end");
            if (!maybeStart || !maybeEnd) {
                Logger::Error("GET /data: invalid start/end parameter");
                res.status = 400; // Bad request
                res.set_content("Invalid 'start' or 'end' parameter (must be integer)", "text/plain");
                return;
            }
            Logger::Verbose("GET /data: range => ReadDataByRange({}, {})", *maybeStart, *maybeEnd);
            records = dbHandler->ReadDataByRange(*maybeStart, *maybeEnd);
        }
        else if (hasStart && !hasEnd) {
            // 3) only start => time > start
            auto maybeStart = parseParam("start");
            if (!maybeStart) {
                Logger::Error("GET /data: invalid start parameter");
                res.status = 400;
                res.set_content("Invalid 'start' parameter (must be integer)", "text/plain");
                return;
            }
            Logger::Verbose("GET /data: after => ReadDataAfter({})", *maybeStart);
            records = dbHandler->ReadDataAfter(*maybeStart);
        }
        else { // (!hasStart && hasEnd)
            // 4) only end => time < end
            auto maybeEnd = parseParam("end");
            if (!maybeEnd) {
                Logger::Error("GET /data: invalid end parameter");
                res.status = 400;
                res.set_content("Invalid 'end' parameter (must be integer)", "text/plain");
                return;
            }
            Logger::Verbose("GET /data: before => ReadDataBefore({})", *maybeEnd);
            records = dbHandler->ReadDataBefore(*maybeEnd);
        }

        // Build a JSON array of objects
        json jResponse = json::array();
        for (const auto& r : records) {
            json jObj;
            jObj["time"]      = r.time;

            jObj["event"]     = r.event;
            jObj["className"] = r.className;
            jObj["outcome"]   = r.outcome;
            jResponse.push_back(jObj);
        }

        // Return as JSON with the appropriate content type
        res.set_content(jResponse.dump(), "application/json");
    });

    svr_.Post("/ForceSetServoDebug", [&](const httplib::Request& req, httplib::Response& res) {
            bool hasAngleX = req.has_param("angleX");
            bool hasAngleY = req.has_param("angleY");
            json jResponse;
            // Helper lambda to parse a query param as int64_t
            auto parseParam = [&](const std::string& paramName) -> std::optional<int64_t> {
                try {
                    return std::stoll(req.get_param_value(paramName));
                } catch (...) {
                    return {};
                }
            };
            if(hasAngleX && hasAngleY)
            {
                int64_t angleX = parseParam("angleX").value();
                int64_t angleY = parseParam("angleY").value();
                try
                {
                    AimHandler::SetAnglePoint({angleX, angleY});
                    jResponse["status"]  = "OK";
                    jResponse["message"] = "Successfuly set";
                }
                catch(std::exception ex)
                {
                    jResponse["status"]  = "Error";
                    jResponse["message"] = fmt::format("Couldn't set angle: {}", ex.what());
                }

            }
            else
            {
                jResponse["status"]  = "Error";
                jResponse["message"] = "No angle provided as parameter";
            }

            // Return as JSON with the appropriate content type
            res.set_content(jResponse.dump(), "application/json");
        });


    // GET /status
    svr_.Get("/status", [&](const httplib::Request& req, httplib::Response& res) {
        Logger::Verbose("Handled /status request");

        json j;
        j["status"] = "UNIMPLEMENTED";
        res.set_content(j.dump(), "application/json");
    });

    // GET /enable
    svr_.Get("/enable", [&](const httplib::Request& req, httplib::Response& res) {
        Logger::Verbose("Handled /enable request");
        LaserHandler::Enable();
        json j;
        j["message"] = "Enabled";
        res.set_content(j.dump(), "application/json");
    });

    // GET /disable
    svr_.Get("/disable", [&](const httplib::Request& req, httplib::Response& res) {
        Logger::Verbose("Handled /disable request");
        LaserHandler::Disable();
        json j;
        j["message"] = "Disabled";
        res.set_content(j.dump(), "application/json");
    });
        // ----------------------------------------------------------------------------
    // POST /data
    //
    // Expects JSON array of objects with the shape:
    // [
    //   {
    //     "time": <int64>,
    //     "event": <int>,
    //     "className": <string>,
    //     "outcome": <string>
    //   },
    //   ...
    // ]
    //
    // Inserts each record into the database.
    // ----------------------------------------------------------------------------
    svr_.Post("/data", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            // Parse request body as JSON
            auto bodyJson = json::parse(req.body);

            // Check if it's indeed an array
            if (!bodyJson.is_array()) {
                res.status = 400;
                res.set_content("Request body must be a JSON array of RecordData objects.", "text/plain");
                return;
            }

            // For each item in the JSON array, parse to RecordData and insert
            int insertedCount = 0;
            for (auto& item : bodyJson) {
                // Validate fields: time, event, className, outcome
                if (!item.contains("time")      || !item["time"].is_number_integer() ||
                    !item.contains("event")     || !item["event"].is_number_integer() ||
                    !item.contains("className") || !item["className"].is_string()      ||
                    !item.contains("outcome")   || !item["outcome"].is_string()) 
                {
                    Logger::Error("POST /data: Invalid RecordData in JSON array");
                    res.status = 400;
                    res.set_content(R"(Each object must have "time"(int), "event"(int), "className"(string), "outcome"(string))", 
                                    "text/plain");
                    return;
                }

                int64_t time       = item["time"].get<int64_t>();
                int     eventId    = item["event"].get<int>();
                auto    className  = item["className"].get<std::string>();
                auto    outcome    = item["outcome"].get<std::string>();

                // Create RecordData object
                RecordData rd(time, eventId, className, outcome);

                // Insert into DB
                dbHandler->InsertData(rd);
                insertedCount++;
            }

            // If all records inserted successfully
            Logger::Info("POST /data: Inserted {} records.", insertedCount);

            // Build a success JSON response
            json jResponse;
            jResponse["status"]  = "OK";
            jResponse["message"] = fmt::format("Inserted {} records.", insertedCount);

            res.set_content(jResponse.dump(), "application/json");
        }
        catch (std::exception& ex) {
            // JSON parse error or other
            Logger::Error("POST /data exception: {}", ex.what());
            res.status = 400;
            res.set_content(std::string("Error parsing or inserting data: ") + ex.what(), "text/plain");
        }
    });
}
