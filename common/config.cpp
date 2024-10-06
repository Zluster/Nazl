//
// Created by zwz on 2024/9/9.
//


#include "config.h"

namespace Nazl
{


template <>
bool ConfigItem<std::string>::fromString(const std::string &str)
{
    setValue(str);
    return true;
}
bool Config::loadItemsFromYaml()
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

void Config::loadFromYamlNode(const YAML::Node &node, const std::string &prefix)
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
        if (node.IsScalar())
        {
            std::string valueStr = node.as<std::string>();
            std::regex intRegex("^-?\\d+$"); // Matches optional leading '-' followed by digits
            try
            {
                if (node.Tag() == "tag:yaml.org,2002:int" || std::regex_match(valueStr, intRegex))
                {
                    int value = node.as<int>();
                    auto item = std::make_shared<ConfigItem<int>>(prefix, value);
                    items_[prefix] = item;
                    std::cout << "Loaded config item (int): " << prefix << " = " << value << std::endl;
                }
                else if (node.Tag() == "tag:yaml.org,2002:bool")
                {
                    bool value = node.as<bool>();
                    auto item = std::make_shared<ConfigItem<bool>>(prefix, value);
                    items_[prefix] = item;
                    std::cout << "Loaded config item (bool): " << prefix << " = " << value << std::endl;
                }
                else
                {
                    std::string value = node.as<std::string>();
                    auto item = std::make_shared<ConfigItem<std::string>>(prefix, value);
                    items_[prefix] = item;
                    std::cout << "Loaded config item (string): " << prefix << " = " << item->toString() << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error loading config item for key: " << prefix << " - " << e.what() << std::endl;
            }
        }
    }
}

bool Config::hasItem(const std::string & name, bool exactMatch)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (exactMatch)
    {
        return items_.find(name) != items_.end();
    }
    else
    {
        for (const auto& item : items_)
        {
            if (item.first.find(name) == 0)
            {
                return true;
            }
        }
        return false;
    }
}
}