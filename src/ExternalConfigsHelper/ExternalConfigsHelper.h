#pragma once
#include <string>
#include <unordered_map>
#include "../ThirdParties/nlohmann/json.hpp"  // nlohmann::json

class CalibrationSettings;

class ExternalConfigsHelper
{
public:
    static CalibrationSettings getCalibrationSettings(std::string path = "config.json");
    static CalibrationSettings getOrCreateCalibrationSettings(std::string path = "config.json");
    static void setCalibrationSettings(CalibrationSettings settings, std::string path = "config.json");
    static void setDefaultCalibrationSettings(std::string path = "config.json");
private:
    static void writeJson(nlohmann::json value, std::string path);
    static nlohmann::json readJson(std::string path);
    static std::unordered_map<std::string, CalibrationSettings> settingsMap;
    static CalibrationSettings defaultCalibrationSettings;
};