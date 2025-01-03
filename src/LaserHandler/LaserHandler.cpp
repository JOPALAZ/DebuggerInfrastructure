#include "LaserHandler.h"
#include "../Logger/Logger.h"
#include <gpiod.h>
#include <stdexcept>
#include <string>

// Define the static member variables
std::mutex       LaserHandler::mtx;
bool             LaserHandler::lock       = true;
unsigned int     LaserHandler::GPIOpin    = 0;
const char*      LaserHandler::chipname   = "gpiochip0";
gpiod_chip*      LaserHandler::chip       = nullptr;
gpiod_line*      LaserHandler::line       = nullptr;
bool             LaserHandler::initialized = false;

void LaserHandler::Initialize(unsigned int GPIOpin)
{
    // Acquire the mutex to protect shared state
    std::lock_guard<std::mutex> guard(mtx);

    if (initialized)
    {
        Logger::Info("LaserHandler is already initialized.");
        if (GPIOpin != LaserHandler::GPIOpin)
        {
            std::runtime_error ex(
                "Initialization requested with a different GPIO pin, cannot proceed."
            );
            Logger::Critical(ex.what());
            throw ex;
        }
        return;
    }

    LaserHandler::GPIOpin = GPIOpin;

    // Open the GPIO chip
    chip = gpiod_chip_open_by_name(chipname);
    if (!chip)
    {
        std::runtime_error ex("Failed to open GPIO chip: " + std::string(chipname));
        Logger::Critical(ex.what());
        throw ex;
    }

    // Get the GPIO line
    line = gpiod_chip_get_line(chip, GPIOpin);
    if (!line)
    {
        std::runtime_error ex("Failed to get GPIO line: " + std::to_string(GPIOpin));
        Logger::Critical(ex.what());
        gpiod_chip_close(chip);
        chip = nullptr;
        throw ex;
    }

    // Request the line as output
    if (gpiod_line_request_output(line, "LASER_gpio", 0) < 0)
    {
        std::runtime_error ex("Failed to request line as output for the laser.");
        Logger::Critical(ex.what());
        gpiod_chip_close(chip);
        chip = nullptr;
        line = nullptr;
        throw ex;
    }

    // Initially unlock and disable (no extra locking here)
    Unlock_Internal();
    Disable_Internal();

    LaserHandler::initialized = true;
    Logger::Info("LaserHandler initialized successfully.");
}

void LaserHandler::Dispose()
{
    // Acquire the mutex to protect shared state
    std::lock_guard<std::mutex> guard(mtx);

    // Perform emergency disable and lock for safety
    EmergencyDisableAndLock_Internal();

    // Close GPIO resources
    if (chip)
    {
        gpiod_chip_close(chip);
        chip = nullptr;
    }
    line = nullptr;
    initialized = false;

    Logger::Info("Disposed of LaserHandler resources.");
}

void LaserHandler::EmergencyDisableAndLock()
{
    // Acquire the mutex, then call the internal method
    std::lock_guard<std::mutex> guard(mtx);
    EmergencyDisableAndLock_Internal();
}

void LaserHandler::Unlock()
{
    // Acquire the mutex, then call the internal method
    std::lock_guard<std::mutex> guard(mtx);
    Unlock_Internal();
}

void LaserHandler::Enable()
{
    // Acquire the mutex, then call the internal method
    std::lock_guard<std::mutex> guard(mtx);
    Enable_Internal();
}

void LaserHandler::Disable()
{
    // Acquire the mutex, then call the internal method
    std::lock_guard<std::mutex> guard(mtx);
    Disable_Internal();
}

// ----------------------- Internal methods (no mutex) ----------------------- //

void LaserHandler::EmergencyDisableAndLock_Internal()
{
    // Disable the laser and set lock to true (no extra locking here)
    Disable_Internal();
    lock = true;
    Logger::Info("Laser is disabled and locked (emergency).");
}

void LaserHandler::Unlock_Internal()
{
    lock = false;
    Logger::Info("Laser is unlocked.");
}

void LaserHandler::Enable_Internal()
{
    if (!lock)
    {
        if (line != nullptr)
        {
            gpiod_line_set_value(line, 1);
            Logger::Info("Laser enabled.");
        }
        else
        {
            Logger::Warning("Enable called but line is not initialized.");
        }
    }
    else
    {
        Logger::Info("Attempted to enable laser while locked.");
    }
}

void LaserHandler::Disable_Internal()
{
    if (line != nullptr)
    {
        gpiod_line_set_value(line, 0);
        Logger::Info("Laser disabled.");
    }
    else
    {
        Logger::Info("Disable called but line is not initialized.");
    }
}
