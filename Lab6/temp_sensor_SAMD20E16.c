/* spi_MCP3008.c: Using SERCOM1 to communicate with the MCP3008 ADC via SPI
 *
 * SERCOM1 is configured as SPI with software Slave Select.
 * Clock rate is set to 200kHz. mode is set to 0, 0
 *
 * PA16  PAD0  MOSI
 * PA17  PAD1  SCK
 * PA18  PAD2  SS       software controlled
 * PA19  PAD3  MISO
 *
 * Authors:
 *	Kerem Oktay
 *	Idil Bil
 *
 * Functionality:
 *	Temperature sensor
 *
 * Note:
 * 	Parts of this code are taken from examples provided for SAMD20E16
 */

#include "samd20.h"
#include <stdlib.h>
#include <stdio.h>

void init_Clock48(void);
void UART3_init(uint32_t baud);

#define LCD_D7 PORT_PA05 // Pin 6 of QFP32
#define LCD_D6 PORT_PA04 // Pin 5 of QFP32
#define LCD_D5 PORT_PA03 // Pin 4 of QFP32
#define LCD_D4 PORT_PA02 // Pin 3 of QFP32
#define LCD_E  PORT_PA01 // Pin 2 of QFP32
#define LCD_RS PORT_PA00 // Pin 1 of QFP32
#define CHARS_PER_LINE 16

void delayMs(int n)
{
    int i;
    // Configure SysTick
    SysTick->LOAD = (F_CPU/1000) - 1; // Reload with number of clocks per millisecond (1ms=1/1kHz). F_CPU is defined in the makefile; it should be 1MHz.
    SysTick->VAL = 0;         // clear current value register
    SysTick->CTRL = 0x5;      // Enable the timer

    for(i = 0; i < n; i++)
    {
        while((SysTick->CTRL & 0x10000) == 0); // wait until the COUNTFLAG is set.  Reading the set flag clears it.
    }
    SysTick->CTRL = 0; // Stop the SysTick timer (Enable = 0)
}

void LCD_delay (void)
{
	volatile int j;
	for(j=0; j<100; j++);
}

void LCD_pulse (void)
{
	REG_PORT_OUTSET0=LCD_E; // LCD_E=1
	LCD_delay();
	REG_PORT_OUTCLR0=LCD_E; // LCD_E=0
}

void LCD_byte (unsigned char x)
{
	if (x & 0x80) {REG_PORT_OUTSET0=LCD_D7;} else {REG_PORT_OUTCLR0=LCD_D7;}
	if (x & 0x40) {REG_PORT_OUTSET0=LCD_D6;} else {REG_PORT_OUTCLR0=LCD_D6;}
	if (x & 0x20) {REG_PORT_OUTSET0=LCD_D5;} else {REG_PORT_OUTCLR0=LCD_D5;}
	if (x & 0x10) {REG_PORT_OUTSET0=LCD_D4;} else {REG_PORT_OUTCLR0=LCD_D4;}
	LCD_pulse();
	LCD_delay();
	if (x & 0x08) {REG_PORT_OUTSET0=LCD_D7;} else {REG_PORT_OUTCLR0=LCD_D7;}
	if (x & 0x04) {REG_PORT_OUTSET0=LCD_D6;} else {REG_PORT_OUTCLR0=LCD_D6;}
	if (x & 0x02) {REG_PORT_OUTSET0=LCD_D5;} else {REG_PORT_OUTCLR0=LCD_D5;}
	if (x & 0x01) {REG_PORT_OUTSET0=LCD_D4;} else {REG_PORT_OUTCLR0=LCD_D4;}
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	REG_PORT_OUTSET0=LCD_RS; // LCD_RS=1;
	LCD_byte(x);
	delayMs(2);
}

void WriteCommand (unsigned char x)
{
	REG_PORT_OUTCLR0=LCD_RS; // LCD_RS=0;
	LCD_byte(x);
	delayMs(5);
}

