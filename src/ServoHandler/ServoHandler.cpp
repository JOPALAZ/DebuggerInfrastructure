#include "ServoHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"

#include <gpiod.h>
#include <stdexcept>
#include <chrono>
#include <thread>

// Default constructor
ServoHandler::ServoHandler()
{
    // Nothing to do here; actual setup is done in Initialize()
}

// Destructor
ServoHandler::~ServoHandler()
{
    Dispose();
}

void ServoHandler::Initialize(gpiod_line* line, double frequency)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_initialized) {
        Logger::Info("ServoHandler is already initialized. Checking consistency...");

        // If it's already initialized on the same pin, do nothing
        if (m_line == line) {
            Logger::Info("ServoHandler is already using the same pin. Doing nothing.");
            return;
        }

        // Otherwise, consider it an error because the class is already initialized on a different pin
        std::runtime_error ex(
            "ServoHandler was already initialized with a different GPIO pin!");
        Logger::Critical(ex.what());
        throw ex;
    }

    // Store parameters
    m_line      = line;
    m_frequency = frequency;

    // Ensure the pin is LOW initially
    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }
    m_angle.store(0.0);
    m_locked = false; // Unlock.

    // Start the thread for generating the PWM signal
    m_running = true;
    m_pwmThread = std::thread(&ServoHandler::pwmLoop, this);

    m_initialized = true;
    Logger::Info("ServoHandler initialized successfully.");
}

void ServoHandler::SetAngle(double newAngle)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_initialized) {
        Logger::Info("SetAngle called, but ServoHandler not initialized.");
        return;
    }
    if (m_locked) {
        Logger::Info("SetAngle called, but servo is locked.");
        return;
    }

    // Clamp the angle to [0..180]
    if (newAngle < 0.0)   newAngle = 0.0;
    if (newAngle > 180.0) newAngle = 180.0;

    m_angle.store(newAngle);

    Logger::Info("Servo angle set to " + std::to_string(newAngle) + " degrees.");
}

void ServoHandler::EmergencyDisableAndLock()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_initialized) {
        return;
    }

    // Set angle to 0
    m_angle.store(0.0);

    Logger::Info("Emergency disabling servo (angle=0).");
    // Give the PWM thread a little time to actually apply 0
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Lock the servo
    m_locked = true;
    Logger::Info("Servo is locked after emergency disable.");
}

void ServoHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_initialized) {
        Logger::Info("Unlock called, but ServoHandler not initialized.");
        return;
    }

    m_locked = false;
    Logger::Info("Servo is unlocked.");
}

void ServoHandler::Dispose()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_initialized) {
        // Already disposed
        return;
    }

    // Perform an emergency disable first
    EmergencyDisableAndLock();

    // Stop the thread
    m_running = false;
    if (m_pwmThread.joinable()) {
        m_pwmThread.join();
    }

    // Release GPIO resources
    if (m_line) {
        GPIOHandler::ReleaseLine(m_line);
        m_line = nullptr;
    }

    m_initialized = false;
    Logger::Info("ServoHandler disposed.");
}

bool ServoHandler::IsLocked() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_locked;
}

void ServoHandler::pwmLoop()
{
    using namespace std::chrono;

    // Period (seconds) = 1 / frequency
    // For SG90, typically about 20ms (50Hz)
    const double periodSec = 1.0 / m_frequency;
    const double periodUs  = periodSec * 1'000'000.0;

    while (m_running) {
        double currentAngle = m_angle.load(); // Atomic read of the angle

        // Map angle [0..180] to pulse width [1.0..2.0] ms (adjust if necessary)
        double pulseWidthMs = 1.0 + (currentAngle / 180.0);
        double pulseWidthUs = pulseWidthMs * 1000.0;

        // Drive the GPIO line HIGH
        if (m_line) {
            gpiod_line_set_value(m_line, 1);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(pulseWidthUs)));

        // Then drive it LOW for the remainder of the period
        double remainderUs = periodUs - pulseWidthUs;
        if (remainderUs < 0) {
            remainderUs = 0; // In case the angle or frequency is set incorrectly
        }

        if (m_line) {
            gpiod_line_set_value(m_line, 0);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(remainderUs)));
    }

    // When the loop ends, make sure to drive LOW for safety
    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }
}
