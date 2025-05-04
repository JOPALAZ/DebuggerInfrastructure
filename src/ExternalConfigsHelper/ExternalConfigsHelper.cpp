#include "../AimHandler/AimHandler.h"
#include "ExternalConfigsHelper.h"
#include <fstream>

bool fileExists(std::string filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string read_file_to_string(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if(!file.good())
    {
        throw std::runtime_error("Could not open stream to read the settings from [" + path + "]");
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

CalibrationSettings ExternalConfigsHelper::getCalibrationSettings(std::string path)
{
    CalibrationSettings settings;
    if(settingsMap.contains(path))
    {
        settings = settingsMap.at(path); 
    }
    else
    {
        nlohmann::json settingsJson = readJson(path);
        std::vector<double> calibrationArrayX = settingsJson["calibrationX"];
        std::vector<double> calibrationArrayY = settingsJson["calibrationY"];
        std::vector<double> calibrationCenter = settingsJson["calibrationCenter"];
        settings.calibrationX = {calibrationArrayX[0],calibrationArrayX[1]};
        settings.calibrationY = {calibrationArrayY[0],calibrationArrayY[1]};
        settings.calibrationCenter = {calibrationCenter[0],calibrationCenter[1]};
        settingsMap[path] = settings;
    }
    return settings;
}

CalibrationSettings ExternalConfigsHelper::getOrCreateCalibrationSettings(std::string path)
{
    CalibrationSettings settings;
    try
    {
        settings = getCalibrationSettings(path);
    }
    catch(...)
    {
        setDefaultCalibrationSettings(path);
        settings = getCalibrationSettings(path);
    }
    return settings;
}

void ExternalConfigsHelper::writeJson(nlohmann::json value, std::string path)
{
    std::string stringVal = value.dump();
    std::ofstream stream(path);
    if(!stream.good())
    {
        throw std::runtime_error("Could not open stream to save the settings to [" + path + "]");
    }
    stream<<stringVal;
}

nlohmann::json ExternalConfigsHelper::readJson(std::string path)
{
    std::string fileContentsString = read_file_to_string(path);
    return nlohmann::json::parse(fileContentsString);
}

void ExternalConfigsHelper::setCalibrationSettings(CalibrationSettings settings, std::string path)
{
    settingsMap[path] = settings;
    nlohmann::json jsonSettings;
    if(fileExists(path))
    {
        jsonSettings = readJson(path);
    }
    jsonSettings["calibrationX"] = std::vector<double>{settings.calibrationX.first, settings.calibrationX.second};
    jsonSettings["calibrationY"] = std::vector<double>{settings.calibrationY.first, settings.calibrationY.second};
    jsonSettings["calibrationCenter"] = std::vector<double>{settings.calibrationCenter.first, settings.calibrationCenter.second};
    writeJson(jsonSettings, path);
}

void ExternalConfigsHelper::setDefaultCalibrationSettings(std::string path)
{
    setCalibrationSettings(defaultCalibrationSettings, path);
}

CalibrationSettings ExternalConfigsHelper::defaultCalibrationSettings = CalibrationSettings{
    {23.0, 55.0},
    {10.0, 65.0},
    {36.0, 38.0}
};
std::unordered_map<std::string, CalibrationSettings> ExternalConfigsHelper::settingsMap;