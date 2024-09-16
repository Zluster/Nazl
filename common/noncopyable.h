//
// Created by zwz on 2024/9/2.
//

#ifndef COMMON_NONCOPYABLE_H
#define COMMON_NONCOPYABLE_H
namespace Nazl {

class Noncopyable {
public:
    Noncopyable() = default;

    ~Noncopyable() = default;

    Noncopyable(const Noncopyable &) = delete;

    Noncopyable &operator=(const Noncopyable &) = delete;
};
}
#endif //COMMON_NONCOPYABLE_H
