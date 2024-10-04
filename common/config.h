//
// Created by zwz on 2024/9/9.
//

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H
#include <sstream>
#include <memory>
#include <variant>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <regex>
#include "mutex.h"
namespace Nazl
{
class BaseConfigItem
{
public:
    virtual ~BaseConfigItem() = default;
    virtual bool fromString(const std::string &str) = 0;
    virtual std::string toString() const = 0;
};
template <typename T>
class ConfigItem: public BaseConfigItem
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

    bool fromString(const std::string &str) override
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

    std::string toString() const override
    {
        std::ostringstream oss;
        oss << value_;
        return oss.str();
    }

private:
    std::string name_;
    T value_;
};


class Config
{
public:
    using MutexType = Nazl::Mutex;

    Config(const std::string &fileName) : fileName_(fileName) {}

    ~Config() = default;

    bool hasItem(const std::string &name, bool exactMatch = true);
    template <typename T>
    std::shared_ptr<ConfigItem<T>> getItem(const std::string &name);
    const std::unordered_map<std::string, std::shared_ptr<BaseConfigItem>>& getItems() const
    {
        return items_;
    }
    bool loadItemsFromYaml();

private:
    void loadFromYamlNode(const YAML::Node &node, const std::string &prefix = "");

    std::string fileName_;
    std::unordered_map<std::string, std::shared_ptr<BaseConfigItem>> items_;
    MutexType mutex_;
};

/*template <typename T>
std::shared_ptr<ConfigItem<T>> Config::getItem(const std::string &name)
{
    MutexType::Lock lock(mutex_);
    auto it = items_.find(name);
    if (it != items_.end())
    {
        return std::dynamic_pointer_cast<ConfigItem<T>>(it->second);
    }
    return nullptr;
}*/
template <typename T>
std::shared_ptr<ConfigItem<T>> Config::getItem(const std::string &name)
{
    MutexType::Lock lock(mutex_);
    auto it = items_.find(name);
    if (it != items_.end())
    {
        auto item = std::dynamic_pointer_cast<ConfigItem<T>>(it->second);
        if (!item)
        {
            std::cerr << "Type mismatch for key: " << name << std::endl;
        }
        return item;
    }
    std::cerr << "Configuration item not found for key: " << name << std::endl;
    return nullptr;
}
}



#endif //COMMON_CONFIG_H
