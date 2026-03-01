cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(UltraHackPack VERSION 1.0.0)

# Подключаем Geode SDK
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# ⬇️ ВОТ ЭТА СТРОКА БЫЛА ПРОПУЩЕНА — создаём библиотеку
file(GLOB SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Настраиваем как Geode мод
setup_geode_mod(${PROJECT_NAME})
