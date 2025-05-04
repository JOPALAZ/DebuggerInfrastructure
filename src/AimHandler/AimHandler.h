#pragma once
#include <utility>
#include <mutex>
#include "../Logger/Logger.h"
#include "../ExternalConfigsHelper/ExternalConfigsHelper.h"

class ServoHandler;
class gpiod_line;

struct CalibrationSettings
{
    std::pair<double, double> calibrationX;
    std::pair<double, double> calibrationY;
    std::pair<double, double> calibrationCenter;
};

class AimHandler {
public:
    static void Initialize(int pwmChip = 0, int xChannel = 0, int yChannel = 1, std::string calibrationPath = "config.json");
    static bool SetXAngle(double angle);
    static bool SetYAngle(double angle);
    static std::string SetAnglePoint(std::pair<double,double> anglePoint);
    static std::string SetPoint(std::pair<double,double> Point);
    static void EmergencyDisableAndLock();
    static void Unlock();
    static void Dispose();
    static void RestoreLastState();

private:
    static void CheckIfInitialized(std::string methodName);
    static std::mutex mtx;
    static ServoHandler* XServoPtr;
    static ServoHandler* YServoPtr;
    static CalibrationSettings calbration;
    static std::pair<double, double> lastState;
    static bool m_initialized;
    static bool locked;
};