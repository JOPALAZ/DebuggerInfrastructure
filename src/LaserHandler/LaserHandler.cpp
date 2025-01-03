#include "LaserHandler.h"
#include "../Logger/Logger.h"
#include <gpiod.h>
#include <stdexcept>
#include <string>

// Define the static member variables
std::mutex LaserHandler::mtx;
bool LaserHandler::lock = true;
unsigned int LaserHandler::GPIOpin = 0;
const char* LaserHandler::chipname = "gpiochip0";
gpiod_chip* LaserHandler::chip = nullptr;
gpiod_line* LaserHandler::line = nullptr;
bool LaserHandler::initialized = false;

void LaserHandler::Initialize(const unsigned int GPIOpin)
{
    // Protect shared data/state
    std::lock_guard<std::mutex> guard(mtx);

    if (initialized)
    {
        Logger::Info("LaserHandler is already initialized.");
        // Ensure the requested pin matches the already-initialized pin
        if (GPIOpin != LaserHandler::GPIOpin)
        {
            std::runtime_error ex(
                "Initialization requested for laser handler with different GPIO pin, cannot proceed.");
            Logger::Critical(ex.what());
            throw ex;
        }
        return;
    }

    LaserHandler::GPIOpin = GPIOpin;
    
    // Open GPIO chip
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
        std::runtime_error ex("Failed to request line as output for the laser");
        Logger::Critical(ex.what());
        gpiod_chip_close(chip);
        chip = nullptr;
        line = nullptr;
        throw ex;
    }
    
    // Unlock and disable the laser initially
    LaserHandler::lock = false;
    gpiod_line_set_value(line, 0);

    LaserHandler::initialized = true;
    Logger::Info("LaserHandler initialized successfully.");
}

void LaserHandler::EmergencyDisableAndLock()
{
    std::lock_guard<std::mutex> guard(mtx);

    // Disable laser and lock it
    LaserHandler::Disable();
    LaserHandler::lock = true;
    Logger::Info("Laser is disabled and locked.");
}

void LaserHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(mtx);

    LaserHandler::lock = false;
    Logger::Info("Laser is unlocked.");
}

void LaserHandler::Enable()
{
    std::lock_guard<std::mutex> guard(mtx);

    if (!lock)
    {
        gpiod_line_set_value(line, 1);
        Logger::Info("Laser enabled.");
    }
    else
    {
        Logger::Info("Attempted to enable laser while locked.");
    }
}

void LaserHandler::Disable()
{
    std::lock_guard<std::mutex> guard(mtx);

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

void LaserHandler::Dispose()
{
    std::lock_guard<std::mutex> guard(mtx);

    // Disable and lock for safety
    LaserHandler::EmergencyDisableAndLock();

    // Close chip resources
    if (chip != nullptr)
    {
        gpiod_chip_close(chip);
        chip = nullptr;
    }
    line = nullptr;

    initialized = false;
    Logger::Info("Disposed of LaserHandler resources.");
}
