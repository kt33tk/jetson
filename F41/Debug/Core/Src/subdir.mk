################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/CanRX.c \
../Core/Src/CanTX.c \
../Core/Src/Can_protocol.c \
../Core/Src/ControlCore.c \
../Core/Src/UART_cmd.c \
../Core/Src/Watchdog.c \
../Core/Src/control.c \
../Core/Src/crc8.c \
../Core/Src/log.c \
../Core/Src/main.c \
../Core/Src/sensor.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_hal_timebase_tim.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/test.c 

OBJS += \
./Core/Src/CanRX.o \
./Core/Src/CanTX.o \
./Core/Src/Can_protocol.o \
./Core/Src/ControlCore.o \
./Core/Src/UART_cmd.o \
./Core/Src/Watchdog.o \
./Core/Src/control.o \
./Core/Src/crc8.o \
./Core/Src/log.o \
./Core/Src/main.o \
./Core/Src/sensor.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_hal_timebase_tim.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/test.o 

C_DEPS += \
./Core/Src/CanRX.d \
./Core/Src/CanTX.d \
./Core/Src/Can_protocol.d \
./Core/Src/ControlCore.d \
./Core/Src/UART_cmd.d \
./Core/Src/Watchdog.d \
./Core/Src/control.d \
./Core/Src/crc8.d \
./Core/Src/log.d \
./Core/Src/main.d \
./Core/Src/sensor.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_hal_timebase_tim.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/test.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I"D:/git/F41/FreeRTOS/Inc" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/CanRX.cyclo ./Core/Src/CanRX.d ./Core/Src/CanRX.o ./Core/Src/CanRX.su ./Core/Src/CanTX.cyclo ./Core/Src/CanTX.d ./Core/Src/CanTX.o ./Core/Src/CanTX.su ./Core/Src/Can_protocol.cyclo ./Core/Src/Can_protocol.d ./Core/Src/Can_protocol.o ./Core/Src/Can_protocol.su ./Core/Src/ControlCore.cyclo ./Core/Src/ControlCore.d ./Core/Src/ControlCore.o ./Core/Src/ControlCore.su ./Core/Src/UART_cmd.cyclo ./Core/Src/UART_cmd.d ./Core/Src/UART_cmd.o ./Core/Src/UART_cmd.su ./Core/Src/Watchdog.cyclo ./Core/Src/Watchdog.d ./Core/Src/Watchdog.o ./Core/Src/Watchdog.su ./Core/Src/control.cyclo ./Core/Src/control.d ./Core/Src/control.o ./Core/Src/control.su ./Core/Src/crc8.cyclo ./Core/Src/crc8.d ./Core/Src/crc8.o ./Core/Src/crc8.su ./Core/Src/log.cyclo ./Core/Src/log.d ./Core/Src/log.o ./Core/Src/log.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/sensor.cyclo ./Core/Src/sensor.d ./Core/Src/sensor.o ./Core/Src/sensor.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_hal_timebase_tim.cyclo ./Core/Src/stm32f4xx_hal_timebase_tim.d ./Core/Src/stm32f4xx_hal_timebase_tim.o ./Core/Src/stm32f4xx_hal_timebase_tim.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/test.cyclo ./Core/Src/test.d ./Core/Src/test.o ./Core/Src/test.su

.PHONY: clean-Core-2f-Src

