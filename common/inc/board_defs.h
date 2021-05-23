/*
 * Definitions for pins on board.
 * For supporting multiple boards, we may make some more specific files,
 * or #ifdef some sections in this file.
 */

#pragma once

#include "stm32f4xx_ll_gpio.h"

// todo - replace with GreenLedOn, etc.
#define GreenLedPort GPIOD
#define GreenLedPin GPIO_PIN_4
#define RedLedPort GPIOD
#define RedLedPin GPIO_PIN_5

// Can replace these with nop calls to disable gpio debugging in sections.

#if 0

#define UartTxDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_8)
#define UartTxDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_8)

#define UartRxDbgPinHigh() __NOP()
#define UartRxDbgPinLow() __NOP()

#else

#define UartTxDbgPinHigh() __NOP()
#define UartTxDbgPinLow() __NOP()

#define UartRxDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_8)
#define UartRxDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_8)

#endif

#if 1

#define LoggingDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_9)
#define LoggingDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_9)

#define VfdErrorDbgPinHigh() __NOP()
#define VfdErrorDbgPinLow() __NOP()

#else

#define LoggingDbgPinHigh() __NOP()
#define LoggingDbgPinLow() __NOP()

#define VfdErrorDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_9)
#define VfdErrorDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_9)

#endif

#if 1

#define UsbRxPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_10)
#define UsbRxPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_10)

#define ModbusDbgPinHigh() __NOP()
#define ModbusDbgPinLow() __NOP()

#else

#define UsbRxPinHigh() __NOP()
#define UsbRxPinLow() __NOP()

#define ModbusDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_10)
#define ModbusDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_10)

#endif

#define TaskTickDbgPinToggle() LL_GPIO_TogglePin(GPIOE, LL_GPIO_PIN_11)

#define IdleDbgPinHigh() LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_12)
#define IdleDbgPinLow() LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_12)