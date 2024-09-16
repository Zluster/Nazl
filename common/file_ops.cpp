//
// Created by zwz on 2024/9/17.
//
#include "file_ops.h"
namespace Nazl {

bool path_exists(const std::string& file_name) {
    struct stat st;
    return (::stat(file_name.c_str(), &st) == 0);
}

FileOps::FileOps(FileOps&& other) noexcept
    : fd_(other.fd_), file_name_(std::move(other.file_name_)) {
    other.fd_ = nullptr;
}

FileOps& FileOps::operator=(FileOps&& other) noexcept {
    if (this != &other) {
        if (fd_ != nullptr) {
            close();
        }
        fd_ = other.fd_;
        file_name_ = std::move(other.file_name_);
        other.fd_ = nullptr;
    }
    return *this;
}

FileOps::~FileOps() {
    if (fd_ != nullptr) {
        close();
    }
}

void FileOps::open(const std::string& file_name, bool truncate) {
    if (fd_ != nullptr) {
        close();
    }
    file_name_ = file_name;
    if (!path_exists(file_name_)) {
        ::mkdir(file_name_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    if (truncate) {
        ::unlink(file_name_.c_str());
    }
    fd_ = ::fopen(file_name_.c_str(), truncate ? "w+b" : "r+b");
}

void FileOps::flush() {
    if (fd_ != nullptr) {
        ::fflush(fd_);
    }
}

void FileOps::close() {
    if (fd_ != nullptr) {
        fclose(fd_);
        fd_ = nullptr;
    }
}

int32_t FileOps::write(const char* data, size_t size) {
    if (fd_ != nullptr) {
        return static_cast<int32_t>(::fwrite(data, 1, size, fd_));
    }
    return 0;
}

size_t FileOps::size() const {
    struct stat64 st;
    if (::fstat64(fileno(fd_), &st) == 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

std::string FileOps::filename() const {
    return file_name_;
}

} // namespace Nazl