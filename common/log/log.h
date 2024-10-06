#ifndef __NAZL_LOG_H_
#define __NAZL_LOG_H_

#include <string.h>
#include <stdint.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <fmt/core.h>
#include <fmt/printf.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "file_ops.h"
#include "singleton.h"
//Timestamp Level EOL Func Line Thread_name Thread_id message
class LogLevel
{
public:
    enum class LevelEnum
    {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };
    LogLevel(LevelEnum level = LevelEnum::Info): level_(level) {}
    LevelEnum GetLevel()
    {
        return level_;
    }

    std::string GetLevelString();

    void SetLevel(LevelEnum level)
    {
        level_ = level;
    }
private:
    LevelEnum level_;
};


class LogEvent
{
public:
    LogEvent(const std::string& fileName, const std::string& funcName, const std::string& threadName,
             int32_t line, uint32_t threadId, uint64_t timestamp, LogLevel level, const std::string& message)
        : fileName_(fileName), funcName_(funcName), threadName_(threadName), line_(line), threadId_(threadId),
          timestamp_(timestamp), level_(level), message_(message) {}
    ~LogEvent() = default;
    const std::string GetFile() const
    {
        return fileName_;
    }
    const std::string GetFunc() const
    {
        return funcName_;
    }
    const std::string GetThreadName() const
    {
        return threadName_;
    }
    int32_t GetLine() const
    {
        return line_;
    }
    uint32_t GetThreadId() const
    {
        return threadId_;
    }
    uint64_t GetTimestamp() const
    {
        return timestamp_;
    }
    std::string GetMessage() const
    {
        return message_;
    }
    LogLevel GetLevel() const
    {
        return level_;
    }
private:
    std::string fileName_;
    std::string funcName_;
    std::string threadName_;
    int32_t line_;
    uint32_t threadId_;
    uint64_t timestamp_;
    LogLevel level_;
    std::string message_;
};
class FlagFormatter
{
public:
    FlagFormatter() = default;
    virtual ~FlagFormatter() = default;
    virtual void Format(std::ostream& os, std::shared_ptr<LogEvent> event) = 0;
};
class StringFlagFormatter : public FlagFormatter
{
public:
    StringFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << event->GetMessage();
    }
};
class TimeFlagFormatter : public FlagFormatter
{
public:
    TimeFlagFormatter(const std::string& format = "%Y-%m-%d %H:%M:%S")
        : time_format_(format) {};
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        std::time_t t = std::time(nullptr);
        std::tm localTime;
        localtime_r(&t, &localTime);
        char buf[64];
        strftime(buf, sizeof(buf), time_format_.c_str(), &localTime);
        os << buf << " ";
    }
private:
    std::string time_format_;
};
class LevelFlagFormatter : public FlagFormatter
{
public:
    LevelFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << event->GetLevel().GetLevelString() << " ";
    }
};
class EOLFlagFormatter : public FlagFormatter
{
public:
    EOLFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << std::endl;
    }
};
class FuncFlagFormatter : public FlagFormatter
{
public:
    FuncFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << "[" << event->GetFunc() << ":";
    }
};
class LineFlagFormatter : public FlagFormatter
{
public:
    LineFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << event->GetLine() << "] ";
    }
};
class TidFlagFormatter : public FlagFormatter
{
public:
    TidFlagFormatter() = default;
    void Format(std::ostream& os, std::shared_ptr<LogEvent> event)
    {
        os << std::this_thread::get_id() << " ";
    }
};

class LogFormat
{
public:
    LogFormat(std::string pattern = "");
    ~LogFormat() = default;
    std::string Format(std::shared_ptr<LogEvent> event);
    void Format(std::ostream &os, std::shared_ptr<LogEvent> event);
private:
    void ParsePattern();
private:
    std::string pattern_;
    std::vector<std::unique_ptr<FlagFormatter>> formate_flag_;
};

