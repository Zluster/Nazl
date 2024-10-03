//
// Created by zwz on 2024/8/25.
//
#include <iostream>
#include "log.h"

LogFormat::LogFormat()
{
    ParsePattern();
}

std::string LogFormat::Format(std::shared_ptr<LogEvent> event)
{
    std::stringstream ss;
    for (auto &it : formate_flag_)
    {
        it->Format(ss, event);
    }
    return ss.str();
}
void LogFormat::Format(std::ostream &os, std::shared_ptr<LogEvent> event)
{
    for (auto &it : formate_flag_)
    {
        it->Format(os, event);
    }
}
void LogFormat::ParsePattern()
{
    if (pattern_.empty())
    {
        //Timestamp Level  Func Line  Thread_id Message EOL
        pattern_ = "%T %L  %f %l %N %m %E";
    }
    for (auto it = pattern_.begin(); it != pattern_.end(); ++it)
    {
        if (*it == '%')
        {
            ++it;
            switch (*it)
            {
            case 'T':
                formate_flag_.push_back(std::make_unique<TimeFlagFormatter>());
                break;
            case 'L':
                formate_flag_.push_back(std::make_unique<LevelFlagFormatter>());
                break;
            case 'E':
                formate_flag_.push_back(std::make_unique<EOLFlagFormatter>());
                break;
            case 'f':
                formate_flag_.push_back(std::make_unique<FuncFlagFormatter>());
                break;
            case 'l':
                formate_flag_.push_back(std::make_unique<LineFlagFormatter>());
                break;
            case 'm':
                formate_flag_.push_back(std::make_unique<StringFlagFormatter>());
                break;
            case 'N':
                formate_flag_.push_back(std::make_unique<TidFlagFormatter>());
                break;
            default:
                std::cout << "error pattern" << std::endl;
                break;
            }
        }
    }
}

void StdoutSink::Log(std::shared_ptr<LogEvent> event)
{
    MutexType::Lock lock(mutex_);
    format_->Format(std::cout, event);
    ::fflush(stdout);
}
void StdoutSink::Flush()
{
    MutexType::Lock lock(mutex_);
    ::fflush(stdout);
}
void StdoutSink::SetFormat(std::shared_ptr <LogFormat> format)
{
    MutexType::Lock lock(mutex_);
    format_ = std::move(format);
}
void StdoutSink::SetLevel(LogLevel log_level)
{
    MutexType::Lock lock(mutex_);
    level_ = log_level;
}
LogLevel StdoutSink::GetLevel()
{
    MutexType::Lock lock(mutex_);
    return level_;
}
FileSink::FileSink(const std::string &file_name, std::size_t max_size, std::size_t max_files, Nazl::FileOps&& file_ops)
    : file_name_(file_name), max_size_(max_size), max_files_(max_files), file_ops_(std::move(file_ops))
{
    if (max_size == 0)
    {
        max_size_ = 1024 * 1024 * 1024;
    }
    if (max_files > 2000)
    {
        max_files_ = 2000;
    }
    if (max_files == 0)
    {
        max_files_ = 10;
    }
    file_ops_.open(file_name_);
    std::cout << "open file " << file_name_ << std::endl;
    current_size_ = file_ops_.size();
    if (current_size_ > 0)
    {
        Rotate();
        current_size_ = 0;
    }
}
void FileSink::Log(std::shared_ptr<LogEvent> event)
{
    MutexType::Lock lock(mutex_);
    std::string buffer = format_->Format(event);
    size_t new_size = current_size_ + buffer.size();
    if (new_size + buffer.size() > max_size_)
    {
        file_ops_.flush();
        Rotate();
        new_size = buffer.size();
    }
    file_ops_.write(buffer.c_str(), buffer.size());
    current_size_ = new_size;
}
void FileSink::Flush()
{
    MutexType::Lock lock(mutex_);
    file_ops_.flush();
}
void FileSink::SetFormat(std::shared_ptr <LogFormat> format)
{
    MutexType::Lock lock(mutex_);
    format_ = std::move(format);
}
void FileSink::SetLevel(LogLevel log_level)
{
    MutexType::Lock lock(mutex_);
    level_ = log_level;
}
LogLevel FileSink::GetLevel()
{
    MutexType::Lock lock(mutex_);
    return level_;
}
void FileSink::Rotate()
{
    file_ops_.close();
    std::string oldest_file = file_name_ + "." + std::to_string(max_files_);
    if (Nazl::path_exists(oldest_file))
    {
        std::remove(oldest_file.c_str());
    }
    for (int i = max_files_ - 1; i >= 0; --i)
    {
        std::string src = (i == 0) ? file_name_ : file_name_ + "." + std::to_string(i);
        std::string target = file_name_ + "." + std::to_string(i + 1);

        if (Nazl::path_exists(src))
        {
            ::rename(src.c_str(), target.c_str());
        }
    }
    file_ops_.open(file_name_);
}

std::string LogLevel::GetLevelString()
{
    switch (level_)
    {
    case LevelEnum::Debug:
        return "DEBUG";
    case LevelEnum::Info:
        return "INFO";
    case LevelEnum::Warn:
        return "WARNING";
    case LevelEnum::Error:
        return "ERROR";
    case LevelEnum::Fatal:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

void Logger::SetFormatter(std::shared_ptr <LogFormat> format)
{
    for (auto it = sinks_.begin(); it != sinks_.end(); ++it)
    {
        (*it)->SetFormat(std::move(format));
    }
}
void Logger::SinkIt(std::shared_ptr<LogEvent> event)
{
    for (auto it = sinks_.begin(); it != sinks_.end(); ++it)
    {
        (*it)->Log(event);
    }
}
std::shared_ptr<Logger> LogManager::GetLogger(const std::string &name)
{
    MutexType::Lock lock(map_mutex_);
    auto found = loggers_.find(name);
    return (found == loggers_.end()) ? nullptr : found->second;
}
std::shared_ptr<Logger> LogManager::GetDefaultLogger()
{
    MutexType::Lock lock(map_mutex_);
    if (loggers_.empty())
    {
        return nullptr;
    }
    return loggers_.begin()->second;
}

void LogManager::RegisterLogger(std::shared_ptr <Logger> logger)
{
    auto logger_name = logger->GetName();
    loggers_[logger_name] = std::move(logger);
}
/*
log:
- name: test_log
level: info
pattern: "[%Y-%m-%d %H:%M:%S] [%l] %v"
file:
enabled: true
filename: "logs/app.log"
max_size: 1048576  # 1 MB
max_files: 3
console:
enabled: true
- name: test_log2
level: debug
*/

int32_t log_init()
{
    std::shared_ptr <StdoutSink> sink = std::make_shared<StdoutSink>();
    sink->SetLevel(LogLevel(LogLevel::LevelEnum::Info));
    std::shared_ptr <FileSink> file_sink = std::make_shared<FileSink>("logs/app.log", 1024 * 1024, 10);
    sink->SetLevel(LogLevel(LogLevel::LevelEnum::Info));
    std::vector<std::shared_ptr<Sink>> sinks = {/*sink,*/ file_sink};
    std::shared_ptr<Logger> logger = std::make_shared<Logger>("default", sinks.begin(), sinks.end());
    logger->SetFormatter(std::make_shared<LogFormat>());
    LogManager& logManager = logger_manager::GetInstance();
    logManager.RegisterLogger(logger);
    return 0;
}
