//
// Created by zwz on 2024/9/9.
//

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H
#include <sstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include "mutex.h"
namespace Nazl
{
template <typename T>
class ConfigItem
{
public:
    using ptr = std::shared_ptr<ConfigItem<T>>;

    ConfigItem(const std::string &name, const T &value) : name_(name), value_(value) {}

    T getValue() const
    {
        return value_;
    }

    void setValue(const T &value)
    {
        value_ = value;
    }

    bool fromString(const std::string &str)
    {
        std::istringstream iss(str);
        T new_val;
        if (iss >> new_val)
        {
            setValue(new_val);
            return true;
        }
        return false;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << value_;
        return oss.str();
    }

private:
    std::string name_;
    T value_;
};

template <typename T>
class Config
{
public:
    using MutexType = Nazl::Mutex;

    Config(const std::string &fileName) : fileName_(fileName) {}

    ~Config() = default;

    template <typename V>
    std::shared_ptr<ConfigItem<V>> getItem(const std::string &name, const V &defaultValue);

    bool loadItemsFromYaml();

private:
    void loadFromYamlNode(const YAML::Node &node, const std::string &prefix = "");

    std::string fileName_;
    std::unordered_map<std::string, typename ConfigItem<T>::ptr> items_;
    MutexType mutex_;
};

}



#endif //COMMON_CONFIG_H
