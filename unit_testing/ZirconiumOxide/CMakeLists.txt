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

set(SRC ../../STM32/ZirconiumOxide)
set(LIB ../../CA_Embedded_Libraries/STM32)
set(INC_LIB ${LIB}/ADCMonitor/Inc ${LIB}/circularBuffer/Inc ${LIB}/Filtering/Inc 
            ${LIB}/FLASH_readwrite/Inc ${LIB}/I2C/Inc ${LIB}/jumpToBootloader/Inc 
            ${LIB}/SPI/Inc ${LIB}/USBprint/Inc ${LIB}/Util/Inc)
set(UT_LIB ../../CA_Embedded_Libraries/unit_testing)
set(UT_FAKES ${UT_LIB}/fakes)
set(UT_STUBS ${UT_LIB}/stubs)
set(DRIV ../../STM32/ZirconiumOxide/Drivers/STM32F4xx_HAL_Driver)

####################################################################################################
## List of tests to run
###################################################################################################

enable_testing()

include(GoogleTest)

# ZrO2 tests
add_executable(zro2_tests zro2_tests.cpp ${UT_FAKES}/fake_HAL_otp.cpp ${UT_FAKES}/fake_FLASH_readwrite.cpp ${UT_FAKES}/fake_StmGpio.cpp ${UT_STUBS}/stub_jumpToBootloader.cpp ${UT_FAKES}/fake_stm32xxxx_hal.cpp ${LIB}/Util/Src/systeminfo.c ${LIB}/Util/Src/time32.c ${LIB}/Filtering/Src/array-math.c ${UT_FAKES}/fake_USBprint.cpp ${UT_LIB}/Util/serialStatus_tests.cpp)
target_include_directories(zro2_tests PRIVATE ${INC_LIB} ${UT_FAKES} ${SRC}/Core/Src ${SRC}/Core/Inc ${DRIV}/Inc ${DRIV}/Src ${LIB}/Util/Src ${LIB}/ADCMonitor/Src/ ${DRIV}/../CMSIS/Device/ST/STM32F4xx/Include ${UT_LIB}/Util)
target_link_libraries(zro2_tests GTest::gtest_main gmock_main)
target_compile_definitions(zro2_tests PUBLIC UNIT_TESTING)
target_compile_options(zro2_tests PRIVATE -Wall)
gtest_discover_tests(zro2_tests)