#include "LaserHandler.h"
#include "../Logger/Logger.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../DeadLocker/DeadLocker.h"
#include "../ExceptionExtensions/ExceptionExtensions.h"
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
    try
    {
        LaserHandler::Disable();
    }
    catch(...){ /*IGNORED*/ }
    LaserHandler::lock = true;
    Logger::Info("Laser is disabled and locked.");
}

void LaserHandler::Unlock()
{
    std::lock_guard<std::mutex> guard(mtx);

    LaserHandler::lock = false;
    Logger::Info("Laser is unlocked.");
}

std::string LaserHandler::Enable()
{
    std::lock_guard<std::mutex> guard(mtx);
    int laserValue = gpiod_line_get_value(line);
    if(laserValue == 0)
    {
        if (!lock)
        {
            gpiod_line_set_value(line, 1);
            return "Laser enabled.";
        }
        throw BadRequestException("Laser is locked due to emergency => cannot enable.");   
    }
    else if(laserValue == 1)
    {
        throw BadRequestException("Laser is enabled already.");
    }
    throw std::runtime_error("Laser is not initialized properly");
}

std::string LaserHandler::Disable()
{
    std::lock_guard<std::mutex> guard(mtx);
    int laserValue = gpiod_line_get_value(line);
    if(laserValue == 1)
    {
        gpiod_line_set_value(line, 0);
        if (!lock)
        {
            return "Laser disabled.";
        }
        throw BadRequestException("Laser is locked due to emergency, but was disabled anyway for safety");   
    }
    else if(laserValue == 0)
    {
        throw BadRequestException("Laser is disabled already.");
    }
    throw std::runtime_error("Laser is not initialized properly");
}

std::string LaserHandler::GetStatus()
{
    std::string response;
    int laserValue = gpiod_line_get_value(line);
    if(!initialized)
    {
        return "Uninitialized";
    }
    switch (laserValue)
    {
    case 1:
        response = "Enabled";
        break;
    case 0:
        response = "Disabled";
        break;
    default:
        response = "Error";
        break;
    }
    std::string reasons;
    for(std::string el : DeadLocker::lockReasons)
    {
        reasons += " " + el + " ";
    }
    response = lock? response + " (Locked due to an emergency)" + reasons : response;
    return response;
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
