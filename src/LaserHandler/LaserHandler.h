#pragma once

#include <mutex>
#include <gpiod.hpp>

/**
 * @class LaserHandler
 * @brief This class controls a laser connected via a GPIO pin on the host device.
 *
 * The LaserHandler manages initialization, enabling/disabling, and locking/unlocking
 * of the laser. It uses static methods and shared static data protected by a mutex to
 * ensure thread safety when accessed by multiple threads.
 */
class LaserHandler
{
private:
    /**
     * @brief Mutex to protect static state in multi-threaded environments.
     */
    static std::mutex mtx;

    /**
     * @brief Represents whether the laser is locked or not.
     *
     * When locked, @ref Enable() does nothing (the laser remains disabled).
     */
    static bool lock;

    /**
     * @brief GPIO pin number used to control the laser.
     */
    static unsigned int GPIOpin;

    /**
     * @brief Name of the GPIO chip (as recognized by libgpiod).
     */
    static const char* chipname;

    /**
     * @brief Pointer to the opened gpiod chip.
     */
    static gpiod_chip* chip;

    /**
     * @brief Pointer to the gpiod line object controlling the laser pin.
     */
    static gpiod_line* line;

    /**
     * @brief Flag indicating whether the handler has been initialized.
     */
    static bool initialized;

public:
    /**
     * @brief Initializes the LaserHandler with the specified GPIO pin.
     *
     * Opens the GPIO chip, requests the given pin for output, and sets
     * the laser to a disabled and unlocked state.
     *
     * @param GPIOpin The pin number on the GPIO chip to be used for laser control.
     * @throw std::runtime_error If the chip or line fails to open, or if line request fails.
     */
    static void Initialize(const unsigned int GPIOpin = 16);

    /**
     * @brief Disables the laser immediately and locks it (cannot be enabled until unlocked).
     *
     * Calls @ref Disable() internally, then sets @ref lock to true.
     */
    static void EmergencyDisableAndLock();

    /**
     * @brief Unlocks the laser, allowing it to be enabled by @ref Enable().
     */
    static void Unlock();

    /**
     * @brief Enables the laser if it is unlocked.
     *
     * If @ref lock is true, enabling is ignored.
     */
    static void Enable();

    /**
     * @brief Disables the laser (regardless of the lock state).
     */
    static void Disable();

    /**
     * @brief Disposes of the laser resources safely, disabling and locking the laser.
     *
     * Closes the GPIO chip and resets internal state. After disposal, the handler can be
     * re-initialized via @ref Initialize() if needed.
     */
    static void Dispose();
};
