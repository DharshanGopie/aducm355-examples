/******************************************************************************
Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/

#include "ad5940.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint32_t ADCPgaGain;
  float ADCRefVolt;
  uint32_t SettlingTime10us;
} AppDiffVoltageCfg_Type;

void AppDiffVoltageCfgInit(AppDiffVoltageCfg_Type *pCfg);
AD5940Err AppDiffVoltageMeasure(const AppDiffVoltageCfg_Type *pCfg, float *pVoltage);

static AD5940Err AD5940PlatformCfg(const AppDiffVoltageCfg_Type *pCfg)
{
  CLKCfg_Type clk_cfg;
  DSPCfg_Type dsp_cfg;

  if(pCfg == NULL)
    return AD5940ERR_PARA;

  AD5940_Initialize();

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

  printf("\nAIN3-AIN2 differential voltage measurement started.\n");

  while(1)
  {
    err = AppDiffVoltageMeasure(&diffCfg, &voltage);
    if(err == AD5940ERR_OK)
      printf("AIN3-AIN2 = %0.6f V\n", voltage);
    else
      printf("No reading\n");

    AD5940_Delay10us(5000u * 100u); /* 5s */
  }
}
