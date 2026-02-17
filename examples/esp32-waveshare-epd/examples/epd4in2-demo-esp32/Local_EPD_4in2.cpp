/*****************************************************************************
* | File      	:   Local_EPD_4in2.cpp
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
#include "Local_EPD_4in2.h"
// #include "Debug.h"
#define Debug(__info,...) Serial.printf(__info,##__VA_ARGS__)

// Reduce Partial Buffer to Top Section (400x160)
// 400 * 160 / 8 = 8000 bytes
#define PARTIAL_WIDTH_LIMIT  EPD_4IN2_WIDTH
#define PARTIAL_HEIGHT_LIMIT EPD_4IN2_HEIGHT
static UBYTE EPD_4IN2_Partial_DATA[PARTIAL_WIDTH_LIMIT * PARTIAL_HEIGHT_LIMIT / 8] = {0x00};

static const unsigned char EPD_4IN2_lut_vcom0[] = {
    0x00, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x00, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 					
	};
static const unsigned char EPD_4IN2_lut_ww[] = {
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
static const unsigned char EPD_4IN2_lut_bw[] = {
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
static const unsigned char EPD_4IN2_lut_wb[] = {
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	};
static const unsigned char EPD_4IN2_lut_bb[] = {
	0x20, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x10, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	};

/******************************partial screen update LUT*********************************/
const unsigned char EPD_4IN2_Partial_lut_vcom1[] ={
    0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char EPD_4IN2_Partial_lut_ww1[] ={
    0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char EPD_4IN2_Partial_lut_bw1[] ={
    0x20, 0x01, 0x20, 0x01, 0x00, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

const unsigned char EPD_4IN2_Partial_lut_wb1[] ={
    0x10, 0x01, 0x20, 0x01, 0x00, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

const unsigned char EPD_4IN2_Partial_lut_bb1[] ={
    0x00, 0x01,0x20, 0x01, 0x00, 0x01, 
    0x00, 0x00,0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00,0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00,0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00,0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00,0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,0x00, 0x00, 0x00, 0x00, 
};


/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4IN2_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);

    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
	
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_4IN2_SendCommand(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_4IN2_SendData(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void Local_EPD_4IN2_ReadBusy(void)
{
    Debug("e-Paper busy\r\n");
	EPD_4IN2_SendCommand(0x71);
    while(DEV_Digital_Read(EPD_BUSY_PIN) == 0) {      //LOW: idle, HIGH: busy
		EPD_4IN2_SendCommand(0x71);
        DEV_Delay_ms(100);
    }
    Debug("e-Paper busy release\r\n");
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_4IN2_TurnOnDisplay(void)
{
    EPD_4IN2_SendCommand(0x12);
    DEV_Delay_ms(100);
    Local_EPD_4IN2_ReadBusy();
}

/******************************************************************************
function :	set the look-up tables
parameter:
******************************************************************************/
static void EPD_4IN2_Partial_SetLut(void)
{
	unsigned int count;
	EPD_4IN2_SendCommand(0x20);
	for(count=0;count<44;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_Partial_lut_vcom1[count]);}

	EPD_4IN2_SendCommand(0x21);
	for(count=0;count<42;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_Partial_lut_ww1[count]);}   
	
	EPD_4IN2_SendCommand(0x22);
	for(count=0;count<42;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_Partial_lut_bw1[count]);} 

	EPD_4IN2_SendCommand(0x23);
	for(count=0;count<42;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_Partial_lut_wb1[count]);} 

	EPD_4IN2_SendCommand(0x24);
	for(count=0;count<42;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_Partial_lut_bb1[count]);} 
}

static void EPD_4IN2_SetLut(void)
{
	unsigned int count;
	EPD_4IN2_SendCommand(0x20);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_vcom0[count]);}

	EPD_4IN2_SendCommand(0x21);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_ww[count]);}   
	
	EPD_4IN2_SendCommand(0x22);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_bw[count]);} 

	EPD_4IN2_SendCommand(0x23);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_wb[count]);} 

	EPD_4IN2_SendCommand(0x24);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_bb[count]);}   
}

/*
//LUT download
static void EPD_4IN2_4Gray_lut(void)
{
	unsigned int count;	 
	{
		EPD_4IN2_SendCommand(0x20);							//vcom
        // ... (LUTs not defined)
	}	         
}
*/
/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/

