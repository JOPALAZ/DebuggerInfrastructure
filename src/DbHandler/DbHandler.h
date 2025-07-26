#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <mutex>
#include <chrono>
#include "../ThirdParties/sqlite3/sqlite3.h"
namespace DebuggerInfrastructure
{
    // Forward declaration, so we can use RecordData in DbHandler.
    struct RecordData;

    /**
     * @brief Safely converts unsigned char* to std::string (throws exceptions if invalid).
     * @param input Pointer to the unsigned char array
     * @return String representation
     */
    std::string safeConvertToString(const unsigned char* input);

    /**
     * @brief A simple struct to hold event data.
     */
    struct RecordData
    {
        int64_t time;
        int event;
        std::string className;
        std::string description;

        RecordData(sqlite_int64 time, int event, const unsigned char* className, const unsigned char* description);
        RecordData(int64_t time, int event, const std::string& className, const std::string& description);
    };

    enum Event
    {
        EMERGENCYLOCK = 0,
        EMERGENCYADDLOCKREASON = 1,
        EMERGENCYREMOVELOCKREASON = 2,
        EMERGENCYUNLOCK = 3,
        CALIBRATIONSTART = 4,
        CALIBRATIONEND = 5,
        ELIMINATION = 6
    };

    /**
     * @brief Class responsible for handling database operations with buffering.
     */
    class DbHandler
    {
    public:
        DbHandler() = delete;
        DbHandler(const DbHandler&) = delete;
        DbHandler(DbHandler&&) = delete;
        DbHandler& operator=(const DbHandler&) = delete;
        DbHandler& operator=(DbHandler&&) = delete;
        /**
         * @brief Default Init. Opens database at "..\\db.sqlite3" and ensures table existence.
         */
        static void Initialize();

        /**
         * @brief Init DbHandler with custom database path.
         * @param dbpath Path to SQLite database file
         */
        static void Initialize(std::filesystem::path dbpath);

        /**
         * @brief Destructor flushes any remaining buffered data and closes the database.
         */
        static void Dispose();

        /**
         * @brief Adds a record to internal buffer; flushes to DB if conditions met.
         * @param time Unix timestamp
         * @param event Event ID
         * @param className Class name
         * @param description Outcome
         */
        static void InsertData(int64_t time, int event, const std::string& className, const std::string& description);
        static void InsertData(int64_t time, Event event, const std::string& className, const std::string& description);
        static void InsertDataNow(int event, const std::string& className, const std::string& description);
        static void InsertDataNow(Event event, const std::string& className, const std::string& description);

        /**
         * @brief Adds a record to internal buffer; flushes to DB if conditions met.
         * @param record The record data
         */
        static void InsertData(RecordData record);

        /**
         * @brief Reads all events from the database
         * @return Vector of RecordData
         */
        static std::vector<RecordData> ReadData();

        /**
         * @brief Reads events from the database with TIME between start and end (inclusive).
         * @param start Start timestamp
         * @param end End timestamp
         * @return Vector of RecordData
         */
        static std::vector<RecordData> ReadDataByRange(int64_t start, int64_t end);

        /**
         * @brief Reads events from the database with TIME > time.
         * @param time The cutoff timestamp
         * @return Vector of RecordData
         */
        static std::vector<RecordData> ReadDataAfter(int64_t time);

        /**
         * @brief Reads events from the database with TIME < time.
         * @param time The cutoff timestamp
         * @return Vector of RecordData
         */
        static std::vector<RecordData> ReadDataBefore(int64_t time);

    private:
        /**
         * @brief Opens the database connection and initializes 'db'.
         * @throws std::ios_base::failure if opening fails
         */
        static void OpenDb();

        /**
         * @brief Creates the table (if it doesn't exist) for storing events.
         * @throws std::runtime_error if creation fails
         */
        static void CreateTableIfNeeded();

        /**
         * @brief Flushes the buffer to the database if conditions are met (size/time).
         */
        static void MaybeFlush();

        /**
         * @brief Flushes all events currently in the buffer into the database.
         * @throws std::runtime_error if insertion fails
         */
        static void FlushBuffer();

        static void CheckInitialized();

    private:
        static sqlite3* db;
        static std::filesystem::path dbpath;

        // Buffer for records
        static std::vector<RecordData> buffer_;

        // Maximum number of records to accumulate before forcing a flush
        static constexpr size_t maxBufferSize_ = 0xff;

        // Interval after which we also force a flush
        static std::chrono::steady_clock::time_point lastFlushTime;
        static constexpr std::chrono::seconds flushInterval_ = std::chrono::seconds(30);

        // Mutex for thread safety (in case multiple threads use DbHandler)
        static std::mutex mutex_;

        static bool initialized;
    };
}