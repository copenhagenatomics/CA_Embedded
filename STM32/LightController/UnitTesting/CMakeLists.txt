# to generate project for Eclipse.
# cmake -G "Eclipse CDT4 - Unix Makefiles" ./

cmake_minimum_required(VERSION 3.13)

project(CA_UnitTesting)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O0 -g3")
set(CMAKE_C_FLAGS "-O0 -g3")

include_directories(../Core/Inc
                    ../../../CA_Embedded_Libraries/STM32/Util/Inc
                    ../../../CA_Embedded_Libraries/STM32/FLASH_readwrite/Inc
                    ../../../CA_Embedded_Libraries/STM32/USBprint/Inc
                    ../../../CA_Embedded_Libraries/STM32/circularBuffer/Inc
                    ../../../CA_Embedded_Libraries/STM32/jumpToBootloader/Inc
                    ../Drivers/CMSIS/Include/
                    ../Drivers/STM32F4xx_HAL_Driver/Inc/
					../Drivers/CMSIS/Device/ST/STM32F4xx/Include
					../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
					../Middlewares/ST/STM32_USB_Device_Library/Core/Inc
                    ../USB_DEVICE/App
                    ../USB_DEVICE/Target
                    Inc)

add_compile_definitions(STM32F401xC)

enable_testing()
add_executable(CA_UnitTesting ../Core/Src/LightController.c Src/lightContollerTest.cpp)

# Suppresses -Wint-to-pointer-cast from building on a 64bit system. 
# Be careful not to miss other warnings when making a project. 
target_compile_options(CA_UnitTesting PRIVATE -w) 
target_link_libraries(CA_UnitTesting PRIVATE m)
