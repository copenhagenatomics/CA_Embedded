####################################################################################################
## Required to install gtest dependency
####################################################################################################

cmake_minimum_required(VERSION 3.14)
project(unit_testing)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(SRC ../STM32)
set(LIB ../CA_Embedded_Libraries/STM32)
set(INC_LIB ${LIB}/ADCMonitor/Inc ${LIB}/circularBuffer/Inc ${LIB}/Filtering/Inc 
            ${LIB}/FLASH_readwrite/Inc ${LIB}/I2C/Inc ${LIB}/jumpToBootloader/Inc 
            ${LIB}/Regulation/Inc ${LIB}/SPI/Inc ${LIB}/TransformationFunctions/Inc 
            ${LIB}/USBprint/Inc ${LIB}/Util/Inc)
set(DRIV ../STM32/AC/Drivers/STM32F4xx_HAL_Driver)

####################################################################################################
## List of tests to run
###################################################################################################

enable_testing()

include(GoogleTest)

# AC tests
add_executable(heatCtrl_test AC/heatCtrl_tests.cpp ${LIB}/Util/Src/time32.c fakes/fake_stm32xxxx_hal.cpp fakes/fake_StmGpio.cpp)
target_include_directories(heatCtrl_test PRIVATE fakes ${LIB}/Util/Inc ${SRC}/AC/HeatCtrl/Inc ${SRC}/AC/HeatCtrl/Src)
target_link_libraries(heatCtrl_test GTest::gtest_main gmock_main)
gtest_discover_tests(heatCtrl_test)

add_executable(ac_test AC/AC_tests.cpp stubs/stub_jumpToBootloader.cpp fakes/fake_systemInfo.cpp fakes/fake_USBprint.cpp ${LIB}/Util/Src/time32.c fakes/fake_stm32xxxx_hal.cpp fakes/fake_StmGpio.cpp fakes/fake_HAL_otp.cpp)
target_include_directories(ac_test PRIVATE fakes ${SRC}/AC/Core/Src ${SRC}/AC/Core/Inc ${SRC}/AC/HeatCtrl/Inc ${SRC}/AC/HeatCtrl/Src ${LIB}/ADCMonitor/Src ${LIB}/Util/Src ${INC_LIB} ${DRIV}/Inc)
target_link_libraries(ac_test GTest::gtest_main gmock_main)
gtest_discover_tests(ac_test)
