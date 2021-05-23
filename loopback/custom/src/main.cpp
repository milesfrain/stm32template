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

#include "itm_logger_task.h" // dedicated logging task
#include "itm_logging.h"     // ITM prints
#include "no_new.h"          // Traps unwanted usage of new or delete
#include "packet_flow_tasks.h"
#include "profiling.h" // include to enable rtos task profiling
#include "task_utilities.h"
#include "throughput_tasks.h"
#include "uart_tasks.h"
#include "usb_task.h"

// Define one of these for testing:
//#define TEST_UART_THROUGHPUT
//#define TEST_USB_IO
//#define TEST_USB_LOOPBACK
#define TEST_USB_PACKET_PARSING
//#define TEST_USB_SINGLE_UART_LOOPBACK
//#define TEST_USB_ALL_UART_LOOPBACK

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
  MX_UART5_Init();
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

#ifdef TEST_UART_THROUGHPUT

  // Configures a producer / consumer pair of tasks for each UART.
  // The producers generate packets.
  // The consumers parse packets and print diagnostic info (losses, etc.) to ITM logs.
  // This assumes all UARTs are hardwired as loopback, but it's also okay to hardwire
  // different UARTs to each other.

  static UartTasks uart4Tasks("uart4", uartInfo4, utilities);
  static Producer producer4("producer4", 4, uart4Tasks, utilities);
  static Consumer consumer4("consumer4", uart4Tasks, utilities);

  static UartTasks uart5Tasks("uart5", uartInfo5, utilities);
  static Producer producer5("producer5", 5, uart5Tasks, utilities);
  static Consumer consumer5("consumer5", uart5Tasks, utilities);

  static UartTasks uart7Tasks("uart7", uartInfo7, utilities);
  static Producer producer7("producer7", 7, uart7Tasks, utilities);
  static Consumer consumer7("consumer7", uart7Tasks, utilities);

  /*
  // Cannot enable both uart 5 and uart 8.
  // uart 5 rx and uart 8 tx conflict on dma1 stream 0
  static UartTasks uart8Tasks("uart8", uartInfo8, utilities);
  static Producer producer8("producer8", 8, uart8Tasks, utilities);
  static Consumer consumer8("consumer8", uart8Tasks, utilities);
  */

  static UartTasks uart9Tasks("uart9", uartInfo9, utilities);
  static Producer producer9("producer9", 9, uart9Tasks, utilities);
  static Consumer consumer9("consumer9", uart9Tasks, utilities);

#elif defined TEST_USB_IO

  // Tests usb input/output
  // The producer task outputs periodic messages that you can inspect on your PC's serial monitor.
  // The consumer task prints any messages sent to the uC in the ITM log (as hex).

  static UsbTask usbTask(utilities);
  static ProducerUsb producerUsb("producerUsb", usbTask, utilities);
  static ConsumerUsb consumerUsb("consumerUsb", usbTask, utilities);

#elif defined TEST_USB_LOOPBACK

  // Test USB loopback.
  // Anything written to usb will be echoed back.
  // Can reach 300 KBps (25x faster than 115200 UART baud rate), but
  // USB task will then consume 70% CPU.

  static UsbTask usbTask(utilities);
  static Coupling usbLoopback("usbLoop", usbTask, usbTask, utilities);

#elif defined TEST_USB_PACKET_PARSING

  // Test USB loopback where packets are parsed and echoed back
  // with the appropriate fields modified.

  static UsbTask usbTask(utilities);
  static PacketIntake packetIntake("intake", usbTask, utilities);
  static PacketOutput packetOutput("output", usbTask, utilities);
  static Coupling packetLoopback("coupling", packetIntake, packetOutput, utilities);

  // Allow watchdog to note timeouts via PacketOutput
  watchdog.packetOutput = &packetOutput;

#elif defined TEST_USB_SINGLE_UART_LOOPBACK

  // Pass USB data through UART
  // This assumes UART 5 is hardwired to itself

  static UsbTask usbTask(utilities);
  static UartTasks uart5Tasks("uart5", uartInfo5, utilities);
  static Coupling c1("usbToUart5", usbTask, uart5Tasks, utilities);
  static Coupling c2("uart5ToUsb", uart5Tasks, usbTask, utilities);

#elif defined TEST_USB_ALL_UART_LOOPBACK

  // Setup a big chain of all UARTs plus USB.
  // Any data written to USB will be echoed back (after passing through all UARTs).
  // This assumes the following hardwired connections:
  // - uart7 connected to uart9.
  // - uart4 and and uart5 are connected to themselves.

  // At theoretical maximum of 11.52 KBps (@ 115200 baud with start and stop bits),
  // consumes less than 8% of available CPU across 15 tasks.

  static UsbTask usbTask(utilities);
  static UartTasks uart7Tasks("uart7", uartInfo7, utilities);
  static UartTasks uart9Tasks("uart9", uartInfo9, utilities);
  static UartTasks uart5Tasks("uart5", uartInfo5, utilities);
  static UartTasks uart4Tasks("uart4", uartInfo4, utilities);

  static Coupling c1("usbToUart7", usbTask, uart7Tasks, utilities);
  // uart7 tx wired to uart9 rx
  static Coupling c2("uart9ToUart4", uart9Tasks, uart4Tasks, utilities);
  // uart4 wired to itself
  static Coupling c3("uart4ToUart5", uart4Tasks, uart5Tasks, utilities);
  // uart5 wired to itself
  static Coupling c4("uart5ToUart9", uart5Tasks, uart9Tasks, utilities);
  // uart9 tx wired to uart7 rx
  static Coupling c5("uart7ToUsb", uart7Tasks, usbTask, utilities);
#endif

  /* Start scheduler */
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  while (1)
    ;
}
