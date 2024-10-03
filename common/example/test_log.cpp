//
// Created by zwz on 2024/9/10.
//
#include <iostream>
#include "log.h"
int main()
{
    std::cout << "hello world" << std::endl;
    log_init();
    while (1)
    {
        LOG_INFO("aaaa");
        LOG_DEBUG("bbbb");
        LOG_ERROR("cccc");
    }


    return 0;
}