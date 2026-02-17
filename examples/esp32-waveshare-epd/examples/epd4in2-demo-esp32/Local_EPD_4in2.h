/*****************************************************************************
* | File      	:   Local_EPD_4in2.h
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper
* | Info        :
*----------------
* |	This version:   V3.1
* | Date        :   2019-11-14
* | Info        :
* -----------------------------------------------------------------------------
* Local Copy for Custom Init Fix
******************************************************************************/
#ifndef _LOCAL_EPD_4IN2_H_
#define _LOCAL_EPD_4IN2_H_

#include "DEV_Config.h"

// Display resolution
#define EPD_4IN2_WIDTH       400
#define EPD_4IN2_HEIGHT      300

void Local_EPD_4IN2_Init(void);
void Local_EPD_4IN2_Init_Fast(void);
void Local_EPD_4IN2_Init_Partial(void);
void Local_EPD_4IN2_Clear(void);
void Local_EPD_4IN2_Display(UBYTE *Image);
void Local_EPD_4IN2_Sleep(void);
void Local_EPD_4IN2_PartialDisplay(UWORD X_start,UWORD Y_start,UWORD X_end,UWORD Y_end, UBYTE *Image);

void Local_EPD_4IN2_Init_4Gray(void);
void Local_EPD_4IN2_4GrayDisplay(const UBYTE *Image);


#endif
