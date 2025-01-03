#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <mutex>
#include <chrono>
#include "..\ThirdParties\sqlite3.h"

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
    std::string outcome;

    RecordData(sqlite_int64 time, int event, const unsigned char* className, const unsigned char* outcome);
    RecordData(int64_t time, int event, const std::string& className, const std::string& outcome);
};

/**
 * @brief Class responsible for handling database operations with buffering.
 */
class DbHandler
{
public:
    /**
     * @brief Default constructor. Opens database at "..\\db.sqlite3" and ensures table existence.
     */
    DbHandler();

    /**
     * @brief Constructs DbHandler with custom database path.
     * @param dbpath Path to SQLite database file
     */
    DbHandler(std::filesystem::path dbpath);

    /**
     * @brief Destructor flushes any remaining buffered data and closes the database.
     */
    ~DbHandler();

    /**
     * @brief Adds a record to internal buffer; flushes to DB if conditions met.
     * @param time Unix timestamp
     * @param event Event ID
     * @param className Class name
     * @param outcome Outcome
     */
    void InsertData(int64_t time, int event, const std::string& className, const std::string& outcome);

    /**
     * @brief Adds a record to internal buffer; flushes to DB if conditions met.
     * @param record The record data
     */
    void InsertData(RecordData record);

    /**
     * @brief Reads all events from the database
     * @return Vector of RecordData
     */
    std::vector<RecordData> ReadData();

    /**
     * @brief Reads events from the database with TIME between start and end (inclusive).
     * @param start Start timestamp
     * @param end End timestamp
     * @return Vector of RecordData
     */
    std::vector<RecordData> ReadDataByRange(int64_t start, int64_t end);

    /**
     * @brief Reads events from the database with TIME > time.
     * @param time The cutoff timestamp
     * @return Vector of RecordData
     */
    std::vector<RecordData> ReadDataAfter(int64_t time);

    /**
     * @brief Reads events from the database with TIME < time.
     * @param time The cutoff timestamp
     * @return Vector of RecordData
     */
    std::vector<RecordData> ReadDataBefore(int64_t time);

private:
    /**
     * @brief Opens the database connection and initializes 'db'.
     * @throws std::ios_base::failure if opening fails
     */
    void OpenDb();

    /**
     * @brief Creates the table (if it doesn't exist) for storing events.
     * @throws std::runtime_error if creation fails
     */
    void CreateTableIfNeeded();

    /**
     * @brief Flushes the buffer to the database if conditions are met (size/time).
     */
    void MaybeFlush();

    /**
     * @brief Flushes all events currently in the buffer into the database.
     * @throws std::runtime_error if insertion fails
     */
    void FlushBuffer();

private:
    sqlite3* db = nullptr;
    std::filesystem::path dbpath;

    // Buffer for records
    std::vector<RecordData> buffer_;

    // Maximum number of records to accumulate before forcing a flush
    size_t maxBufferSize_ = 0xff;

    // Interval after which we also force a flush
    std::chrono::steady_clock::time_point lastFlushTime_ = std::chrono::steady_clock::now();
    std::chrono::seconds flushInterval_ = std::chrono::seconds(30);

    // Mutex for thread safety (in case multiple threads use DbHandler)
    std::mutex mutex_;
};
