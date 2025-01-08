#pragma once

#include <string>

// Forward declarations to avoid pulling in <gpiod.h> here
struct gpiod_chip;
struct gpiod_line;

/**
 * @brief A static helper class that provides basic GPIO handling via libgpiod.
 *
 * This version stores a static pointer to a gpiod_chip, created during
 * Initialize() and released by Dispose(). Other methods access that static pointer.
 */
class GPIOHandler
{
public:
    /**
     * @brief Initializes the GPIOHandler by opening a chip with the given name.
     *        May be called only once; subsequent calls with the same chip name do nothing.
     *        If a different name is passed, it throws std::runtime_error.
     * @param chipName The name of the GPIO chip, e.g. "gpiochip0".
     */
    static void Initialize(const std::string &chipName);

    /**
     * @brief Closes the opened GPIO chip and resets the internal state.
     *        Safe to call multiple times; calling it when uninitialized does nothing.
     */
    static void Dispose();

    /**
     * @brief Retrieves a GPIO line from the internally stored gpiod_chip.
     * @param lineOffset The GPIO line offset (pin).
     * @return A pointer to the requested gpiod_line (throws std::runtime_error on failure).
     */
    static gpiod_line* GetLine(unsigned int lineOffset);

    /**
     * @brief Requests the specified gpiod_line as output with an initial value.
     * @param line        The gpiod_line pointer (must not be null).
     * @param consumer    A name for the consumer (e.g., "SERVO_gpio").
     * @param defaultVal  The initial GPIO output value (0 = LOW, 1 = HIGH).
     * @throws std::runtime_error on failure.
     */
    static void RequestLineOutput(gpiod_line *line, const std::string &consumer, int defaultVal = 0);

    /**
     * @brief Releases a gpiod_line (sets it to nullptr afterwards).
     * @param line Reference to the pointer to gpiod_line. After this call, line becomes nullptr.
     */
    static void ReleaseLine(gpiod_line *&line);
    
private:
    // Delete all constructors and operators to enforce static-only usage.
    GPIOHandler() = delete;
    ~GPIOHandler() = delete;
    GPIOHandler(const GPIOHandler&) = delete;
    GPIOHandler& operator=(const GPIOHandler&) = delete;

    /**
     * @brief A static pointer to the opened gpiod_chip.
     */
    static gpiod_chip *chip;

    /**
     * @brief Whether the static chip has been successfully initialized.
     */
    static bool initialized;

    /**
     * @brief Stores the current chip name (used for consistency checks).
     */
    static std::string chipName;
};