class Sink
{
public:
    Sink() = default;
    virtual ~Sink() = default;
    virtual void Flush() = 0;
    virtual void Log(std::shared_ptr<LogEvent> event) = 0;
    virtual void SetFormat(std::shared_ptr<LogFormat> format) = 0;
    virtual void SetLevel(LogLevel log_level) = 0;
    virtual LogLevel GetLevel() = 0;
protected:
    LogLevel level_;
    std::shared_ptr<LogFormat> format_;
};
class StdoutSink : public Sink
{
public:
    StdoutSink() = default;
    ~StdoutSink() = default;
    StdoutSink(const StdoutSink&) = delete;
    StdoutSink& operator=(const StdoutSink&) = delete;
    void Log(std::shared_ptr<LogEvent> event) override;
    void Flush() override;
    void SetFormat(std::shared_ptr<LogFormat> format) override;
    void SetLevel(LogLevel log_level) override;
    LogLevel GetLevel() override;
private:
    std::mutex mutex_;
};
class FileSink : public Sink
{
public:
    explicit FileSink(const std::string &file_name, std::size_t max_size, std::size_t max_files, Nazl::FileOps&& file_ops = Nazl::FileOps());
    ~FileSink() = default;
    void Log(std::shared_ptr<LogEvent> event) override;
    void Flush() override;
    void SetFormat(std::shared_ptr<LogFormat> format) override;
    void SetLevel(LogLevel log_level) override;
    LogLevel GetLevel() override;
private:
    void Rotate();
    bool RenameFile(const std::string& src, const std::string& dst);
private:
    std::string file_name_;
    std::size_t max_size_;
    std::size_t max_files_;
    std::size_t current_size_;
    Nazl::FileOps file_ops_;
    std::mutex mutex_;
};
class Logger
{
public:
    explicit Logger(const std::string &name)
        : name_(std::move(name)) {}

    template<typename T>
    Logger(const std::string &name, T begin, T end)
        : name_(std::move(name)), sinks_(begin, end)  {}

    virtual ~Logger() = default;
    void SinkIt(std::shared_ptr<LogEvent> event);
    std::string GetName() const
    {
        return name_;
    }
    void SetFormatter(std::shared_ptr<LogFormat> format);
private:
    std::string name_;
    std::vector<std::shared_ptr<Sink>> sinks_;
};
class LogManager
{
public:
    LogManager() = default;
    void RegisterLogger(std::shared_ptr<Logger> logger);
    std::shared_ptr<Logger> GetDefaultLogger();
    std::shared_ptr<Logger> GetLogger(const std::string &name);
private:
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex map_mutex_;
};
typedef Nazl::Singleton<LogManager> logger_manager;

template<typename... Args>
void LOG_COMMON(LogLevel level, const std::string& logger_name, const std::string& file, const std::string& func,
                int32_t line, const std::string& format, Args&&... args)
{
    std::shared_ptr<Logger> logger;
    if (!logger_name.empty())
    {
        logger = logger_manager::GetInstance().GetLogger(logger_name);
    }
    else
    {
        logger = logger_manager::GetInstance().GetDefaultLogger();
    }
    if (!logger)
    {
        std::cerr << "Logger not found!" << std::endl;
        return;
    }
    std::string message = fmt::sprintf(format, std::forward<Args>(args)...);
    auto event = std::make_shared<LogEvent>(file, func, "ThreadName", line, 1234, std::time(nullptr), level, message);
    logger->SinkIt(event);
}


#define LOG_COMMON_IMPL(level, logger, format, ...) LOG_COMMON(level, logger, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) LOG_COMMON_IMPL(LogLevel::LevelEnum::Info, "", format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG_COMMON_IMPL(LogLevel::LevelEnum::Debug, "", format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) LOG_COMMON_IMPL(LogLevel::LevelEnum::Warn, "", format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_COMMON_IMPL(LogLevel::LevelEnum::Error, "", format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) LOG_COMMON_IMPL(LogLevel::LevelEnum::Fatal, "", format, ##__VA_ARGS__)
int32_t log_init(const std::string& name);
#endif