void Local_EPD_4IN2_Init_Partial(void)
{
    EPD_4IN2_Reset();

    EPD_4IN2_SendCommand(0x01); // POWER SETTING
    EPD_4IN2_SendData(0x03);
    EPD_4IN2_SendData(0x00);
    EPD_4IN2_SendData(0x2b);
    EPD_4IN2_SendData(0x2b);

    EPD_4IN2_SendCommand(0x06); // boost soft start
    EPD_4IN2_SendData(0x17);		//A
    EPD_4IN2_SendData(0x17);		//B
    EPD_4IN2_SendData(0x17);		//C

    EPD_4IN2_SendCommand(0x04); // POWER_ON
    Local_EPD_4IN2_ReadBusy();

    EPD_4IN2_SendCommand(0x00); // panel setting
    EPD_4IN2_SendData(0xbf); // KW-BF   KWR-AF	BWROTP 0f	BWOTP 1f

    EPD_4IN2_SendCommand(0x30); // PLL setting
    EPD_4IN2_SendData(0x3C); // 3A 100HZ   29 150Hz 39 200HZ	31 171HZ

    EPD_4IN2_SendCommand(0x61); // resolution setting
    EPD_4IN2_SendData(0x01);
    EPD_4IN2_SendData(0x90); //128
    EPD_4IN2_SendData(0x01); //
    EPD_4IN2_SendData(0x2c);

    EPD_4IN2_SendCommand(0x82); // vcom_DC setting
    EPD_4IN2_SendData(0x12);

    EPD_4IN2_SendCommand(0X50); // VCOM AND DATA INTERVAL SETTING
    EPD_4IN2_SendData(0x07); // 97white border 77black border		VBDF 17|D7 VBDW 97 VBDB 57		VBDF F7 VBDW 77 VBDB 37  VBDR B7

    EPD_4IN2_Partial_SetLut();
}

void Local_EPD_4IN2_Init(void)
{
	EPD_4IN2_Reset();
	EPD_4IN2_SendCommand(0x01);			//POWER SETTING 
	EPD_4IN2_SendData (0x03);	          
	EPD_4IN2_SendData (0x00);
	EPD_4IN2_SendData (0x2b);  
	EPD_4IN2_SendData (0x2b);

	EPD_4IN2_SendCommand(0x06);         //boost soft start
	EPD_4IN2_SendData (0x17);		//A
	EPD_4IN2_SendData (0x17);		//B
	EPD_4IN2_SendData (0x17);		//C       

	EPD_4IN2_SendCommand(0x04);  
	Local_EPD_4IN2_ReadBusy();

	EPD_4IN2_SendCommand(0x00);			//panel setting
	EPD_4IN2_SendData(0xbf);		//KW-bf   KWR-2F	BWROTP 0f	BWOTP 1f


	EPD_4IN2_SendCommand(0x30);			
	EPD_4IN2_SendData (0x3c);      	// 3A 100HZ   29 150Hz 39 200HZ	31 171HZ

	EPD_4IN2_SendCommand(0x61);			//resolution setting
	EPD_4IN2_SendData (0x01);        	 
	EPD_4IN2_SendData (0x90);	 //400	
	EPD_4IN2_SendData (0x01);	 //300
	EPD_4IN2_SendData (0x2c);	   


	EPD_4IN2_SendCommand(0x82);			//vcom_DC setting  	
	EPD_4IN2_SendData (0x12);	

	EPD_4IN2_SendCommand(0X50);
	EPD_4IN2_SendData(0x97);

	EPD_4IN2_SetLut();		
}

//UC8176C
void Local_EPD_4IN2_Init_Fast(void)
{
	Local_EPD_4IN2_Init();
}	


