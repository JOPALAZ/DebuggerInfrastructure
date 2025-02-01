#pragma once

#include <atomic>
#include <thread>
#include <mutex>

// Forward declaration for GPIO line structure
struct gpiod_line;

/*
 * Manages a servo motor using an asynchronous pulse approach.
 * The servo is driven for a short duration when the angle changes,
 * rather than being driven continuously.
 */
class ServoHandler
{
public:
    /*
     * Constructs the servo handler with the specified GPIO line and PWM frequency.
     * The line is set to LOW initially, and the handler is prepared for angle updates.
     */
    ServoHandler(gpiod_line* line, double frequency);

    /*
     * Destroys the handler by stopping any running pulse thread,
     * disabling the servo, and releasing the GPIO line.
     */
    ~ServoHandler();

    /*
     * Requests a new target angle for the servo.
     * This starts a thread that sends PWM pulses for a short duration (e.g. 500 ms).
     * Any previously running thread is stopped before starting a new one.
     */
    void SetAngle(double newAngle);

    /*
     * Immediately moves the servo to 0° and locks the servo to prevent further angle changes.
     * Designed for emergency situations or quick shutdown.
     */
    void EmergencyDisableAndLock();

    /*
     * Unlocks the servo so new angles can be applied.
     */
    void Unlock();

    /*
     * Indicates whether the servo is currently locked.
     */
    bool IsLocked() const;

private:
    /*
     * Sends a train of pulses corresponding to the specified angle, frequency,
     * and total duration (in milliseconds). Runs in a separate thread.
     */
    void sendPulseTrain(double targetAngle, double frequency, int durationMs);

    /*
     * Stops a running pulse thread if it is still active.
     */
    void stopPulseThreadLocked();

    // Pointer to the GPIO line used for servo control
    gpiod_line* m_line;

    // Frequency for PWM pulses (commonly 50 Hz for many hobby servos)
    double m_frequency;

    // Indicates whether the handler should continue running
    bool m_running;

    // Flag that locks the servo angle from further updates
    bool m_locked;

    // Atomic flag used to request a stop for the pulse sending thread
    std::atomic<bool> m_stopThread;

    // Stores the last requested angle in degrees
    std::atomic<double> m_angle;

    // The thread object used for generating pulses asynchronously
    std::thread m_pulseThread;

    // Mutex used to protect shared state across threads
    mutable std::mutex m_mutex;
};
