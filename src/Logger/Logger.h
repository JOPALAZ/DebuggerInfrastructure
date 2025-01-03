#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <format>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <ctime>

/**
 * @brief A simple Logger class that writes messages to both a file and console (based on severities).
 */
class Logger
{
private:
    /**
     * @brief Minimum severity required to log a message into a file.
     */
    static int minSeverityToLogfile;

    /**
     * @brief Minimum severity required to log a message to console.
     */
    static int minSeverityToLogConsole;

    /**
     * @brief Output file stream for logging.
     */
    static std::ofstream fileOutput;

    /**
     * @brief True if logger is initialized.
     */
    static bool initialized;

    /**
     * @brief Base logging function that does actual output to file/console.
     * @tparam T the message type (usually std::string).
     * @param severity severity of the message.
     * @param msg string message to log.
     */
    template <typename T>
    static void BaseLog(int severity, T msg);

public:
    /**
     * @brief Initializes the logger by opening a log file and setting severity thresholds.
     * @param logPath Path to the directory or file prefix where log file will be created.
     * @param minSeverityToLogfile Minimal severity for writing logs to file.
     * @param minSeverityToLogConsole Minimal severity for writing logs to console.
     * @throw std::ios_base::failure if file creation fails.
     */
    static void Initialize(std::filesystem::path logPath, int minSeverityToLogfile, int minSeverityToLogConsole);

    /**
     * @brief Returns the current local time as a string, formatted as YYYY-MM-DD-HH-MM-SS.
     */
    static std::string getCurrentTime();

    //---------------------------------------------------------------------------------------------
    // Non-formatted logging methods (directly accept a string or other printable type).
    //---------------------------------------------------------------------------------------------
    /**
     * @brief Logs a verbose message (severity = 0).
     */
    template <typename T>
    static void Verbose(T msg);

    /**
     * @brief Logs an info message (severity = 1).
     */
    template <typename T>
    static void Info(T msg);

    /**
     * @brief Logs an error message (severity = 2).
     */
    template <typename T>
    static void Error(T msg);

    /**
     * @brief Logs a critical error message (severity = 3).
     */
    template <typename T>
    static void Critical(T msg);

    //---------------------------------------------------------------------------------------------
    // Formatted logging methods (accept a format string with variadic arguments).
    //---------------------------------------------------------------------------------------------
    /**
     * @brief Logs a verbose message using a format string (severity = 0).
     * @tparam Args variadic template parameter pack.
     */
    template <typename... Args>
    static void Verbose(std::string_view msgFormat, Args&&... args);

    /**
     * @brief Logs an info message using a format string (severity = 1).
     */
    template <typename... Args>
    static void Info(std::string_view msgFormat, Args&&... args);

    /**
     * @brief Logs an error message using a format string (severity = 2).
     */
    template <typename... Args>
    static void Error(std::string_view msgFormat, Args&&... args);

    /**
     * @brief Logs a critical message using a format string (severity = 3).
     */
    template <typename... Args>
    static void Critical(std::string_view msgFormat, Args&&... args);
};

//-------------------------------------------------------------------------------------------------
// Template method implementations
//-------------------------------------------------------------------------------------------------
template <typename T>
void Logger::BaseLog(int severity, T msg)
{
    std::string severityStr;
    switch (severity) {
        case 0: severityStr = "VRB"; break;  // Verbose
        case 1: severityStr = "INF"; break;  // Info
        case 2: severityStr = "ERR"; break;  // Error
        case 3: severityStr = "CRT"; break;  // Critical
        default: severityStr = "UKN"; break; // Unknown
    }

    // Example: [2024-12-28-10-22-30] [INF]: Your message
    std::string str = std::format("[{}] [{}]: {}", getCurrentTime(), severityStr, msg);

    // Log to file if the severity is >= minSeverityToLogfile
    if (severity >= Logger::minSeverityToLogfile && Logger::fileOutput.is_open()) {
        Logger::fileOutput << str << std::endl;
    }

    // Log to console if the severity is >= minSeverityToLogConsole
    if (severity >= Logger::minSeverityToLogConsole) {
        // By convention, errors and above go to std::cerr
        if (severity >= 2) {
            std::cerr << str << std::endl;
        } else {
            std::cout << str << std::endl;
        }
    }
}

template <typename T>
void Logger::Verbose(T msg)
{
    BaseLog(0, msg);
}

template <typename T>
void Logger::Info(T msg)
{
    BaseLog(1, msg);
}

template <typename T>
void Logger::Error(T msg)
{
    BaseLog(2, msg);
}

template <typename T>
void Logger::Critical(T msg)
{
    BaseLog(3, msg);
}

template <typename... Args>
void Logger::Verbose(std::string_view msgFormat, Args&&... args)
{
    std::string msg = std::vformat(msgFormat, std::make_format_args(args...));
    Verbose(msg);
}

template <typename... Args>
void Logger::Info(std::string_view msgFormat, Args&&... args)
{
    std::string msg = std::vformat(msgFormat, std::make_format_args(args...));
    Info(msg);
}

template <typename... Args>
void Logger::Error(std::string_view msgFormat, Args&&... args)
{
    std::string msg = std::vformat(msgFormat, std::make_format_args(args...));
    Error(msg);
}

template <typename... Args>
void Logger::Critical(std::string_view msgFormat, Args&&... args)
{
    std::string msg = std::vformat(msgFormat, std::make_format_args(args...));
    Critical(msg);
}
