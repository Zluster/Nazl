#ifndef __NAZL_LOG_H_
#define __NAZL_LOG_H_

#include <string.h>
#include <stdint.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include "mutex.h"
#include "file_ops.h"
#include "singleton.h"
//Timestamp Level EOL Func Line Thread_name Thread_id message
class LogLevel {
public:
    enum class LevelEnum {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };
    LogLevel():level_(LevelEnum::Debug) {}
    LevelEnum GetLevel(){ return level_;}

    std::string GetLevelString();

    void SetLevel(LevelEnum level){ level_ = level;}
private:
    LevelEnum level_{LevelEnum::Info};
};
class FlagFormatter {
public:
    FlagFormatter() = default;
    virtual ~FlagFormatter() = default;
    virtual void Format(std::ostream& os, const std::string &msg) = 0;
};
class StringFlagFormatter : public FlagFormatter {
public:
    StringFlagFormatter() = default;
    void Format(std::ostream& os, const std::string &msg) {
        os << msg;
    }

};
class TimeFlagFormatter : public FlagFormatter {
public:
    TimeFlagFormatter(const std::string& format = "%Y-%m-%d %H:%M:%S")
        :time_format_(format) {};
    void Format(std::ostream& os, const std::string &msg) {
        std::time_t t = std::time(nullptr);
        std::tm localTime;
        localtime_r(&t, &localTime);
        char buf[64];
        strftime(buf, sizeof(buf), time_format_.c_str(), &localTime);
        os << buf;
    }
private:
    std::string time_format_;
};
class LevelFlagFormatter : public FlagFormatter {
public:
    LevelFlagFormatter(LogLevel level ):level_(level) {}
    void Format(std::ostream& os, const std::string &msg) {
        os << level_.GetLevelString();
    }
private:
    LogLevel level_;
};
class EOLFlagFormatter : public FlagFormatter {
public:
    EOLFlagFormatter() = default;
    void Format(std::ostream& os, const std::string &msg) {
        os << std::endl;
    }
};
class FuncFlagFormatter : public FlagFormatter {
public:
    FuncFlagFormatter() = default;
    void Format(std::ostream& os, const std::string &msg) {
        os << __FUNCTION__;
    }
};
class LineFlagFormatter : public FlagFormatter {
public:
    LineFlagFormatter() = default;
    void Format(std::ostream& os, const std::string &msg) {
        os << __LINE__;
    }
};
class TidFlagFormatter : public FlagFormatter {
public:
    TidFlagFormatter() = default;
    void Format(std::ostream& os, const std::string &msg) {
        os << std::this_thread::get_id();
    }
};

class LogFormat {
public:
    LogFormat();
    ~LogFormat() = default;
    std::string Format(const std::string &msg);
    void Format(std::ostream &os, const std::string &msg);
private:
    void ParsePattern();
private:
    std::string pattern_;
    std::vector<std::unique_ptr<FlagFormatter>> formate_flag_;
    LogLevel level_;
};

class Sink {
public:
    Sink() = default;
    virtual ~Sink() = default;
    virtual void Flush() = 0;
    virtual void Log(const std::string &msg) = 0;
    virtual void SetFormat(std::shared_ptr<LogFormat> format) = 0;
    virtual void SetLevel(LogLevel log_level) = 0;
    virtual LogLevel GetLevel() = 0;
protected:
    LogLevel level_;
    std::shared_ptr<LogFormat> format_;
};
class StdoutSink : public Sink{
public:
    typedef Nazl::Mutex MutexType;
    StdoutSink() = default;
    ~StdoutSink() = default;
    StdoutSink(const StdoutSink&) = delete;
    StdoutSink& operator=(const StdoutSink&) = delete;
    void Log(const std::string &msg) override;
    void Flush() override;
    void SetFormat(std::shared_ptr<LogFormat> format) override;
    void SetLevel(LogLevel log_level) override;
    LogLevel GetLevel() override;
private:
    MutexType mutex_;
};
class FileSink : public Sink{
public:
    typedef Nazl::Mutex MutexType;
    explicit FileSink(const std::string &file_name, std::size_t max_size, std::size_t max_files, Nazl::FileOps&& file_ops);
    ~FileSink() = default;
    void Log(const std::string &msg) override;
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
    MutexType mutex_;
};
class Logger{
public:
    explicit Logger(const std::string &name)
        :name_(std::move(name)) {}

    template<typename T>
        Logger(const std::string &name, T begin, T end)
        :name_(std::move(name)), sinks_(begin, end)  {}

    virtual ~Logger() = default;
    void SinkIt(const std::string& msg);
    std::string GetName() const { return name_; }
    void SetFormatter(std::shared_ptr<LogFormat> format);
private:
    std::string name_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    LogLevel level_;
};
class LogManager{
public:
    typedef Nazl::Mutex MutexType;
    LogManager() = default;
    void RegisterLogger(std::shared_ptr<Logger> logger);
    std::shared_ptr<Logger> GetLogger(const std::string &name);
private:
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    MutexType map_mutex_;
    LogLevel level_;

};
typedef Nazl::Singleton<LogManager> logger_manager;
#endif