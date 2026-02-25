/******************************************************************************
Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/

#include "ad5940.h"

typedef struct
{
  uint32_t ADCPgaGain;
  float ADCRefVolt;
  uint32_t SettlingTime10us;
} AppDiffVoltageCfg_Type;

void AppDiffVoltageCfgInit(AppDiffVoltageCfg_Type *pCfg)
{
  if(pCfg == NULL)
    return;

  pCfg->ADCPgaGain = ADCPGA_1P5;
  pCfg->ADCRefVolt = 1.82f;
  pCfg->SettlingTime10us = 16u * 25u;
}

AD5940Err AppDiffVoltageMeasure(const AppDiffVoltageCfg_Type *pCfg, float *pVoltage)
{
  uint32_t afeResult;
  uint32_t timeout;

  if((pCfg == NULL) || (pVoltage == NULL))
    return AD5940ERR_PARA;

  AD5940_ADCMuxCfgS(ADCMUXP_AIN3, ADCMUXN_AIN2);

  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_SINC2NOTCH, bTRUE);
  AD5940_Delay10us(pCfg->SettlingTime10us);

  AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE); /* Start ADC conversion */

  timeout = 1000000u;
  while((!AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_SINC2RDY)) && (timeout > 0u))
  {
    timeout--;
  }

  if(timeout == 0u)
  {
    AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_SINC2NOTCH, bFALSE);
    return AD5940ERR_APPERROR;
  }

  AD5940_INTCClrFlag(AFEINTSRC_SINC2RDY);
  afeResult = AD5940_ReadAfeResult(AFERESULT_SINC2);

  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_SINC2NOTCH, bFALSE);

  *pVoltage = AD5940_ADCCode2Volt(afeResult & 0xFFFFu, pCfg->ADCPgaGain, pCfg->ADCRefVolt);
  return AD5940ERR_OK;
}
