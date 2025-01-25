#include "ServoHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"

#include <gpiod.h>
#include <stdexcept>
#include <chrono>
#include <thread>

// Pulse width range per specification (500→2500 μs)
static constexpr double kMinPulseWidthUs = 500.0;
static constexpr double kMaxPulseWidthUs = 2500.0;
// Default angular range for the servo: 0..180 degrees
static constexpr double kMinAngle = 0.0;
static constexpr double kMaxAngle = 180.0;

ServoHandler::ServoHandler()
{
    // Default constructor
}

ServoHandler::~ServoHandler()
{
    Dispose();
}

void ServoHandler::Initialize(gpiod_line* line, double frequency)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_initialized) {
        Logger::Info("ServoHandler is already initialized. Checking consistency...");

        // If it's the same pin, do nothing
        if (m_line == line) {
            Logger::Info("ServoHandler is already using the same pin. Doing nothing.");
            return;
        }

        // Otherwise, throw an error
        std::runtime_error ex("ServoHandler was already initialized with a different GPIO pin!");
        Logger::Critical(ex.what());
        throw ex;
    }

    // Store parameters
    m_line      = line;
    m_frequency = frequency;

    // Drive the line LOW initially
    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }

    // By default, set angle to 0°
    m_angle.store(0.0);
    m_locked = false; // Unlock by default

    // Start the PWM generation thread
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

    // Clamp angle to [0..180];
    if (newAngle < kMinAngle) newAngle = kMinAngle;
    if (newAngle > kMaxAngle) newAngle = kMaxAngle;

    m_angle.store(newAngle);

    Logger::Info("Servo angle set to " + std::to_string(newAngle) + " degrees.");
}

void ServoHandler::EmergencyDisableAndLock()
{
    if (!m_initialized) {
        return;
    }

    // Immediately move to 0°
    m_angle.store(0.0);

    Logger::Info("Emergency disabling servo (angle=0).");
    // Short delay to let the servo actually move to 0°
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
    if (!m_initialized) {
        // Already disposed
        return;
    }

    // Perform an emergency disable for safety
    EmergencyDisableAndLock();

    // Stop the PWM thread
    m_running = false;
    if (m_pwmThread.joinable()) {
        m_pwmThread.join();
    }

    // Release the GPIO line
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
    // For many hobby servos, 50 Hz => 20 ms period
    const double periodSec = 1.0 / m_frequency;
    const double periodUs  = periodSec * 1'000'000.0;

    while (m_running) {
        double currentAngle = m_angle.load(); // Atomic read of current angle

        // Map angle [0..180] to pulse width [500..2500] µs
        double pulseWidthUs = kMinPulseWidthUs
                            + (currentAngle / (kMaxAngle - kMinAngle))
                            * (kMaxPulseWidthUs - kMinPulseWidthUs);

        // Drive GPIO HIGH for the pulse width
        if (m_line) {
            gpiod_line_set_value(m_line, 1);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(pulseWidthUs)));

        // Drive LOW for the remainder of the period
        double remainderUs = periodUs - pulseWidthUs;
        if (remainderUs < 0) {
            remainderUs = 0; // Safety check in case of invalid config
        }

        if (m_line) {
            gpiod_line_set_value(m_line, 0);
        }
        std::this_thread::sleep_for(microseconds(static_cast<long>(remainderUs)));
    }

    // At the end of the loop (Dispose called), set pin LOW for safety
    if (m_line) {
        gpiod_line_set_value(m_line, 0);
    }
}
