/******************************************************************************
Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/

#include "ad5940.h"
#include <stdio.h>
#include <string.h>
#include "RtcLib.h"

typedef struct
{
  uint32_t ADCPgaGain;
  float ADCRefVolt;
  uint32_t SettlingTime10us;
} AppDiffVoltageCfg_Type;

void AppDiffVoltageCfgInit(AppDiffVoltageCfg_Type *pCfg);
AD5940Err AppDiffVoltageMeasure(const AppDiffVoltageCfg_Type *pCfg, float *pVoltage);

#define PWRMODE_SHUTDOWN      3u
#define SAMPLE_PERIOD_SECONDS 10u

static volatile uint8_t gVoltageReadPending = 0u;

static AD5940Err AppDiffVoltageWarmup(const AppDiffVoltageCfg_Type *pCfg)
{
  AD5940Err err;
  float throwAway;

  /* First conversion after shutdown is used to settle the analog path. */
  err = AppDiffVoltageMeasure(pCfg, &throwAway);
  return err;
}

static void AD5940_PowerModeCfg(uint8_t PwrMode)
{
  if(PwrMode == PWRMODE_SHUTDOWN)
  {
    AD5940_ReadReg(REG_AFE_ADCDAT); /* Dummy read to wake AFE die if asleep */
    AD5940_ShutDownS();             /* Enter AFE shutdown mode */
  }
}

static AD5940Err AD5940ApplyMeasurementCfg(const AppDiffVoltageCfg_Type *pCfg)
{
  CLKCfg_Type clk_cfg;
  DSPCfg_Type dsp_cfg;

  if(pCfg == NULL)
    return AD5940ERR_PARA;

  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);

  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_AIN3;
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_AIN2;
  dsp_cfg.ADCBaseCfg.ADCPga = pCfg->ADCPgaGain;

  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_2;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_22;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_2;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bTRUE;
  dsp_cfg.ADCFilterCfg.BpNotch = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;

  memset(&dsp_cfg.DftCfg, 0, sizeof(dsp_cfg.DftCfg));
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  AD5940_DSPCfgS(&dsp_cfg);

  return AD5940ERR_OK;
}

static AD5940Err AD5940PlatformCfg(const AppDiffVoltageCfg_Type *pCfg)
{
  if(pCfg == NULL)
    return AD5940ERR_PARA;

  AD5940_Initialize();
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK); /* Allow AFE sleep/shutdown transitions */

  return AD5940ApplyMeasurementCfg(pCfg);
}

static void VoltageSampleTimerInit(void)
{
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 0);             /* Disable RTC during configuration */
  RtcSetPre(RTC1_PRESCALE_32768);               /* Prescaled interrupt period = 1s */
  RtcSetCnt(0);
  RtcIntClrSR2(BITM_RTC_SR2_PSINT);
  RtcCfgCR1(BITM_RTC_CR1_PSINTEN, 1);           /* Enable 1-second RTC prescaled interrupt */
  NVIC_EnableIRQ(RTC1_EVT_IRQn);
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 1);             /* Enable RTC */
}

void RTC1_Int_Handler(void)
{
  static uint8_t secondCounter = 0u;

  if((pADI_RTC1->SR2 & BITM_RTC_SR2_PSINT) != 0u)
  {
    RtcIntClrSR2(BITM_RTC_SR2_PSINT);

    secondCounter++;
    if(secondCounter >= SAMPLE_PERIOD_SECONDS)
    {
      secondCounter = 0u;
      gVoltageReadPending = 1u;
    }
  }
}

void AD5940_Main(void)
{
  AD5940Err err;
  AppDiffVoltageCfg_Type diffCfg;
  float voltage;

  AppDiffVoltageCfgInit(&diffCfg);
  err = AD5940PlatformCfg(&diffCfg);
  if(err != AD5940ERR_OK)
  {
    printf("AD5940 platform init failed: %d\n", err);
    return;
  }

  VoltageSampleTimerInit();
  gVoltageReadPending = 1u; /* Take first reading immediately */
  printf("\nAIN3-AIN2 differential voltage measurement started (10s RTC interrupt, shutdown between samples).\n");

  while(1)
  {
    if(gVoltageReadPending == 0u)
    {
      __WFI(); /* Sleep CPU until next interrupt */
      continue;
    }

    gVoltageReadPending = 0u;

    if(AD5940_WakeUp(10u) > 10u)
    {
      printf("Wakeup failed\n");
      continue;
    }

    err = AD5940ApplyMeasurementCfg(&diffCfg);
    if(err == AD5940ERR_OK)
      err = AppDiffVoltageWarmup(&diffCfg);
    if(err != AD5940ERR_OK)
    {
      printf("AFE reconfigure failed (err=%d)\n", err);
      continue;
    }

    err = AppDiffVoltageMeasure(&diffCfg, &voltage);

    if(err == AD5940ERR_OK)
      printf("AIN3-AIN2 = %0.6f V\n", voltage);
    else
      printf("No reading (err=%d)\n", err);

    AD5940_PowerModeCfg(PWRMODE_SHUTDOWN);
  }

}
