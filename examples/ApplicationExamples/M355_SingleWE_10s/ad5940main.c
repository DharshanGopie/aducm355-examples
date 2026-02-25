/******************************************************************************
Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/

#include "ad5940.h"
#include "AD5940.h"
#include <stdio.h>
#include "string.h"
#include "math.h"
#include "Amperometric.h"
#include "PwrLib.h"
#include "RtcLib.h"

#define APPBUFF_SIZE 1000
uint32_t AppBuff[APPBUFF_SIZE];
float LFOSCFreq;
static volatile uint8_t RtcWakeupFlag = 1;

#define RTC_WAKEUP_INTERVAL_S    (10u)
#define RTC_TICKS_PER_SECOND     (32768u)

static void RTCWakeupInit(void)
{
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 0); /* Disable RTC while configuring */
  RtcSetPre(RTC1_PRESCALE_1);
  RtcSetCnt(0);
  RtcIntClrSR0(BITM_RTC_SR0_ALMINT);
  RtcCfgCR0(BITM_RTC_CR0_ALMEN | BITM_RTC_CR0_ALMINTEN, 1);
  NVIC_EnableIRQ(RTC1_EVT_IRQn);
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 1);
}

static void RTCScheduleWakeupIn10s(void)
{
  uint32_t rtc_cnt;
  uint16_t rtc_cnt2;
  uint32_t alarm_ticks;

  RtcGetSnap(&rtc_cnt, &rtc_cnt2);
  alarm_ticks = rtc_cnt + (RTC_WAKEUP_INTERVAL_S * RTC_TICKS_PER_SECOND);
  RtcSetAlarm(alarm_ticks, 0);
  RtcIntClrSR0(BITM_RTC_SR0_ALMINT);
}

static void EnterShutdownHibernate(void)
{
  AD5940_ReadReg(REG_AFE_ADCDAT); /* Wake AFE before shutdown command */
  AD5940_ShutDownS();
  PwrCfg(ENUM_PMG_PWRMOD_HIBERNATE, MONITOR_VBAT_EN, 0);
}

/* It's your choice here what to do with the data. Here is just an example to print them to UART */
int32_t AMPShowResult(float *pData, uint32_t DataCount)
{
  AppAMPCfg_Type *pAmpCfg;
  AppAMPGetCfg(&pAmpCfg); 
  
  /* Print data*/
  for(int i=0;i<DataCount;i++)
  {
    printf("Index %i:, %.3f , uA\n", i, pAmpCfg->SensorCh0.ResultBuffer[i]);
  }        
return 0;
}

/* Initialize AD5940 basic blocks like clock */
static int32_t AD5940PlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  LFOSCMeasure_Type LfoscMeasure;
  /* Use hardware reset */
  AD5940_HWReset();
  /* Platform configuration */
  AD5940_Initialize();
  /* Step1. Configure clock */
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  AD5940_CLKCfg(&clk_cfg);
  
  /* Step2. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);           /* Enable all interrupt in Interrupt Controller 1, so we can check INTC flags */
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE);   /* Interrupt Controller 0 will control GP0 to generate interrupt to MCU */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  
  
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Enable AFE to enter sleep mode. */
  /* Measure LFOSC frequency */
  LfoscMeasure.CalDuration = 1000.0;  /* 1000ms used for calibration. */
  LfoscMeasure.CalSeqAddr = 0;
  LfoscMeasure.SystemClkFreq = 16000000.0f; /* 16MHz in this firmware. */
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  printf("Freq:%f\n", LFOSCFreq); 
  return 0;
}

/* !!Change the application parameters here if you want to change it to none-default value */
void AD5940AMPStructInit(void)
{
  AppAMPCfg_Type *pAmpCfg;
  
  AppAMPGetCfg(&pAmpCfg);
  pAmpCfg->WuptClkFreq = LFOSCFreq;
  /* Configure general parameters */
  pAmpCfg->SeqStartAddr = 0;
  pAmpCfg->MaxSeqLen = 512;     /* @todo add checker in function */  
  pAmpCfg->RcalVal = 200.0;     /* Value of RCAl on board */
  pAmpCfg->NumOfData = -1;      /* Never stop until you stop it mannually by AppAMPCtrl() function */	
  pAmpCfg->AmpODR = 1;          /* Make one Gas reading every second */
  
  pAmpCfg->M355FifoThresh = 10;  
  
  /* Configure EC Sensor Parameters */
  //CO Sensor is connected to CH0 on EVAL-ADuCM355QSPZ
  pAmpCfg->SensorCh0.LpTiaRf = LPTIARF_1M;              /* 1Mohm Rfilter, 4.7uF cap connected external on AIN4 */
  pAmpCfg->SensorCh0.LpTiaRl = LPTIARLOAD_10R;          /* CO sensor datasheet specifies 10ohm Rload */
  pAmpCfg->SensorCh0.LptiaRtiaSel = LPTIARTIA_20K;      /* LPTIA gain resistor is 30kohm */
  pAmpCfg->SensorCh0.Vzero = 1100;                       /* Set Vzero = 500mV. VOltage on SE0 pin*/
  pAmpCfg->SensorCh0.SensorBias = -150;                    /* 0V bias voltage */
  
}

void AD5940_Main(void)
{  
  uint32_t temp;
  AppAMPCfg_Type *pAmpCfg;
  RTCWakeupInit();
  
  while(1)
  {
    if(RtcWakeupFlag)
    {
      RtcWakeupFlag = 0;
      AppAMPGetCfg(&pAmpCfg);
      AD5940PlatformCfg();
      AD5940AMPStructInit(); /* Configure your parameters in this function */
      pAmpCfg->NumOfData = 1;
      pAmpCfg->M355FifoThresh = 1;
      pAmpCfg->AMPInited = bFALSE;      /* Sequencer SRAM content is lost after shutdown, force regen */
      pAmpCfg->bParaChanged = bTRUE;

      AppECSnrInit(&pAmpCfg->SensorCh0, M355_CHAN0);
      AppAMPInit(AppBuff, APPBUFF_SIZE);
      AD5940_ClrMCUIntFlag();
      AppAMPCtrl(AMPCTRL_START, 0);

      while(AD5940_GetMCUIntFlag() == 0);
      AD5940_ClrMCUIntFlag();
      AppAMPISR(AppBuff, &temp);
      AMPShowResult((float*)AppBuff, temp);
      RTCScheduleWakeupIn10s();
      EnterShutdownHibernate();
    }
  }
}

void RTC1_Int_Handler(void)
{
  if(pADI_RTC1->SR0 & BITM_RTC_SR0_ALMINT)
  {
    RtcIntClrSR0(BITM_RTC_SR0_ALMINT);
    RtcWakeupFlag = 1;
  }
}
