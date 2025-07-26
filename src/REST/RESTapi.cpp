#include "RESTapi.h"
#include "../Logger/Logger.h"
#include "../DbHandler/DbHandler.h"
#include "../ThirdParties/nlohmann/json.hpp"  // nlohmann::json
#include "../LaserHandler/LaserHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../AimHandler/AimHandler.h"
#include "../DeadLocker/DeadLocker.h"
#include "../ExceptionExtensions/ExceptionExtensions.h"
#include "../NeuralNetworkHandler/NeuralNetworkHandler.h"

namespace DebuggerInfrastructure
    {
    // A static pointer to a single DbHandler instance.
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
        auto logRequest = [](const httplib::Request& req) {
            std::ostringstream params;
            for (const auto& p : req.params) {
                params << p.first << "=" << p.second << " ";
            }
            Logger::Verbose("Incoming request: {} {} Params: [{}] Body: {}", req.method, req.path, params.str(), req.body);
        };

        auto logResponse = [](const httplib::Request& req, int status, const std::string& body) {
            Logger::Verbose("Response to {} {}: status={} body={}", req.method, req.path, status, body);
        };

        svr_.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            if (req.method == "OPTIONS") {
                res.status = 200;
                res.set_content("", "text/plain");
                logRequest(req);
                logResponse(req, res.status, "");
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

        svr_.Get("/data", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            bool hasStart = req.has_param("start");
            bool hasEnd   = req.has_param("end");

            auto parseParam = [&](const std::string& paramName) -> std::optional<int64_t> {
                try {
                    return std::stoll(req.get_param_value(paramName));
                } catch (...) {
                    return {};
                }
            };

            std::vector<RecordData> records;
            if (!hasStart && !hasEnd) {
                Logger::Verbose("GET /data => ReadData()");
                records = DbHandler::ReadData();
            } else if (hasStart && hasEnd) {
                auto maybeStart = parseParam("start");
                auto maybeEnd   = parseParam("end");
                if (!maybeStart || !maybeEnd) {
                    res.status = 400;
                    res.set_content("Invalid 'start' or 'end' parameter (must be integer)", "text/plain");
                    logResponse(req, res.status, res.body);
                    return;
                }
                Logger::Verbose("GET /data: range => ReadDataByRange({}, {})", *maybeStart, *maybeEnd);
                records = DbHandler::ReadDataByRange(*maybeStart, *maybeEnd);
            } else if (hasStart) {
                auto maybeStart = parseParam("start");
                if (!maybeStart) {
                    res.status = 400;
                    res.set_content("Invalid 'start' parameter (must be integer)", "text/plain");
                    logResponse(req, res.status, res.body);
                    return;
                }
                Logger::Verbose("GET /data: after => ReadDataAfter({})", *maybeStart);
                records = DbHandler::ReadDataAfter(*maybeStart);
            } else {
                auto maybeEnd = parseParam("end");
                if (!maybeEnd) {
                    res.status = 400;
                    res.set_content("Invalid 'end' parameter (must be integer)", "text/plain");
                    logResponse(req, res.status, res.body);
                    return;
                }
                Logger::Verbose("GET /data: before => ReadDataBefore({})", *maybeEnd);
                records = DbHandler::ReadDataBefore(*maybeEnd);
            }

            json jResponse = json::array();
            for (const auto& r : records) {
                json jObj;
                jObj["time"]        = r.time;
                jObj["event"]       = r.event;
                jObj["className"]   = r.className;
                jObj["description"] = r.description;
                jResponse.push_back(jObj);
            }
            res.set_content(jResponse.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/ForceSetServoDebug", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            bool hasAngleX = req.has_param("angleX");
            bool hasAngleY = req.has_param("angleY");
            int statusCode = 200;
            std::string msg;
            json jResponse;

            auto parseParam = [&](const std::string& paramName) -> std::optional<double> {
                try {
                    return std::stod(req.get_param_value(paramName));
                } catch (...) {
                    return {};
                }
            };

            if (hasAngleX && hasAngleY) {
                auto maybeAngleX = parseParam("angleX");
                auto maybeAngleY = parseParam("angleY");
                if (!maybeAngleX || !maybeAngleY) {
                    statusCode = 400;
                    msg = "Invalid angle parameter(s)";
                } else {
                    try {
                        msg = AimHandler::SetDefaultState({*maybeAngleX, *maybeAngleY});
                    } catch (BadRequestException& ex) {
                        statusCode = 400;
                        msg = ex.what();
                    } catch (std::runtime_error& ex) {
                        statusCode = 500;
                        msg = ex.what();
                    }
                }
            } else {
                statusCode = 400;
                msg = "No angle provided as parameter";
            }
            jResponse["message"] = msg;
            res.status = statusCode;
            res.set_content(jResponse.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/ToggleLaserForCalibration", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            int statusCode = 200;
            std::string msg;
            json jResponse;
            try {
                msg = AimHandler::IsCalibrationEnabled() ? AimHandler::DisableCalibration() : AimHandler::EnableCalibration();
            } catch (BadRequestException& ex) {
                statusCode = 400;
                msg = ex.what();
            } catch (std::runtime_error& ex) {
                statusCode = 500;
                msg = ex.what();
            }
            jResponse["message"] = msg;
            res.status = statusCode;
            res.set_content(jResponse.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/ForceSetPoint", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            bool hasX = req.has_param("pointX");
            bool hasY = req.has_param("pointY");
            int statusCode = 200;
            std::string msg;
            json jResponse;

            auto parseParam = [&](const std::string& paramName) -> std::optional<double> {
                try {
                    return std::stod(req.get_param_value(paramName));
                } catch (...) {
                    return {};
                }
            };

            if (hasX && hasY) {
                auto maybeX = parseParam("pointX");
                auto maybeY = parseParam("pointY");
                if (!maybeX || !maybeY) {
                    statusCode = 400;
                    msg = "Invalid point parameter(s)";
                } else {
                    Logger::Verbose("{} was called with parameters, pointX={} | pointY={}", req.path, *maybeX, *maybeY);
                    try {
                        msg = AimHandler::ShootAt({*maybeX, *maybeY});
                        auto tab = msg.find('\t');
                        if (tab != std::string::npos) msg = msg.substr(0, tab);
                    } catch (BadRequestException& ex) {
                        statusCode = 400;
                        msg = ex.what();
                    } catch (std::runtime_error& ex) {
                        statusCode = 500;
                        msg = ex.what();
                    }
                }
            } else {
                statusCode = 400;
                msg = "No point provided as parameter";
            }
            jResponse["message"] = msg;
            res.status = statusCode;
            res.set_content(jResponse.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Get("/status", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            std::string status;
            if (DeadLocker::IsLocked()) {
                std::string reasons;
                for (const auto& el : DeadLocker::lockReasons) {
                    reasons += " " + el + " ";
                }
                status = "Locked due to an emergency (Reasons:" + reasons + ")";
            } else if (AimHandler::IsCalibrationEnabled()) {
                status = "Calibration (Insects will be ignored and laser is always ON)";
            } else {
                status = "Armed";
            }
            json j;
            j["status"] = status;
            res.set_content(j.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/enable", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            int statusCode = 200;
            std::string msg;
            try {
                if (DeadLocker::lockReasons.find(NAMEOF(RESTApi)) != DeadLocker::lockReasons.end()) {
                    DeadLocker::Recover(NAMEOF(RESTApi));
                    DbHandler::InsertDataNow(EMERGENCYREMOVELOCKREASON, NAMEOF(RESTApi), "RESTapi veto was revoked.");
                    msg = "Successfully unLocked.";
                } else {
                    msg = "Already unlocked by REST";
                }
            } catch (BadRequestException& ex) {
                statusCode = 400;
                msg = ex.what();
            } catch (std::exception& ex) {
                statusCode = 500;
                msg = ex.what();
            }
            json j;
            j["message"] = msg;
            res.status = statusCode;
            res.set_content(j.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/disable", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            int statusCode = 200;
            std::string msg;
            try {
                if (DeadLocker::lockReasons.find(NAMEOF(RESTApi)) == DeadLocker::lockReasons.end()) {
                    msg = "Successfully Locked.";
                    DbHandler::InsertDataNow(EMERGENCYADDLOCKREASON, NAMEOF(RESTApi), "RESTapi veto was invoked.");
                } else {
                    msg = "Already locked by REST";
                }
                DeadLocker::EmergencyInitiate(NAMEOF(RESTApi));
            } catch (BadRequestException& ex) {
                statusCode = 400;
                msg = ex.what();
            } catch (std::exception& ex) {
                statusCode = 500;
                msg = ex.what();
            }
            json j;
            j["message"] = msg;
            res.status = statusCode;
            res.set_content(j.dump(), "application/json");
            logResponse(req, res.status, res.body);
        });

        svr_.Post("/data", [&](const httplib::Request& req, httplib::Response& res) {
            logRequest(req);
            try {
                auto bodyJson = json::parse(req.body);
                if (!bodyJson.is_array()) {
                    res.status = 400;
                    res.set_content("Request body must be a JSON array of RecordData objects.", "text/plain");
                    logResponse(req, res.status, res.body);
                    return;
                }
                int insertedCount = 0;
                for (auto& item : bodyJson) {
                    if (!item.contains("time")      || !item["time"].is_number_integer() ||
                        !item.contains("event")     || !item["event"].is_number_integer() ||
                        !item.contains("className") || !item["className"].is_string()      ||
                        !item.contains("description")   || !item["description"].is_string())
                    {
                        res.status = 400;
                        res.set_content(R"(Each object must have "time"(int), "event"(int), "className"(string), "description"(string))",
                                    "text/plain");
                        logResponse(req, res.status, res.body);
                        return;
                    }
                    int64_t time        = item["time"].get<int64_t>();
                    int     eventId     = item["event"].get<int>();
                    auto    className   = item["className"].get<std::string>();
                    auto    description = item["description"].get<std::string>();
                    RecordData rd(time, eventId, className, description);
                    DbHandler::InsertData(rd);
                    insertedCount++;
                }
                json jResponse;
                jResponse["status"]  = "OK";
                jResponse["message"] = fmt::format("Inserted {} records.", insertedCount);
                res.set_content(jResponse.dump(), "application/json");
                logResponse(req, res.status, res.body);
            }
            catch (std::exception& ex) {
                res.status = 400;
                res.set_content(std::string("Error parsing or inserting data: ") + ex.what(), "text/plain");
                logResponse(req, res.status, res.body);
            }
        });

        svr_.Get("/video", [&](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Connection", "close");
            try {
                res.set_content_provider(
                    "multipart/x-mixed-replace; boundary=frame",
                    [&](size_t /*offset*/, httplib::DataSink& sink) {
                        try {
                            auto frame = NeuralNetworkHandler::GetLatestFrame();
                            std::vector<uchar> buf;
                            cv::imencode(".jpg", frame, buf);
                            std::ostringstream header;
                            header << "--frame\r\n"
                                << "Content-Type: image/jpeg\r\n"
                                << "Content-Length: " << buf.size() << "\r\n\r\n";
                            sink.write(header.str().data(), header.str().size());
                            sink.write(reinterpret_cast<char*>(buf.data()), buf.size());
                            sink.write("\r\n", 2);
                            return true;
                        } catch (...) {
                            sink.write(
                                "HTTP/1.1 500 Internal Server Error\r\n"
                                "Content-Type: text/plain\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "Internal Server Error", 93
                            );
                            return false;
                        }
                    }
                );
            } catch (...) {
                res.status = 500;
                res.set_content("Internal Server Error", "text/plain");
            }
        });
    }
}