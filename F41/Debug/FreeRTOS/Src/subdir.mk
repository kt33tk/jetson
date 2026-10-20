################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FreeRTOS/Src/croutine.c \
../FreeRTOS/Src/event_groups.c \
../FreeRTOS/Src/heap_4.c \
../FreeRTOS/Src/list.c \
../FreeRTOS/Src/port.c \
../FreeRTOS/Src/queue.c \
../FreeRTOS/Src/stream_buffer.c \
../FreeRTOS/Src/tasks.c \
../FreeRTOS/Src/timers.c 

OBJS += \
./FreeRTOS/Src/croutine.o \
./FreeRTOS/Src/event_groups.o \
./FreeRTOS/Src/heap_4.o \
./FreeRTOS/Src/list.o \
./FreeRTOS/Src/port.o \
./FreeRTOS/Src/queue.o \
./FreeRTOS/Src/stream_buffer.o \
./FreeRTOS/Src/tasks.o \
./FreeRTOS/Src/timers.o 

C_DEPS += \
./FreeRTOS/Src/croutine.d \
./FreeRTOS/Src/event_groups.d \
./FreeRTOS/Src/heap_4.d \
./FreeRTOS/Src/list.d \
./FreeRTOS/Src/port.d \
./FreeRTOS/Src/queue.d \
./FreeRTOS/Src/stream_buffer.d \
./FreeRTOS/Src/tasks.d \
./FreeRTOS/Src/timers.d 


# Each subdirectory must supply rules for building sources it contributes
FreeRTOS/Src/%.o FreeRTOS/Src/%.su FreeRTOS/Src/%.cyclo: ../FreeRTOS/Src/%.c FreeRTOS/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I"D:/git/F41/FreeRTOS/Inc" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FreeRTOS-2f-Src

clean-FreeRTOS-2f-Src:
	-$(RM) ./FreeRTOS/Src/croutine.cyclo ./FreeRTOS/Src/croutine.d ./FreeRTOS/Src/croutine.o ./FreeRTOS/Src/croutine.su ./FreeRTOS/Src/event_groups.cyclo ./FreeRTOS/Src/event_groups.d ./FreeRTOS/Src/event_groups.o ./FreeRTOS/Src/event_groups.su ./FreeRTOS/Src/heap_4.cyclo ./FreeRTOS/Src/heap_4.d ./FreeRTOS/Src/heap_4.o ./FreeRTOS/Src/heap_4.su ./FreeRTOS/Src/list.cyclo ./FreeRTOS/Src/list.d ./FreeRTOS/Src/list.o ./FreeRTOS/Src/list.su ./FreeRTOS/Src/port.cyclo ./FreeRTOS/Src/port.d ./FreeRTOS/Src/port.o ./FreeRTOS/Src/port.su ./FreeRTOS/Src/queue.cyclo ./FreeRTOS/Src/queue.d ./FreeRTOS/Src/queue.o ./FreeRTOS/Src/queue.su ./FreeRTOS/Src/stream_buffer.cyclo ./FreeRTOS/Src/stream_buffer.d ./FreeRTOS/Src/stream_buffer.o ./FreeRTOS/Src/stream_buffer.su ./FreeRTOS/Src/tasks.cyclo ./FreeRTOS/Src/tasks.d ./FreeRTOS/Src/tasks.o ./FreeRTOS/Src/tasks.su ./FreeRTOS/Src/timers.cyclo ./FreeRTOS/Src/timers.d ./FreeRTOS/Src/timers.o ./FreeRTOS/Src/timers.su

.PHONY: clean-FreeRTOS-2f-Src

