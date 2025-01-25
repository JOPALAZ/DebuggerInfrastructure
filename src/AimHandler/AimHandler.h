#pragma once
#include <utility>
#include <mutex>

class ServoHandler;
class gpiod_line;

class AimHandler
{
private:
    static std::mutex mtx;
    static ServoHandler* XServoPtr;
    static gpiod_line* XServoLinePtr; 
    static ServoHandler* YServoPtr;
    static gpiod_line* YServoLinePtr;
    static bool m_initialized;
    static void CheckIfInitialized(std::string methodName);
public:
    AimHandler() = delete;
    ~AimHandler() = delete;
    static void Initialize(unsigned int Xpin, unsigned int Ypin);
    static void SetXAngle(double angle);
    static void SetYAngle(double angle);
    static void SetAnglePoint(std::pair<double,double> anglePoint);
    static void SetX (double X);
    static void SetY (double Y);
    static void SetPoint(std::pair<double,double> point);
    static void Dispose();
};