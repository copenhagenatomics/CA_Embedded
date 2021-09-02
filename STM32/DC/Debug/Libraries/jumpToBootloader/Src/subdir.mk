################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/jumpToBootloader/Src/jumpToBootloader.c 

OBJS += \
./Libraries/jumpToBootloader/Src/jumpToBootloader.o 

C_DEPS += \
./Libraries/jumpToBootloader/Src/jumpToBootloader.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/jumpToBootloader/Src/jumpToBootloader.o: /home/matias/Documents/ws/CA_Embedded/STM32/Libraries/jumpToBootloader/Src/jumpToBootloader.c Libraries/jumpToBootloader/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F401xC -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/USBprint/Inc" -I"/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/jumpToBootloader/Inc" -I"/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/handleGenericMessages/Inc" -I"/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/circularBuffer/Inc" -I"/home/matias/Documents/ws/CA_Embedded/STM32/Libraries/SI7051/Inc" -I"/home/matias/Documents/ws/CA_Embedded/STM32/DC/InputValidation/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Libraries/jumpToBootloader/Src/jumpToBootloader.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

