#include "AimHandler.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"
#include "../ExceptionExtensions/ExceptionExtensions.h"
std::mutex AimHandler::mtx;
ServoHandler* AimHandler::XServoPtr = nullptr;
ServoHandler* AimHandler::YServoPtr = nullptr;
std::pair<double, double> AimHandler::lastState = {0,0};
bool AimHandler::m_initialized = false;
bool AimHandler::locked = false;

void AimHandler::Initialize(int pwmChip, int xChannel, int yChannel)
{
    std::lock_guard<std::mutex> guard(mtx);

    if (m_initialized) {
        Logger::Info("AimHandler already initialized");
        return;
    }

    XServoPtr = new ServoHandler(pwmChip, xChannel, 50.0);
    YServoPtr = new ServoHandler(pwmChip, yChannel, 50.0);
    m_initialized = true;
}

void AimHandler::CheckIfInitialized(std::string methodName)
{
    if (!m_initialized) {
        std::string msg = "AimHandler not initialized, but " + methodName + " was called";
        Logger::Error(msg);
        throw std::runtime_error(msg);
    }
}

bool AimHandler::SetXAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    if (locked) return false;

    CheckIfInitialized(NAMEOF(AimHandler::SetXAngle));
    XServoPtr->SetAngle(angle);
    lastState.first = angle;
    return true;
}

bool AimHandler::SetYAngle(double angle)
{
    std::lock_guard<std::mutex> guard(mtx);
    if (locked) return false;

    CheckIfInitialized(NAMEOF(AimHandler::SetYAngle));
    YServoPtr->SetAngle(angle);
    lastState.second = angle;
    return true;
}

std::string AimHandler::SetAnglePoint(std::pair<double,double> anglePoint)
{
    std::lock_guard<std::mutex> guard(mtx);
    if (locked) throw BadRequestException("Servos locked, cannot move.");

    CheckIfInitialized(NAMEOF(AimHandler::SetAnglePoint));
    XServoPtr->SetAngle(anglePoint.first);
    YServoPtr->SetAngle(anglePoint.second);
    lastState = anglePoint;
    return "Successfully set";
}

void AimHandler::EmergencyDisableAndLock()
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized(NAMEOF(AimHandler::EmergencyDisableAndLock));
    XServoPtr->EmergencyDisableAndLock();
    YServoPtr->EmergencyDisableAndLock();
    locked = true;
}

void AimHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized(NAMEOF(AimHandler::Unlock));
    XServoPtr->Unlock();
    YServoPtr->Unlock();
    locked = false;
}

void AimHandler::Dispose()
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized(NAMEOF(AimHandler::Dispose));
    delete XServoPtr;
    delete YServoPtr;
    XServoPtr = YServoPtr = nullptr;
    m_initialized = false;
}

void AimHandler::RestoreLastState()
{
    std::lock_guard<std::mutex> guard(mtx);
    CheckIfInitialized(NAMEOF(AimHandler::RestoreLastState));
    XServoPtr->SetAngle(lastState.first);
    YServoPtr->SetAngle(lastState.second);
}