#pragma once
#include <utility>
#include <mutex>

class ServoHandler;
class gpiod_line;

class AimHandler
{
private:
    static std::mutex mtx;
    static std::pair<double, double> lastState;
    static ServoHandler* XServoPtr;
    static gpiod_line* XServoLinePtr; 
    static ServoHandler* YServoPtr;
    static gpiod_line* YServoLinePtr;
    static bool m_initialized;
    static bool locked;
    static void CheckIfInitialized(std::string methodName);
public:
    AimHandler() = delete;
    ~AimHandler() = delete;
    static void Initialize(unsigned int Xpin, unsigned int Ypin);
    static bool SetXAngle(double angle);
    static bool SetYAngle(double angle);
    static std::string SetAnglePoint(std::pair<double,double> anglePoint);
    static bool SetX (double X);
    static bool SetY (double Y);
    static std::string SetPoint(std::pair<double,double> point);
    static void EmergencyDisableAndLock();
    static void Unlock();
    static void RestoreLastState();
    static void Dispose();
};