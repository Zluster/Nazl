cmake_minimum_required(VERSION 3.10)

cmake_minimum_required(VERSION 3.10)

project(NazlConfigInstaller)

# Specify the configuration files to install
set(CONFIG_FILES
        log_config.yml
)

# Install configuration files to the conf directory
install(FILES ${CONFIG_FILES}
        DESTINATION conf
)