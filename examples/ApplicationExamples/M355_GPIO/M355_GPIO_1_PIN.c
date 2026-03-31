/******************************************************************************
Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/

/*
   Description:
      - Configure P1.0 and P1.1 as digital GPIO outputs.
      - Toggle P1.0 every 10 seconds.
      - Keep P1.1 low.
*/

#include "ad5940.h"
#include "DioLib.h"
#include "ClkLib.h"

#define TOGGLE_PERIOD_10US   (1000000u)  /* 10 s = 1,000,000 x 10 us */

static void ClockInit(void);
static void GPIOInit(void);

int main(void)
{
   AD5940_Initialize();
   ClockInit();
   GPIOInit();

   while (1)
   {
      AD5940_Delay10us(TOGGLE_PERIOD_10US);
      DioTglPin(pADI_GPIO1, PIN0);           /* Switch P1.0 high/low every 10 s */
   }
}

static void GPIOInit(void)
{
   DioCfgPin(pADI_GPIO1, PIN0 | PIN1, 0u);   /* Configure P1.0/P1.1 as GPIO */
   DioOenPin(pADI_GPIO1, PIN0 | PIN1, 1u);   /* Configure P1.0/P1.1 as outputs */

   DioClrPin(pADI_GPIO1, PIN0);              /* Start P1.0 low */
   DioClrPin(pADI_GPIO1, PIN1);              /* Keep P1.1 low */
}

/* Clock digital die with internal 26 MHz oscillator. */
static void ClockInit(void)
{
   DigClkSel(DIGCLK_SOURCE_HFOSC);
   ClkDivCfg(1, 1);
}
