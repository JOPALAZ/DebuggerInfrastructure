#pragma once
#include <utility>
#include <mutex>
#include "../Logger/Logger.h"

class ServoHandler;
class gpiod_line;

class AimHandler {
public:
    static void Initialize(int pwmChip, int xChannel, int yChannel);
    static bool SetXAngle(double angle);
    static bool SetYAngle(double angle);
    static std::string SetAnglePoint(std::pair<double,double> anglePoint);
    static std::string SetPoint(std::pair<double,double> Point)
    {
        Logger::Info("{} is not ready yet, is TBD", NAMEOF(AimHandler::SetPoint));
    }
    static void EmergencyDisableAndLock();
    static void Unlock();
    static void Dispose();
    static void RestoreLastState();

private:
    static void CheckIfInitialized(std::string methodName);

    static std::mutex mtx;
    static ServoHandler* XServoPtr;
    static ServoHandler* YServoPtr;
    static std::pair<double, double> lastState;
    static bool m_initialized;
    static bool locked;
};