/*
void Local_EPD_4IN2_Init_4Gray(void)
{
	EPD_4IN2_Reset();
    // ...
}
*/
/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void Local_EPD_4IN2_Clear(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;

    EPD_4IN2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0xFF);
        }
    }

    EPD_4IN2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0xFF);
            // Update partial buffer if within range
            if (j < PARTIAL_HEIGHT_LIMIT && i < (PARTIAL_WIDTH_LIMIT / 8)) {
                EPD_4IN2_Partial_DATA[i + j * (PARTIAL_WIDTH_LIMIT / 8)] = 0x00; 
            }
        }
    }

	EPD_4IN2_SendCommand(0x12);		 //DISPLAY REFRESH 		
	DEV_Delay_ms(1);	
    EPD_4IN2_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void Local_EPD_4IN2_Display(UBYTE *Image)
{
    UWORD Width, Height;
    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;

	EPD_4IN2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0x00);
        }
    }

    EPD_4IN2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(Image[i + j * Width]);
            // Update partial buffer if within range
            if (j < PARTIAL_HEIGHT_LIMIT && i < (PARTIAL_WIDTH_LIMIT / 8)) {
                EPD_4IN2_Partial_DATA[i + j * (PARTIAL_WIDTH_LIMIT / 8)] = ~Image[i + j * Width];
            }
        }
    }

	EPD_4IN2_SendCommand(0x12);		 //DISPLAY REFRESH 		
	DEV_Delay_ms(10);		
    EPD_4IN2_TurnOnDisplay();
}

void Local_EPD_4IN2_PartialDisplay(UWORD X_start,UWORD Y_start,UWORD X_end,UWORD Y_end, UBYTE *Image)
{
	UWORD Width, Height;

    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;
	
	X_start = (X_start % 8 == 0)? (X_start): (X_start/8*8+8);
	X_end = (X_end % 8 == 0)? (X_end): (X_end/8*8+8);
	

	EPD_4IN2_SendCommand(0x91);		//This command makes the display enter partial mode
	EPD_4IN2_SendCommand(0x90);		//resolution setting
	EPD_4IN2_SendData ((X_start)/256);
	EPD_4IN2_SendData ((X_start)%256);   //x-start    

	EPD_4IN2_SendData ((X_end )/256);		
	EPD_4IN2_SendData ((X_end )%256-1);  //x-end

	EPD_4IN2_SendData (Y_start/256);
	EPD_4IN2_SendData (Y_start%256);   //y-start    

	EPD_4IN2_SendData (Y_end/256);		
	EPD_4IN2_SendData (Y_end%256-1);  //y-end
	EPD_4IN2_SendData (0x28);	

	EPD_4IN2_SendCommand(0x10);	       //writes Old data to SRAM for programming
    for (UWORD j = 0; j < Y_end - Y_start; j++) {
        for (UWORD i = 0; i < (X_end - X_start)/8; i++) {
            // Check bounds for partial buffer
            int pX = X_start/8 + i;
            int pY = Y_start + j;
            if (pY < PARTIAL_HEIGHT_LIMIT && pX < (PARTIAL_WIDTH_LIMIT / 8)) {
                EPD_4IN2_SendData(EPD_4IN2_Partial_DATA[pY * (PARTIAL_WIDTH_LIMIT / 8) + pX]);
            } else {
                EPD_4IN2_SendData(0x00); // Default if out of bounds (assuming black/white?)
            }
        }
    }
	EPD_4IN2_SendCommand(0x13);				 //writes New data to SRAM.
    for (UWORD j = 0; j < Y_end - Y_start; j++) {
        for (UWORD i = 0; i < (X_end - X_start)/8; i++) {
            UBYTE data = ~Image[(Y_start + j)*Width + X_start/8 + i];
            EPD_4IN2_SendData(data);
            
            // Update partial buffer
            int pX = X_start/8 + i;
            int pY = Y_start + j;
            if (pY < PARTIAL_HEIGHT_LIMIT && pX < (PARTIAL_WIDTH_LIMIT / 8)) {
                EPD_4IN2_Partial_DATA[pY * (PARTIAL_WIDTH_LIMIT / 8) + pX] = data;
            }
        }
    }

	EPD_4IN2_SendCommand(0x12);		 //DISPLAY REFRESH 		             
	DEV_Delay_ms(10);     //The delay here is necessary, 200uS at least!!!     
	EPD_4IN2_TurnOnDisplay();
}

/*
void Local_EPD_4IN2_4GrayDisplay(const UBYTE *Image)
{
    // ...
}
*/
/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void Local_EPD_4IN2_Sleep(void)
{
	EPD_4IN2_SendCommand(0x50); // DEEP_SLEEP
    EPD_4IN2_SendData(0XF7);

    EPD_4IN2_SendCommand(0x02); // POWER_OFF
    Local_EPD_4IN2_ReadBusy();

    EPD_4IN2_SendCommand(0x07); // DEEP_SLEEP
    EPD_4IN2_SendData(0XA5);
}
