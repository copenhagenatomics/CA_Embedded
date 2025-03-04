####################################################################################################
## Required to install gtest dependency
####################################################################################################

cmake_minimum_required(VERSION 3.14)
project(unit_testing)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set timestamp policy to avoid warning (default value)
if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

####################################################################################################
## Setup source code locations / include locations
####################################################################################################

set(SRC ../../STM32)
set(LIB ../../CA_Embedded_Libraries/STM32)
set(INC_LIB ${LIB}/ADCMonitor/Inc ${LIB}/circularBuffer/Inc ${LIB}/Filtering/Inc 
            ${LIB}/FLASH_readwrite/Inc ${LIB}/I2C/Inc ${LIB}/jumpToBootloader/Inc 
            ${LIB}/Regulation/Inc ${LIB}/SPI/Inc ${LIB}/TransformationFunctions/Inc 
            ${LIB}/USBprint/Inc ${LIB}/Util/Inc)
set(UT_FAKES ../../CA_Embedded_Libraries/unit_testing/fakes)
set(UT_STUBS ../../CA_Embedded_Libraries/unit_testing/stubs)
set(DRIV ../../STM32/LightController/Drivers/STM32F4xx_HAL_Driver)

####################################################################################################
## List of tests to run
###################################################################################################

enable_testing()

include(GoogleTest)

# LightController test
add_executable(lightcontroller_test LightController_tests.cpp ${UT_STUBS}/stub_jumpToBootloader.cpp ${LIB}/Util/Src/systeminfo.c ${UT_FAKES}/fake_USBprint.cpp ${UT_FAKES}/fake_stm32xxxx_hal.cpp ${UT_FAKES}/fake_StmGpio.cpp ${UT_FAKES}/fake_HAL_otp.cpp)
target_include_directories(lightcontroller_test PRIVATE ${UT_FAKES} ${UT_STUBS} ${SRC}/LightController/Core/Src ${SRC}/LightController/Core/Inc ${LIB}/Util/Src ${INC_LIB} ${DRIV}/Inc ${DRIV}/../CMSIS/Device/ST/STM32F4xx/Include)
target_link_libraries(lightcontroller_test GTest::gtest_main gmock_main)
target_compile_definitions(lightcontroller_test PUBLIC UNIT_TESTING)
target_compile_options(lightcontroller_test PRIVATE -Wall)
gtest_discover_tests(lightcontroller_test)