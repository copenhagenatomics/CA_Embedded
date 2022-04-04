# to generate project for Eclipse.
# cmake -G "Eclipse CDT4 - Unix Makefiles" ./

cmake_minimum_required(VERSION 3.13)

project(CA_UnitTesting)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O0 -g3")
set(CMAKE_C_FLAGS "-O0 -g3")

include_directories(../ADCMonitor/Inc
                    ../Util/Inc
                    ../USBprint/Inc
                    ../FLASH_readwrite/Inc
                    Inc )

enable_testing()
add_executable(CA_UnitTesting ../ADCMonitor/Src/ADCmonitor.c ../Util/Src/CAProtocol.c src/stubs.cpp)
target_link_libraries(CA_UnitTesting PRIVATE m)
