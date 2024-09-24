cmake_minimum_required(VERSION 3.0.2)

project(runtime)

set(CMAKE_INCLUDE_CURRENT_DIR true)

set(CXX_CPP
    runtime.cpp
    runtime.json
    )

set(CXX_H
    runtime.h
    )

add_library(${PROJECT_NAME}
    SHARED
    ${CXX_CPP}
    ${CXX_H}
    )

target_link_libraries(${PROJECT_NAME}
    framework
    base
    services
    ${QtUseModules}
    ${PkgUserModules}
    )

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${PLUGIN_INSTALL_PATH})

