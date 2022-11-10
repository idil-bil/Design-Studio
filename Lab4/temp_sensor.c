#include <stdio.h>
#include <at89lp51rd2.h>
#include <string.h>

/*
Authors:
    Kerem Oktay 56121882
    Idil Bil 21344189

Functionality:
    By using a temperature sensor and ADC, get temperature values from 8051
    via Serial Comm.

Note:
    This code is build on the adc_spi.c and LCD_4bit code provided on the course page.
*/

#define CLK 22118400L
#define BAUD 115200L
#define ONE_USEC (CLK/1000000L) //Timer reload for one microsecond delay
#define BRG_VAL (0x100-(CLK/(16L*BAUD)))

#define ADC_CE P2_0
#define BB_MOSI P2_1
#define BB_MISO P2_2
#define BB_SCLK P2_3

#define LCD_RS P3_2
// #define LCD_RW PX_X // Not used in this code, connect the pin to GND
#define LCD_E  P3_3
#define LCD_D4 P3_4
#define LCD_D5 P3_5
#define LCD_D6 P3_6
#define LCD_D7 P3_7
#define CHARS_PER_LINE 16

char mystr[CHARS_PER_LINE+1];

unsigned char SPIWrite(unsigned char out_byte)
{
    ACC = out_byte;

    BB_MOSI = ACC_7; BB_SCLK = 1; B_7 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_6; BB_SCLK = 1; B_6 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_5; BB_SCLK = 1; B_5 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_4; BB_SCLK = 1; B_4 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_3; BB_SCLK = 1; B_3 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_2; BB_SCLK = 1; B_2 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_1; BB_SCLK = 1; B_1 = BB_MISO; BB_SCLK = 0;
    BB_MOSI = ACC_0; BB_SCLK = 1; B_0 = BB_MISO; BB_SCLK = 0;

    return B;
}

unsigned char _c51_external_startup(void)
{
    AUXR = 0B_0001_0001; // 1152 bytes of internal XDATA, P4.4 is a general purpose I/O

    P0M0 = 0X00; P0M1 = 0x00;
    P1M0 = 0X00; P1M1 = 0x00;
    P2M0 = 0X00; P2M1 = 0x00;
    P3M0 = 0X00; P3M1 = 0x00;

    // Initialize the pins used for SPI
    ADC_CE = 0;  // Disable SPI access to MCP3008
    BB_SCLK = 0; // Resting state of SPI clock is '0'
    BB_MISO = 1; // Write '1' to MISO before using as input

    // Configure the serial port and baud rate
    PCON |= 0x80;
    SCON = 0x52,
    BDRCON = 0;
    #if (CLK/(16L*BAUD)) > 0x100
    #error Cannot set baudrate
    #endif
    BRL = BRG_VAL;
    BDRCON = BRR | TBCK | RBCK | SPD;

    CLKREG = 0x00; // TPS = 0000B

    return 0;
}

void wait_us (unsigned char x)
{
    unsigned int j;

    TR0 = 0;      // Stop timer 0
    TMOD &= 0xf0; // Clear the configuration bits for timer 0
    TMOD |= 0x01; // Mode 1: 16-bit timer

    if(x > 5){
        x -= 5; // Subtract the overhead
    }
    else{
        x = 1;
    }

    j = -ONE_USEC*x;
    TF0 = 0;
    TH0 = j/0x100;
    TL0 = j%0x100;
    TR0 = 1; // Start timer 0
    while(TF0 == 0); // Wait for overflow
}

void waitms (unsigned int ms)
{
    unsigned int j;
    unsigned char k;

    for(j=0; j<ms; j++){
        for(k=0; k<4; k++){
            wait_us(250);
        }
    }
}

void LCD_pulse (void)
{
	LCD_E=1;
	wait_us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	// The accumulator in the 8051 is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	wait_us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	//LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

/* Read 10 bits from the MCP3008 ADC converter */
unsigned int volatile GetADC(unsigned char channel)
{
    unsigned int adc;
    unsigned char spid;

    ADC_CE = 0; // Activate the MCP3008 ADC

    SPIWrite(0x01); // Send the start bit
    spid = SPIWrite((channel*0x10) | 0x80); //Send single/diff* bit, D2, D1, and D0 bits
    adc = ((spid & 0x03) * 0x100); // spid has the two most significant bits of the result
    spid = SPIWrite(0x00); // It doesn't matter what we send now
    adc += spid; // spid contains the low part of the result

    ADC_CE = 1; //Deactivate the MCP3008 ADC

    return adc;
}

#define VREF 4.096

void main (void)
{
    float y;
    unsigned char i = 0; //The pin we are reading from ADC
    unsigned char c[CHARS_PER_LINE];
    unsigned char temp[CHARS_PER_LINE] = "Temp=";

    waitms(500);  // Gives time to putty to start before sending text
    printf("\n\nAT89LP51Rx2 SPI ADC Temperature Program\n");

    LCD_4BIT();


    while(1)
    {
        y = (GetADC(i)*VREF) / 1023.0; // Convert the 10-bit integer from the ADC to Voltage
        y = (100 * y) - 273; // Convert the voltage value to temperature value
        printf("%5.3f\n", y); //print the temperature value

        // rest of this code uses the LCD display to display the temperature value and state
        sprintf(c,"%f",y); //convert the temperature value to string
        LCDprint(c,2,1); //print temperature value

        //depending on temperature value printf the state of room 
        //NOTE: THESE BOUNDARY VALUES ARE CHOSEN FOR DEMONSTRATION, NORMAL CONDITIONS CAN BE HIGHER OR LOWER FOR HOT AND COLD STATES
        if(y<22.0){
            LCDprint("Room State: COLD",1,1);
        }
        else if(y>30.0){
            LCDprint("Room State: HOT",1,1);
        }
        else{
            LCDprint("Room State: IDLE",1,1);
        }

        waitms(100);
    }
}
