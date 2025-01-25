#include "AimHandler.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"

std::mutex AimHandler::mtx;
ServoHandler* AimHandler::XServoPtr = nullptr;
gpiod_line* AimHandler::XServoLinePtr = nullptr; 
ServoHandler* AimHandler::YServoPtr = nullptr;
gpiod_line* AimHandler::YServoLinePtr = nullptr;
bool AimHandler::m_initialized = false;


void AimHandler::Initialize(unsigned int Xpin, unsigned int Ypin)
{
    std::lock_guard<std::mutex> guard(mtx);

    if(m_initialized)
    {
        Logger::Info("AimHandler is already Initialized");
        return;
    }
    XServoLinePtr = GPIOHandler::GetLine(Xpin);
    YServoLinePtr = GPIOHandler::GetLine(Ypin);
    GPIOHandler::RequestLineOutput(XServoLinePtr, "ServoXGPIO");
    GPIOHandler::RequestLineOutput(YServoLinePtr, "ServoYGPIO");
    XServoPtr = new ServoHandler();
    YServoPtr = new ServoHandler();
    XServoPtr->Initialize(XServoLinePtr);
    YServoPtr->Initialize(YServoLinePtr);
    m_initialized = true;
}

void AimHandler::CheckIfInitialized(std::string methodName)
{
    static std::string msg;   
    if(!m_initialized)
    {
        msg = fmt::format("AimHandler wasn't initialized, but {} was called", methodName);
        Logger::Error(msg);
        throw std::runtime_error(msg);
    }
}

void AimHandler::SetXAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized("SetXAngle(double angle)");
    XServoPtr->SetAngle(angle);
}

void AimHandler::SetYAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized("SetYAngle(double angle)");
    YServoPtr->SetAngle(angle);
}

void AimHandler::SetAnglePoint(std::pair<double,double> anglePoint)
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized("SetAnglePoint(std::pair<double,double> anglePoint)");
    XServoPtr->SetAngle(anglePoint.first);
    YServoPtr->SetAngle(anglePoint.second);
}

void AimHandler::SetX(double angle)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

void AimHandler::SetY(double angle)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

void AimHandler::SetPoint(std::pair<double,double> anglePoint)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

void AimHandler::Dispose()
{
    std::lock_guard<std::mutex> guard(mtx);

    CheckIfInitialized("Dispose()");
    if(XServoPtr && YServoPtr)
    {
        delete XServoPtr;
        delete YServoPtr;
    }
    else
    {
        throw std::runtime_error("An initialized AimHandler::Dispose() tried to dispose of non existent ServoHandlers which shouldn't be possible.");
    }
    m_initialized = false;
}