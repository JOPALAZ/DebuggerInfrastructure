#include "ServoHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"

#include <gpiod.h>
#include <stdexcept>
#include <chrono>
#include <thread>

// Defines the minimum and maximum pulse widths, in microseconds
static constexpr double kMinPulseWidthUs = 500.0;
static constexpr double kMaxPulseWidthUs = 2500.0;

// Defines the minimum and maximum angles for the servo in degrees
static constexpr double kMinAngle = 0.0;
static constexpr double kMaxAngle = 180.0;

ServoHandler::ServoHandler(gpiod_line* line, double frequency)
    : m_line(line)
    , m_frequency(frequency)
    , m_running(true)
    , m_locked(false)
    , m_stopThread(false)
{
    // Sets the initial state of the servo pin to LOW
    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }

    // Starts with an angle of 0°
    m_angle.store(0.0);

    Logger::Info("ServoHandler constructed and initialized.");
}

ServoHandler::~ServoHandler()
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_running = false;
        // Moves servo to 0°, small delay if needed
        m_angle.store(0.0);
        m_locked = true;
    }

    stopPulseThreadLocked();

    if (m_line) {
        GPIOHandler::ReleaseLine(m_line);
        m_line = nullptr;
    }

    Logger::Info("ServoHandler destroyed and resources released.");
}

void ServoHandler::SetAngle(double newAngle)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_locked) {
        Logger::Info("SetAngle called, but the servo is locked.");
        return;
    }

    if (newAngle < kMinAngle) newAngle = kMinAngle;
    if (newAngle > kMaxAngle) newAngle = kMaxAngle;

    m_angle.store(newAngle);
    Logger::Info("Servo angle set to " + std::to_string(newAngle) + " degrees.");

    stopPulseThreadLocked();
    m_stopThread = false;
    m_pulseThread = std::thread(&ServoHandler::sendPulseTrain, this, newAngle, m_frequency, 500);
}

void ServoHandler::EmergencyDisableAndLock()
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_angle.store(0.0);
        Logger::Info("Emergency disabling servo (angle=0).");
        m_locked = true;
    }

    // Brief pause to allow the servo to move
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Logger::Info("Servo is locked after emergency disable.");
}

void ServoHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_locked = false;
    Logger::Info("Servo is unlocked.");
}

bool ServoHandler::IsLocked() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_locked;
}

void ServoHandler::sendPulseTrain(double targetAngle, double frequency, int durationMs)
{
    using namespace std::chrono;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int cycleCount = static_cast<int>((frequency * durationMs) / 1000.0);
    double periodUs = (1.0 / frequency) * 1'000'000.0;

    for (int i = 0; i < cycleCount; ++i) {
        {
            if (!m_running || m_stopThread) {
                break;
            }
        }

        double pulseWidthUs = kMinPulseWidthUs
                            + (targetAngle / (kMaxAngle - kMinAngle))
                            * (kMaxPulseWidthUs - kMinPulseWidthUs);

        if (m_line) {
            gpiod_line_set_value(m_line, 1);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(pulseWidthUs)));

        double remainderUs = periodUs - pulseWidthUs;
        if (remainderUs < 0) {
            remainderUs = 0;
        }

        if (m_line) {
            gpiod_line_set_value(m_line, 0);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(remainderUs)));
    }

    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }

    Logger::Info("sendPulseTrain finished for angle=" + std::to_string(targetAngle));
}

void ServoHandler::stopPulseThreadLocked()
{
    if (m_pulseThread.joinable()) {
        m_stopThread = true;
        m_pulseThread.join();
        m_stopThread = false;
    }
}
