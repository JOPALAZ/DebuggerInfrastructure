#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

struct gpiod_line;

/**
 * @brief An instance-based (non-static) class for controlling a servo motor.
 *        Allows multiple independent servo objects on different pins.
 */
class ServoHandler {
public:
    /**
     * @brief Constructor. Note that you can initialize (export the pin and start
     *        the PWM thread) either here or in a separate Initialize() method.
     *        In this example, we use a separate Initialize() method.
     */
    ServoHandler();

    /**
     * @brief Destructor. Stops the PWM thread (if running) and releases resources.
     */
    ~ServoHandler();

    /**
     * @brief Initializes the servo on a given GPIO pin (via gpiod_line).
     * @param line      Pointer to an already opened gpiod_line (pin).
     * @param frequency PWM frequency (usually ~50 Hz for SG90).
     */
    void Initialize(gpiod_line* line, double frequency = 50.0);

    /**
     * @brief Sets the servo angle (if not locked).
     *        The angle is clamped to the range [0..180].
     */
    void SetAngle(double angle);

    /**
     * @brief Emergency disable (sets angle = 0) and lock.
     *        After this, SetAngle calls are ignored until Unlock() is called.
     */
    void EmergencyDisableAndLock();

    /**
     * @brief Unlocks the servo so SetAngle can be used again.
     */
    void Unlock();

    /**
     * @brief Stops the PWM thread and releases GPIO resources.
     *        After calling Dispose(), you need to call Initialize() again
     *        if you want to reuse this servo.
     */
    void Dispose();

    /**
     * @brief Returns whether the servo is currently locked.
     */
    bool IsLocked() const;

private:
    /**
     * @brief The function that runs in a separate thread, generating the PWM signal.
     */
    void pwmLoop();

private:
    // A mutex to protect object fields from concurrent access
    mutable std::mutex m_mutex;  

    // State flags
    bool   m_locked      = true;   ///< If true, SetAngle is ignored
    bool   m_initialized = false;  ///< Whether the servo is initialized (PWM thread running)
    bool   m_running     = false;  ///< Whether the PWM thread is currently running

    // Current servo angle (read periodically by pwmLoop)
    std::atomic<double> m_angle{0.0};

    // PWM parameters
    double      m_frequency = 50.0; ///< PWM frequency in Hz

    // GPIO line (libgpiod)
    gpiod_line* m_line = nullptr;  

    // The thread in which the PWM is generated
    std::thread m_pwmThread;
};
