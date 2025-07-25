#include "ServoHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"

#include <gpiod.h>
#include <stdexcept>
#include <chrono>
#include <thread>
namespace DebuggerInfrastructure
    {
    ServoHandler::ServoHandler(int pwmChip, int pwmChannel, double frequency)
        : m_pwmChip(pwmChip)
        , m_pwmChannel(pwmChannel)
        , m_locked(false)
    {
        m_basePath = "/sys/class/pwm/pwmchip" + std::to_string(m_pwmChip);
        writeSysfs(m_basePath + "/export", std::to_string(m_pwmChannel));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        m_basePath += "/pwm" + std::to_string(m_pwmChannel);

        m_periodNs = (1.0 / frequency) * 1e9;
        writeSysfs(m_basePath + "/period", std::to_string(static_cast<long>(m_periodNs)));
        writeSysfs(m_basePath + "/enable", "1");

        SetAngle(0.0);
        Logger::Info("ServoHandler initialized with hardware PWM.");
    }

    ServoHandler::~ServoHandler()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_locked = true;
        }

        writeSysfs(m_basePath + "/enable", "0");

        std::string chipPath = "/sys/class/pwm/pwmchip" + std::to_string(m_pwmChip);
        writeSysfs(chipPath + "/unexport", std::to_string(m_pwmChannel));

        Logger::Info("ServoHandler destroyed and PWM unexported.");
    }

    void ServoHandler::SetAngle(double newAngle)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_locked) {
            Logger::Info("SetAngle ignored: servo is locked.");
            return;
        }

        if (newAngle < kMinAngle) newAngle = kMinAngle;
        if (newAngle > kMaxAngle) newAngle = kMaxAngle;

        double pulseUs = kMinPulseWidthUs +
                        (newAngle / (kMaxAngle - kMinAngle)) *
                        (kMaxPulseWidthUs - kMinPulseWidthUs);
        long dutyNs = static_cast<long>(pulseUs * 1000.0);

        writeSysfs(m_basePath + "/duty_cycle", std::to_string(dutyNs));
        Logger::Info("Servo angle set to " + std::to_string(newAngle) + " degrees.");
    }

    void ServoHandler::EmergencyDisableAndLock()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            writeSysfs(m_basePath + "/duty_cycle",
                    std::to_string(static_cast<long>(kMinPulseWidthUs * 1000.0)));
            m_locked = true;
        }
        Logger::Info("Emergency disable: servo locked at 0 degrees.");
    }

    void ServoHandler::Unlock()
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_locked = false;
        Logger::Info("Servo unlocked.");
    }

    bool ServoHandler::IsLocked()
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_locked;
    }

    void ServoHandler::writeSysfs(const std::string &path, const std::string &value)
    {
        std::ofstream fs(path);
        if (!fs.is_open()) {
            throw std::runtime_error("Failed to open " + path);
        }
        fs << value;
    }
}