//
// Created by zwz on 2024/9/8.
//

#ifndef COMMON_FILE_OPS_H
#define COMMON_FILE_OPS_H
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include "noncopyable.h"
namespace Nazl {
class FileOps : public Noncopyable {
public:
    FileOps() = default;
    FileOps(FileOps&& other) noexcept;
    FileOps& operator=(FileOps&& other) noexcept;
    ~FileOps();

    void open(const std::string& file_name, bool truncate = false);
    void flush();
    void close();
    int32_t write(const char* data, size_t size);
    size_t size() const;
    std::string filename() const;

private:
    std::FILE* fd_{nullptr};
    std::string file_name_;
};
bool path_exists(const std::string& file_name);

}
#endif //COMMON_FILE_OPS_H
