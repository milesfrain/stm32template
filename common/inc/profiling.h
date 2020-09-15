/*
 * For rtos task profiling
 * https://blog.the78mole.de/freertos-debugging-on-stm32-cpu-usage/
 *
 * Unfortunately, names for event objects and queues don't work.
 * https://mcuoneclipse.com/2017/03/18/better-freertos-debugging-in-eclipse/
 */

volatile unsigned long ulHighFrequencyTimerTicks;

void configureTimerForRunTimeStats(void)
{
  ulHighFrequencyTimerTicks = 0;
  HAL_TIM_Base_Start_IT(&htim11);
}

unsigned long getRunTimeCounterValue(void)
{
  return ulHighFrequencyTimerTicks;
}
