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
set(INC_LIB_CAL  ${LIB}/FLASH_readwrite/Inc ${LIB}/USBprint/Inc ${LIB}/Util/Inc)
set(INC_LIB   ${LIB}/ADCMonitor/Inc ${LIB}/circularBuffer/Inc ${LIB}/Filtering/Inc 
              ${LIB}/FLASH_readwrite/Inc ${LIB}/I2C/Inc ${LIB}/jumpToBootloader/Inc 
              ${LIB}/Regulation/Inc ${LIB}/SPI/Inc ${LIB}/TransformationFunctions/Inc 
              ${LIB}/USBprint/Inc ${LIB}/Util/Inc)
set(UT_LIB ../../CA_Embedded_Libraries/unit_testing)
set(UT_FAKES ${UT_LIB}/fakes)
set(UT_STUBS ${UT_LIB}/stubs)
set(DRIV ../../STM32/Current/Drivers/STM32F4xx_HAL_Driver)
set(CaCMSISInterface ../../CA_Embedded_Libraries/STM32/CaCMSISInterface)
set(DSP ../../CA_Embedded_Libraries/STM32/FirmwarePackages/STM32F401CCUx/Drivers/CMSIS/DSP)
set(DSP_STAT ${DSP}/Source/StatisticsFunctions)
set(DSP_BASIC ${DSP}/Source/BasicMathFunctions)
set(DSP_TRANS ${DSP}/Source/TransformFunctions)
set(DSP_TABLES ${DSP}/Source/CommonTables)
set(DSP_INC ${DSP}/Include ${DSP}/PrivateInclude)
set(DSP_SRC "${DSP_BASIC}/arm_shift_q15.c" "${DSP_TABLES}/arm_common_tables.c" "${DSP_TABLES}/arm_const_structs.c"
             "${DSP_STAT}/arm_absmax_q15.c" "${DSP_TRANS}/arm_bitreversal.c" "${DSP_TRANS}/arm_bitreversal2.c" 
             "${DSP_TRANS}/arm_cfft_init_q15.c" "${DSP_TRANS}/arm_cfft_q15.c" "${DSP_TRANS}/arm_cfft_radix4_init_q15.c" 
             "${DSP_TRANS}/arm_cfft_radix4_q15.c" "${DSP_TRANS}/arm_rfft_init_q15.c" "${DSP_TRANS}/arm_rfft_q15.c")

####################################################################################################
## List of tests to run
###################################################################################################

enable_testing()

include(GoogleTest)

# Current tests
add_executable(current_test current_tests.cpp ${UT_STUBS}/stub_jumpToBootloader.cpp 
                            ${UT_FAKES}/fake_HAL_otp.cpp ${UT_FAKES}/fake_stm32xxxx_hal.cpp ${UT_FAKES}/fake_StmGpio.cpp 
                            ${UT_FAKES}/fake_USBprint.cpp ${UT_FAKES}/fake_FLASH_readwrite.cpp ${CaCMSISInterface}/Src/ca_rfft.c 
                            ${DSP_SRC} ${UT_LIB}/Util/serialStatus_tests.cpp)
target_include_directories(current_test PRIVATE ${UT_FAKES} ${UT_STUBS} ${SRC}/Current/Core/Src ${SRC}/Current/Core/Inc ${LIB}/Util/Src ${LIB}/ADCMonitor/Src ${LIB}/Filtering/Src ${INC_LIB} ${DRIV}/Inc ${DRIV}/../CMSIS/Device/ST/STM32F4xx/Include ${DRIV}/../CMSIS/Include ${CaCMSISInterface}/Inc ${DSP_INC} ${UT_LIB}/Util)
target_link_libraries(current_test GTest::gtest_main gmock_main)
target_compile_definitions(current_test PUBLIC UNIT_TESTING __GNUC_PYTHON__)
target_compile_options(current_test PRIVATE -Wall)
gtest_discover_tests(current_test)
