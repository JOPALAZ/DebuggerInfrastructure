#include "AimHandler.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"
#include "../ExceptionExtensions/ExceptionExtensions.h"


std::mutex AimHandler::mtx;
ServoHandler* AimHandler::XServoPtr = nullptr;
gpiod_line* AimHandler::XServoLinePtr = nullptr; 
ServoHandler* AimHandler::YServoPtr = nullptr;
gpiod_line* AimHandler::YServoLinePtr = nullptr;
std::pair<double, double> AimHandler::lastState = {0,0};
bool AimHandler::m_initialized = false;
bool AimHandler::locked = false;


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
    XServoPtr = new ServoHandler(XServoLinePtr, 50);
    YServoPtr = new ServoHandler(YServoLinePtr, 50);
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

bool AimHandler::SetXAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    if(locked)
    {
        Logger::Info("AimHandler::SetXAngle(double angle) called, but AimHandler is locked");
        return false;
    }
    CheckIfInitialized("SetXAngle(double angle)");
    XServoPtr->SetAngle(angle);
    lastState.first = angle;
    return true;
}

bool AimHandler::SetYAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    if(locked)
    {
        Logger::Info("AimHandler::SetYAngle(double angle) called, but AimHandler is locked");
        return false;
    }
    CheckIfInitialized("SetYAngle(double angle)");
    YServoPtr->SetAngle(angle);
    lastState.second = angle;
    return true;
}

std::string AimHandler::SetAnglePoint(std::pair<double,double> anglePoint)
{
    std::lock_guard<std::mutex> guard(mtx);
    if(locked)
    {
        Logger::Info("AimHandler::SetAnglePoint(std::pair<double,double> anglePoint) called, but AimHandler is locked");
        throw BadRequestException("The servos are locked due to the emergency, cannot move them.");
    }
    try
    {
        CheckIfInitialized("SetAnglePoint(std::pair<double,double> anglePoint)");
    }
    catch(std::exception ex)
    {
        throw std::runtime_error(fmt::format("Cannot move servos, internal error: [{}]", ex.what()));
    }
    XServoPtr->SetAngle(anglePoint.first);
    YServoPtr->SetAngle(anglePoint.second);
    lastState = anglePoint;
    return "Successfuly set";
}

bool AimHandler::SetX(double angle)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

bool AimHandler::SetY(double angle)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

std::string AimHandler::SetPoint(std::pair<double,double> anglePoint)
{
    throw std::runtime_error("UNIMPLIMENTED");
}

void AimHandler::EmergencyDisableAndLock()
{
    XServoPtr->SetAngle(0);
    YServoPtr->SetAngle(0);
    locked = true;
}

void AimHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(mtx);
    locked = false;
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

void AimHandler::RestoreLastState()
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized("RestoreLastState()");
    XServoPtr->SetAngle(lastState.first);
    YServoPtr->SetAngle(lastState.second);
}