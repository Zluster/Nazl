//
// Created by zwz on 2024/10/3.
//
//
// Created by zwz on 2024/9/10.
//
#include <iostream>
#include "log.h"
int main()
{
    std::cout << "hello world" << std::endl;
    log_init("process1");

    LOG_INFO("aaaa");
    LOG_DEBUG("bbbb");
    LOG_ERROR("cccc");


    return 0;
}