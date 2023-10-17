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

####################################################################################################
## List of tests to run
###################################################################################################

enable_testing()

include(GoogleTest)

# HelloTest
add_executable(hello_test hello_test.cpp)
target_link_libraries(hello_test GTest::gtest_main)
gtest_discover_tests(hello_test)

# AC tests
add_executable(ac_test AC/AC_tests.cpp ${LIB}/Util/Src/time32.c fakes/fake_stm32xxxx_hal.cpp fakes/fake_StmGpio.c)
target_include_directories(ac_test PRIVATE fakes ${LIB}/Util/Inc ${SRC}/AC/HeatCtrl/Inc ${SRC}/AC/HeatCtrl/Src)
target_link_libraries(ac_test GTest::gtest_main gmock_main)
gtest_discover_tests(ac_test)