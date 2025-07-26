#include "AimHandler.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"
#include "../ExceptionExtensions/ExceptionExtensions.h"
#include "../LaserHandler/LaserHandler.h"
#include "../DbHandler/DbHandler.h"

namespace DebuggerInfrastructure
{
    std::mutex AimHandler::mtx;
    ServoHandler* AimHandler::XServoPtr = nullptr;
    ServoHandler* AimHandler::YServoPtr = nullptr;
    std::pair<double, double> AimHandler::defaultState = {0,0};
    bool AimHandler::m_initialized = false;
    bool AimHandler::locked = false;
    bool AimHandler::calibrationActive = false;
    CalibrationSettings AimHandler::calbration;
    std::chrono::_V2::system_clock::time_point AimHandler::lastShoot;

    void AimHandler::Initialize(int pwmChip, int xChannel, int yChannel, std::string calibrationPath)
    {
        std::lock_guard<std::mutex> guard(mtx);

        if (m_initialized) {
            Logger::Info("AimHandler already initialized");
            return;
        }
        calbration = ExternalConfigsHelper::getOrCreateCalibrationSettings(calibrationPath);
        XServoPtr = new ServoHandler(pwmChip, xChannel, 50.0);
        YServoPtr = new ServoHandler(pwmChip, yChannel, 50.0);
        m_initialized = true;
    }

    std::chrono::_V2::system_clock::time_point AimHandler::GetLastShoot()
    {
        return AimHandler::lastShoot;
    }

    std::string AimHandler::ShootAt(std::pair<double, double> point)
    {
        std::string response = fmt::format("Shooting at X({}) Y({})\t", point.first, point.second);
        if(!IsLaserEnabled())
        {
            response += LaserHandler::Enable() + "\t";
        }
        response += SetPoint(point);
        lastShoot = std::chrono::_V2::system_clock::now();
        return response;
    }

    bool AimHandler::IsLaserEnabled()
    {
        return LaserHandler::GetStatus()=="Enabled";
    }




    std::string AimHandler::SetPoint(std::pair<double, double> point)
    {
        if (locked) throw BadRequestException("Servos locked, cannot move.");

        if (point.first < 0.0 || point.first > 1.0 || point.second < 0.0 || point.second > 1.0)
            throw BadRequestException("Invalid point. Must be in range [0, 1].");

        point.second = 1.0 - point.second;    

        double angleX;
        if (point.first < 0.5) {
            angleX = AimHandler::calbration.calibrationX.first + 
                    (AimHandler::calbration.calibrationCenter.first - AimHandler::calbration.calibrationX.first) * (point.first / 0.5);
        } else {
            angleX = AimHandler::calbration.calibrationCenter.first + 
                    (AimHandler::calbration.calibrationX.second - AimHandler::calbration.calibrationCenter.first) * ((point.first - 0.5) / 0.5);
        }

        double angleY;
        if (point.second < 0.5) {
            angleY = AimHandler::calbration.calibrationY.first + 
                    (AimHandler::calbration.calibrationCenter.second - AimHandler::calbration.calibrationY.first) * (point.second / 0.5);
        } else {
            angleY = AimHandler::calbration.calibrationCenter.second + 
                    (AimHandler::calbration.calibrationY.second - AimHandler::calbration.calibrationCenter.second) * ((point.second - 0.5) / 0.5);
        }
        
        return SetAnglePoint({angleX, angleY});
    }

    std::string AimHandler::Disarm()
    {
        std::string response = fmt::format("Disarming\t");
        response += LaserHandler::Disable() + "\t";
        response += SetAnglePoint(defaultState);
        return response;
    }

    std::string AimHandler::EnableCalibration()
    {
        std::string response = fmt::format("Enabling calibration\t");
        if(!IsLaserEnabled())
        {
            response += LaserHandler::Enable() + "\t";
        }
        calibrationActive = true;
        DbHandler::InsertDataNow(CALIBRATIONSTART, NAMEOF(RESTApi), "System entered the calibration mode.");
        return response;
    }

    std::string AimHandler::DisableCalibration()
    {
        std::string response = fmt::format("Disabling calibration\t");
        if(IsLaserEnabled())
        {
            response += LaserHandler::Disable() + "\t";
        }
        if(calibrationActive == true)
        {
            calibrationActive = false;
            DbHandler::InsertDataNow(CALIBRATIONEND, NAMEOF(RESTApi), "System exited the calibration mode.");
        }
        return response;
    }

    bool AimHandler::IsCalibrationEnabled()
    {
        return calibrationActive;
    }

    std::string AimHandler::SetDefaultState(std::pair<double, double> point)
    {
        std::string response = fmt::format("Setting default state to X({}) Y({})\t", point.first, point.second);
        defaultState = point;
        response += SetAnglePoint(defaultState);
        return response;
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
        defaultState.first = angle;
        return true;
    }

    bool AimHandler::SetYAngle(double angle)
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (locked) return false;

        CheckIfInitialized(NAMEOF(AimHandler::SetYAngle));
        YServoPtr->SetAngle(angle);
        defaultState.second = angle;
        return true;
    }

    std::string AimHandler::SetAnglePoint(std::pair<double,double> anglePoint)
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (locked) throw BadRequestException("Servos locked, cannot move.");

        CheckIfInitialized(NAMEOF(AimHandler::SetAnglePoint));
        XServoPtr->SetAngle(anglePoint.first);
        YServoPtr->SetAngle(anglePoint.second);
        return fmt::format("Successfully set angle point X({}) Y({})", anglePoint.first, anglePoint.second);
    }

    void AimHandler::EmergencyDisableAndLock()
    {
        std::lock_guard<std::mutex> guard(mtx);
        LaserHandler::EmergencyDisableAndLock();
        CheckIfInitialized(NAMEOF(AimHandler::EmergencyDisableAndLock));
        DisableCalibration();
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
        XServoPtr->SetAngle(defaultState.first);
        YServoPtr->SetAngle(defaultState.second);
    }
}