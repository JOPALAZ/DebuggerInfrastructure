#include "LaserHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"
#include <gpiod.h>
#include <stdexcept>
#include <string>

// Define the static member variables
std::mutex LaserHandler::mtx;
bool LaserHandler::lock = true;
gpiod_line* LaserHandler::line = nullptr;
bool LaserHandler::initialized = false;

void LaserHandler::Initialize(gpiod_line* line)
{
    // Protect shared data/state
    std::lock_guard<std::mutex> guard(mtx);

    if (initialized)
    {
        Logger::Info("LaserHandler is already initialized.");
        // Ensure the requested pin matches the already-initialized pin
        if (line != LaserHandler::line)
        {
            std::runtime_error ex(
                "Initialization requested for laser handler with different GPIO pin, cannot proceed.");
            Logger::Critical(ex.what());
            throw ex;
        }
        return;
    }

    LaserHandler::line = line;

    // Unlock and disable the laser initially
    LaserHandler::lock = false;
    gpiod_line_set_value(line, 0);

    LaserHandler::initialized = true;
    Logger::Info("LaserHandler initialized successfully.");
}

void LaserHandler::EmergencyDisableAndLock()
{
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
    // Disable and lock for safety
    LaserHandler::EmergencyDisableAndLock();

    // Close chip resources
    if (line != nullptr)
    {
        GPIOHandler::ReleaseLine(line);
    }
    line = nullptr;

    initialized = false;
    Logger::Info("Disposed of LaserHandler resources.");
}
