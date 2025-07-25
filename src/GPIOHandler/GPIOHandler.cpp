#include "GPIOHandler.h"
#include "../Logger/Logger.h"
#include <gpiod.h>
#include <stdexcept>
#include <iostream>
#include <thread>


namespace DebuggerInfrastructure
{
    // Static member definitions
    gpiod_chip*    GPIOHandler::chip        = nullptr;
    bool           GPIOHandler::initialized = false;
    std::string    GPIOHandler::chipName    = "";
    /**
     * @brief Initializes the GPIO chip if not already initialized.
     */
    void GPIOHandler::Initialize(const std::string &chipName)
    {
        // If already initialized, check if the requested name matches the existing one.
        if (initialized) {
            if (chipName != chipName) {
                throw std::runtime_error(
                    "GPIOHandler is already initialized with a different chip name: " +
                    GPIOHandler::chipName + " vs. " + chipName);
            }
            // If the name is the same, do nothing.
            return;
        }

        // Attempt to open the GPIO chip
        gpiod_chip *chip = gpiod_chip_open_by_name(chipName.c_str());
        if (!chip) {
            throw std::runtime_error("Failed to open GPIO chip: " + chipName);
        }

        // Store into static members
        GPIOHandler::chip           = chip;
        GPIOHandler::chipName       = chipName;
        GPIOHandler::initialized    = true;

        // Simple log message for demonstration
        Logger::Info("GPIOHandler Chip \"{}\" initialized.",chipName);
    }

    /**
     * @brief Closes and disposes of the GPIO chip if initialized.
     */
    void GPIOHandler::Dispose()
    {
        // If not initialized, nothing to do
        if (!initialized) {
            return;
        }

        // Close the chip
        if (chip) {
            gpiod_chip_close(chip);
            chip = nullptr;
        }

        chipName.clear();
        initialized = false;

        Logger::Info("Disposed of GPIOHandler.");
    }

    /**
     * @brief Gets the specified line offset from the static chip pointer.
     */
    gpiod_line* GPIOHandler::GetLine(unsigned int lineOffset)
    {
        if (!initialized || !chip) {
            throw std::runtime_error("GPIOHandler::GetLine() called but chip is not initialized.");
        }

        gpiod_line *line = gpiod_chip_get_line(chip, lineOffset);
        if (!line) {
            throw std::runtime_error(
                "Failed to get GPIO line " + std::to_string(lineOffset));
        }
        return line;
    }

    /**
     * @brief Requests the line as output using libgpiod.
     */
    void GPIOHandler::RequestLineOutput(gpiod_line *line,
                                        const std::string &consumer,
                                        int defaultVal /*=0*/)
    {
        if (!initialized || !chip) {
            throw std::runtime_error("GPIOHandler::RequestLineOutput() called but chip is not initialized.");
        }
        if (!line) {
            throw std::runtime_error("GPIOHandler::RequestLineOutput() called with null line pointer.");
        }

        if (gpiod_line_request_output(line, consumer.c_str(), defaultVal) < 0) {
            throw std::runtime_error("Failed to request line as output. Consumer: " + consumer);
        }
    }

    void GPIOHandler::RequestLineInput(gpiod_line *line,
                                        const std::string &consumer)
    {
        if (!initialized || !chip) {
            throw std::runtime_error("GPIOHandler::RequestLineInput() called but chip is not initialized.");
        }
        if (!line) {
            throw std::runtime_error("GPIOHandler::RequestLineInput() called with null line pointer.");
        }

        if (gpiod_line_request_input(line, consumer.c_str()) < 0) {
            throw std::runtime_error("Failed to request line as Input. Consumer: " + consumer);
        }
    }

    /**
     * @brief Releases a gpiod_line and sets it to nullptr.
     */
    void GPIOHandler::ReleaseLine(gpiod_line *&line)
    {
        if (line) {
            gpiod_line_release(line);
            line = nullptr;
        }
    }

    void GPIOHandler::WaitForValue(gpiod_line *line, int value, std::atomic<bool>& cycle)
    {
        while (gpiod_line_get_value(line)!=value && cycle.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}