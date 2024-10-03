//
// Created by zwz on 2024/9/9.
//


#include "config.h"

namespace Nazl
{
template <typename T>
template <typename V>
std::shared_ptr<ConfigItem<V>> Config<T>::getItem(const std::string &name, const V &defaultValue)
{
    MutexType::Lock lock(mutex_);
    auto it = items_.find(name);
    if (it != items_.end())
    {
        return std::dynamic_pointer_cast<ConfigItem<V>>(it->second);
    }
    auto var = std::make_shared<ConfigItem<V>>(name, defaultValue);
    items_[name] = var;
    return var;
}

template <typename T>
bool Config<T>::loadItemsFromYaml()
{
    if (fileName_.empty())
    {
        return false;
    }
    YAML::Node root = YAML::LoadFile(fileName_);
    if (root.IsNull())
    {
        throw std::runtime_error("Failed to load YAML file: " + fileName_);
    }
    loadFromYamlNode(root);
    return true;
}

template <typename T>
void Config<T>::loadFromYamlNode(const YAML::Node &node, const std::string &prefix)
{
    if (node.IsMap())
    {
        for (const auto &entry : node)
        {
            const std::string key = prefix.empty() ? entry.first.as<std::string>() : prefix + "." + entry.first.as<std::string>();
            loadFromYamlNode(entry.second, key);
        }
    }
    else if (node.IsSequence())
    {
        for (size_t i = 0; i < node.size(); ++i)
        {
            const std::string key = prefix + "[" + std::to_string(i) + "]";
            loadFromYamlNode(node[i], key);
        }
    }
    else
    {
        auto item = getItem(prefix, node.as<T>());
        item->fromString(node.as<std::string>());
    }
}
}