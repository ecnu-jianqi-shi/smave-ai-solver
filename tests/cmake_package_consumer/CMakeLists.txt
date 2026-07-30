cmake_minimum_required(VERSION 3.20)
project(smave_package_consumer LANGUAGES C CXX)

find_package(SMAVE 0.1 CONFIG REQUIRED)

add_executable(smave-package-consumer main.c)
target_link_libraries(smave-package-consumer PRIVATE SMAVE::smave)

add_executable(smave-raii-package-consumer raii.cpp)
target_link_libraries(smave-raii-package-consumer PRIVATE SMAVE::cpp)
