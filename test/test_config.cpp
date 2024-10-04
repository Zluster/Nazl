//
// Created by zwz on 2024/10/4.
//
#include "config.h"
int main()
{
    Nazl::Config config("../conf/log_config.yml");
    if (config.loadItemsFromYaml())
    {
        for (const auto &item : config.getItems())
        {
            auto configItem = std::dynamic_pointer_cast<Nazl::ConfigItem<std::string>>(item.second);
            if (configItem)
            {
                std::cout << item.first << ": " << configItem->toString() << std::endl;
            }
            else
            {
                std::cerr << "Failed to cast config item for key: " << item.first << std::endl;
            }
        }
    }

    return 0;
}