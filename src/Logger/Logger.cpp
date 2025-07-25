#include "Logger.h"
namespace DebuggerInfrastructure
    {
    // Define all static member variables
    int Logger::minSeverityToLogfile = 0;
    int Logger::minSeverityToLogConsole = 0;
    std::ofstream Logger::fileOutput;
    bool Logger::initialized = false;

    /**
     * @brief Initializes the logger by opening a log file with timestamp in its name.
     */
    void Logger::Initialize(std::filesystem::path logPath, int minSeverityFile, int minSeverityConsole)
    {
        if (!initialized)
        {
            // Set severities
            Logger::minSeverityToLogfile = minSeverityFile;
            Logger::minSeverityToLogConsole = minSeverityConsole;

            // If fileOutput was open, close it
            if (Logger::fileOutput.is_open()) {
                Logger::fileOutput.close();
            }

            // Construct final log file name: e.g. if logPath = "C:/Logs",
            // we'll get something like C:/Logs/log2024-12-28-10-22-30.log
            // Feel free to adjust to your liking
            std::filesystem::path finalLogFile = logPath;
            finalLogFile = fmt::format("log{}.log", getCurrentTime());

            // Open the file
            Logger::fileOutput.open(finalLogFile, std::ios::out);

            if (!Logger::fileOutput.is_open() || Logger::fileOutput.bad()) {
                throw std::ios_base::failure("Couldn't create a log file at: " + finalLogFile.string());
            }

            Logger::initialized = true;
        }
    }

    /**
     * @brief Gets the current local time as a string formatted as YYYY-MM-DD-HH-MM-SS.
     */
    std::string Logger::getCurrentTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);

        std::ostringstream oss;
        // Example: 2024-12-28-10-22-30
        oss << std::put_time(localTime, "%Y-%m-%d-%H-%M-%S");
        return oss.str();
    }
}