void LCD_4BIT (void)
{
	// Configure LCD contorl signals as outputs
    REG_PORT_DIRSET0 = LCD_D7;
    REG_PORT_DIRSET0 = LCD_D6;
    REG_PORT_DIRSET0 = LCD_D5;
    REG_PORT_DIRSET0 = LCD_D4;
    REG_PORT_DIRSET0 = LCD_RS;
    REG_PORT_DIRSET0 = LCD_E;

	REG_PORT_OUTCLR0=LCD_E; // Resting state of LCD's enable is zero
	delayMs(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	delayMs(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, unsigned char line, int clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	delayMs(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

void InitSPI (uint32_t baud)
{
	uint64_t br = (uint64_t)65536 * (F_CPU - 16 * baud) / F_CPU;

	PM->APBCMASK.reg |= PM_APBCMASK_SERCOM1; // SERCOM1 bus clock
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM1_GCLK_ID_CORE) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(0); // SERCOM1 core clock

    /* Enable & configure alternate function C for pins PA16, PA17 & PA19 */
    PORT->Group[0].PINCFG[16].bit.PMUXEN = 1;
    PORT->Group[0].PINCFG[17].bit.PMUXEN = 1;
    PORT->Group[0].PINCFG[19].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[8].reg = 0x22; // PA16 = MOSI, PA17 = SCK
    PORT->Group[0].PMUX[9].reg = 0x20; // PA19 = MISO
 
    REG_PORT_OUTSET0 = PORT_PA18;      /* SS idle high */
    REG_PORT_DIRSET0 = PORT_PA18;      /* PA18 software controlled SS */

    REG_SERCOM1_SPI_CTRLA = 1;              /* reset SERCOM1 */
    while (REG_SERCOM1_SPI_CTRLA & 1) {}    /* wait for reset to complete */
    REG_SERCOM1_SPI_CTRLA = 0x0030000C;     /* MISO-3, MOSI-0, SCK-1, SS-2, SPI master */
    REG_SERCOM1_SPI_CTRLB = 0x00020000;     /* RX emabled, 8-bit */
	SERCOM1->USART.BAUD.reg = (uint16_t)br;
    REG_SERCOM1_SPI_CTRLA |= 2;             /* enable SERCOM1 */
}

uint8_t SPIWritex (unsigned char c)
{
	REG_SERCOM1_SPI_DATA = c;                   /* send data byte */
	while(!(REG_SERCOM1_SPI_INTFLAG & 2)) {}    /* wait until Tx complete */
	REG_SERCOM1_SPI_INTFLAG = 2;                /* clear TX Complete flag */
	return REG_SERCOM1_SPI_DATA;
}

uint8_t SPIWrite(uint8_t data)
{
    while(SERCOM1->SPI.INTFLAG.bit.DRE == 0) {};
    SERCOM1->SPI.DATA.reg = data;
    while(SERCOM1->SPI.INTFLAG.bit.RXC == 0) {};
    return SERCOM1->SPI.DATA.reg;
}

// Read 10 bits from ithe MCP3008 ADC converter using the recomended format in the datasheet.
unsigned int GetADC(char channel)
{
	unsigned int adc;
	unsigned char mybyte;

    REG_PORT_OUTCLR0 = PORT_PA18; //Select the MCP3008 converter.
	
	mybyte=SPIWrite(0x01);//Send the start bit.

	adc=mybyte;
	mybyte=SPIWrite((channel*0x10)|0x80);	//Send single/diff* bit,  D2, D1, and D0 bits.
	adc=((mybyte & 0x03)*0x100); // 'mybyte' contains now the high part of the result.
	mybyte=SPIWrite(0x55); // Dont' care what you send now.  0x55 looks good on the oscilloscope though!
	adc+=mybyte; // 'mybyte' contains now the low part of the result. 
	
    REG_PORT_OUTSET0 = PORT_PA18; //Deselect the MCP3008 converter.
		
	return adc;
}

#define VREF 3.3
 
int main(void) 
{
	float temp_Volts;
	float temp_Cdegrees;
	unsigned char buff[CHARS_PER_LINE];

	init_Clock48();
	UART3_init(115200);
	InitSPI(200000);
	LCD_4BIT();

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	// set ports to be used as output
	REG_PORT_DIRSET0 = PORT_PA24;
	REG_PORT_DIRSET0 = PORT_PA25;

	while(1)
	{
		// read ADC value and convert to temperature calue in celcius degrees
		temp_Volts = (GetADC(0)*VREF) / 1023.0;
		temp_Cdegrees = (100 * temp_Volts) - 273;

		// print the temperature on serial comm
		printf("%5.3f\n", temp_Cdegrees);
		fflush(stdout);

		// convert float to string and print it on LCD screen
		sprintf(buff,"%f",temp_Cdegrees);
		LCDprint(buff,2,1);

		//depending on temperature value print the state of room and turn on leds 
        //NOTE: THESE BOUNDARY VALUES ARE CHOSEN FOR DEMONSTRATION, NORMAL CONDITIONS CAN BE HIGHER OR LOWER FOR HOT AND COLD STATES
        if(temp_Cdegrees<22.0){
            LCDprint("Room State: COLD",1,1); 
			REG_PORT_OUTCLR0 = PORT_PA24; // dangerous temperature: turn on red led
			REG_PORT_OUTSET0 = PORT_PA25;
        }
        else if(temp_Cdegrees>30.0){
            LCDprint("Room State: HOT",1,1);
 			REG_PORT_OUTCLR0 = PORT_PA24; // dangerous temperature: turn on red led
			REG_PORT_OUTSET0 = PORT_PA25;
        }
        else{
            LCDprint("Room State: IDLE",1,1);
			REG_PORT_OUTSET0 = PORT_PA24; // normal temperature: turn on green led
			REG_PORT_OUTCLR0 = PORT_PA25;
        }

		delayMs(100);
	}
}
