/*
 * Application entry point.
 *
 * This file contains some of the same code that would have been
 * auto generated in main.c's main() if that option was enabled.
 *
 * These are mostly the MX_*_Init() calls.
 *
 * If you want to pick-up some fresh setup code after modifying the .ioc
 * file, temporarily re-enable the setting and see if anything new gets
 * put in main.c:
 * .ioc editor > Project > Do not generate main()
 * You'll need to disable that setting and regenerate for the project
 * to compile, otherwise there will be two conflicting definitions for main().
 *
 */

#include "main.h"
#include "cmsis_os.h"
#include "crc.h"
#include "dma.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
// Above includes are for initialization.

#include "dispatcher_task.h"
#include "fake_vfd_task.h"
#include "itm_logger_task.h" // dedicated logging task
#include "no_new.h"          // Traps unwanted usage of new or delete
#include "packet_flow_tasks.h"
#include "profiling.h" // include to enable rtos task profiling
#include "usb_task.h"
#include "vfd_task.h"

// Uncomment the following line to run a simulated VFD modbus server
#define USE_FAKE_VFD

// These functions are defined in C files.
// This block lets us use those functions here.
extern "C"
{
  void SystemClock_Config(void);
}

// Application entry point
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_UART4_Init();
  // MX_UART5_Init(); // conflict between 8 and 5
  MX_UART8_Init(); // conflict between 8 and 5
  MX_UART7_Init();
  MX_UART9_Init();
  MX_TIM11_Init();

  itmSendStringln("Starting...");

  // Create tasks

  static Watchdog watchdog;
  static ItmLogger logger(watchdog);

  // Allow watchdog to note timeouts via ItmLogger
  watchdog.logger = &logger;

  static TaskUtilitiesArg utilities(logger, watchdog);

  static UsbTask usbTask(utilities);
  static PacketIntake packetIntake("intake", usbTask, utilities);
  static PacketOutput packetOutput("output", usbTask, utilities);

  // Allow watchdog to note timeouts via PacketOutput
  watchdog.packetOutput = &packetOutput;

  // Modbus client running on uart port 8
  static HalfDuplexCallbacks uart8halfDuplex(GPIOE, LL_GPIO_PIN_14);
  static UartTasks uart8Tasks("uart8", uartInfo8, utilities, &uart8halfDuplex);
  static VfdTask vfdTask("vfdTask", uart8Tasks, packetOutput, utilities);

#ifdef USE_FAKE_VFD
  // Run a simulated VFD modbus server on uart port 9.
  // Must link uart ports 8 and 9 with loopback cable.
  static UartTasks uart9Tasks("uart9", uartInfo9, utilities);
  static FakeVfdTask fakeVfd("fakeVfd", uart9Tasks, utilities);
#endif

  static DispatcherTask dispatcherTask("dispatcherTask", packetIntake, vfdTask, packetOutput, utilities);

  /* Start scheduler */
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  critical();
}
