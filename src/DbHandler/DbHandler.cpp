#include "DbHandler.h"
#include <cstring>    // for strnlen
#include "../Logger/Logger.h"


namespace DebuggerInfrastructure
{
    //----------------------------------------------
    // Safe string conversion
    //----------------------------------------------
    std::string safeConvertToString(const unsigned char* input) {
        if (input == nullptr) {
            throw std::invalid_argument("Null pointer passed to safeConvertToString");
        }

        const size_t maxLength = 1024 * 1024;
        size_t length = strnlen(reinterpret_cast<const char*>(input), maxLength);

        if (length == maxLength) {
            throw std::length_error("Input string exceeds maximum allowed length");
        }

        return std::string(reinterpret_cast<const char*>(input), length);
    }

    //----------------------------------------------
    // RecordData definitions
    //----------------------------------------------
    RecordData::RecordData(sqlite_int64 time, int event, const unsigned char* className, const unsigned char* description)
        : time(time), event(event)
    {
        this->className = safeConvertToString(className);
        this->description   = safeConvertToString(description);
    }

    RecordData::RecordData(int64_t time, int event, const std::string& className, const std::string& description)
        : time(time), event(event), className(className), description(description)
    {
    }

    //----------------------------------------------
    // DbHandler constructors/destructor
    //----------------------------------------------
    DbHandler::DbHandler()
    {
        dbpath = "db.sqlite3";
        this->OpenDb();
        this->CreateTableIfNeeded();
    }

    DbHandler::DbHandler(std::filesystem::path dbpath)
        : dbpath(dbpath)
    {
        this->OpenDb();
        this->CreateTableIfNeeded();
    }

    DbHandler::~DbHandler()
    {
        // Flush any remaining data in the buffer before closing
        {
            std::lock_guard<std::mutex> lock(mutex_);
            FlushBuffer(); 
        }

        // Now close the database connection
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }

        Logger::Verbose("DbHandler destroyed, all buffered data flushed and DB closed.");
    }

    //----------------------------------------------
    // InsertData (buffered)
    //----------------------------------------------
    void DbHandler::InsertData(int64_t time, int event, const std::string& className, const std::string& description)
    {
        // Lock to protect shared resources
        std::lock_guard<std::mutex> lock(mutex_);

        // Add the record to the in-memory buffer
        buffer_.push_back(RecordData(time, event, className, description));

        // Check if we need to flush now
        MaybeFlush();
    }

    void DbHandler::InsertData(int64_t time, Event event, const std::string& className, const std::string& description)
    {
        DbHandler::InsertData(time, (int)(event), className, description);
    }

    void DbHandler::InsertDataNow(int event, const std::string& className, const std::string& description)
    {
        DbHandler::InsertData(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), event, className, description);
    }

    void DbHandler::InsertDataNow(Event event, const std::string& className, const std::string& description)
    {
        DbHandler::InsertDataNow((int)(event), className, description);
    }

    void DbHandler::InsertData(RecordData record)
    {
        // Just reuse the other InsertData
        InsertData(record.time, record.event, record.className, record.description);
    }

    //----------------------------------------------
    // Reading from DB
    //----------------------------------------------
    std::vector<RecordData> DbHandler::ReadData()
    {
        std::vector<RecordData> output;
        const char* sql = "SELECT * FROM Events ORDER BY TIME DESC;";
        sqlite3_stmt* stmt;

        FlushBuffer();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::Error("Failed to prepare statement: {}", sqlite3_errmsg(db));
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            output.push_back(RecordData(
                sqlite3_column_int64(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3)
            ));
        }

        sqlite3_finalize(stmt);
        return output;
    }

    std::vector<RecordData> DbHandler::ReadDataByRange(int64_t start, int64_t end)
    {
        const char* sql = "SELECT * FROM Events WHERE TIME BETWEEN ? AND ? ORDER BY TIME DESC;";
        sqlite3_stmt* stmt;
        std::vector<RecordData> output;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::Error("Failed to prepare statement: {}", sqlite3_errmsg(db));
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_int64(stmt, 1, start);
        sqlite3_bind_int64(stmt, 2, end);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            output.push_back(RecordData(
                sqlite3_column_int64(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3)
            ));
        }

        sqlite3_finalize(stmt);
        return output;
    }

    std::vector<RecordData> DbHandler::ReadDataAfter(int64_t time)
    {
        const char* sql = "SELECT * FROM Events WHERE TIME > ? ORDER BY TIME DESC;";
        sqlite3_stmt* stmt;
        std::vector<RecordData> output;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::Error("Failed to prepare statement: {}", sqlite3_errmsg(db));
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_int64(stmt, 1, time);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            output.push_back(RecordData(
                sqlite3_column_int64(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3)
            ));
        }

        sqlite3_finalize(stmt);
        return output;
    }

    std::vector<RecordData> DbHandler::ReadDataBefore(int64_t time)
    {
        std::vector<RecordData> output;
        const char* sql = "SELECT * FROM Events WHERE TIME < ? ORDER BY TIME DESC;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::Error("Failed to prepare statement: {}", sqlite3_errmsg(db));
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_int64(stmt, 1, time);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            output.push_back(RecordData(
                sqlite3_column_int64(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3)
            ));
        }

        sqlite3_finalize(stmt);
        return output;
    }

    //----------------------------------------------
    // Create table if needed
    //----------------------------------------------
    void DbHandler::CreateTableIfNeeded()
    {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS Events (
                TIME        INTEGER NOT NULL,
                EVENT       INTEGER NOT NULL,
                CLASS       TEXT    NOT NULL,
                DESCRIPTION TEXT    NOT NULL
            );
        )";

        char* errorMessage = nullptr;
        int result = sqlite3_exec(db, sql, nullptr, nullptr, &errorMessage);

        if (result != SQLITE_OK) {
            Logger::Error("Error creating table: {}", errorMessage);
            std::string errStr = errorMessage ? errorMessage : "Unknown error";
            sqlite3_free(errorMessage);
            throw std::runtime_error(fmt::format("Error creating table: {}", errStr));
        } else {
            Logger::Verbose("Table created successfully (or already exists).");
        }
    }

    //----------------------------------------------
    // Open DB
    //----------------------------------------------
    void DbHandler::OpenDb()
    {
        int exitCode = sqlite3_open(dbpath.generic_string().c_str(), &db);
        if (exitCode) {
            Logger::Error("Error opening database: {}", std::string{sqlite3_errmsg(db)});
            throw std::ios_base::failure(fmt::format("Error opening database: {}", sqlite3_errmsg(db)));
        }
        Logger::Verbose("Database opened successfully!");
    }

    //----------------------------------------------
    // Buffer flush logic
    //----------------------------------------------
    void DbHandler::MaybeFlush()
    {
        // We check size limit or time limit
        auto now = std::chrono::steady_clock::now();
        if (buffer_.size() >= maxBufferSize_ ||
            (now - lastFlushTime_) >= flushInterval_)
        {
            FlushBuffer();
            lastFlushTime_ = std::chrono::steady_clock::now();
        }
    }

    void DbHandler::FlushBuffer()
    {
        if (buffer_.empty()) {
            return; // Nothing to flush
        }

        Logger::Verbose("Flushing {} records to the database...", buffer_.size());

        // Begin transaction
        char* errMsg = nullptr;
        if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string errorStr = errMsg ? errMsg : "Unknown error";
            Logger::Error("Error beginning transaction: {}", errorStr);
            sqlite3_free(errMsg);
            throw std::runtime_error("Error beginning transaction: " + errorStr);
        }

        // Prepare statement for multiple inserts
        const char* sql = "INSERT INTO Events (TIME, EVENT, CLASS, DESCRIPTION) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::Error("Failed to prepare statement: {}", sqlite3_errmsg(db));
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        // Insert each record
        for (const auto& record : buffer_) 
        {
            sqlite3_bind_int64(stmt, 1, record.time);
            sqlite3_bind_int   (stmt, 2, record.event);
            sqlite3_bind_text  (stmt, 3, record.className.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text  (stmt, 4, record.description.c_str(),   -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                Logger::Error("Error inserting data: {}", sqlite3_errmsg(db));
                throw std::runtime_error("Error inserting data: " + std::string(sqlite3_errmsg(db)));
            }

            // Reset statement to reuse for the next record
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);

        // Commit transaction
        if (sqlite3_exec(db, "COMMIT TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string errorStr = errMsg ? errMsg : "Unknown error";
            Logger::Error("Error committing transaction: {}", errorStr);
            sqlite3_free(errMsg);
            throw std::runtime_error("Error committing transaction: " + errorStr);
        }

        // Clear buffer
        buffer_.clear();
        Logger::Verbose("Flush complete.");
    }
}