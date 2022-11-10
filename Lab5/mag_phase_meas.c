/*
Authors:
	Kerem Oktay 56121882
	Idil Bil 21344189
Functionality:
	Measures the rms voltages values, frequency and phase shift of two signals
	One signals is used as reference while other signal is tested
Note:
	Both signals must have the same frequency to get accurate readings
*/

#include <stdio.h>
#include <at89lp51rd2.h>
#include <math.h>

// ~C51~ 
 
#define CLK 22118400L
#define BAUD 115200L
#define ONE_USEC (CLK/1000000L) // Timer reload for one microsecond delay
#define BRG_VAL (0x100-(CLK/(16L*BAUD)))

#define ADC_CE  P2_0
#define BB_MOSI P2_1
#define BB_MISO P2_2
#define BB_SCLK P2_3
#define REF_SIGNAL P0_0
#define REF_CHANNEL 0
#define TEST_SIGNAL P0_1
#define TEST_CHANNEL 1

#define LCD_RS P3_2
// #define LCD_RW PX_X // Not used in this code, connect the pin to GND
#define LCD_E  P3_3
#define LCD_D4 P3_4
#define LCD_D5 P3_5
#define LCD_D6 P3_6
#define LCD_D7 P3_7
#define CHARS_PER_LINE 16

unsigned char SPIWrite(unsigned char out_byte)
{
	// In the 8051 architecture both ACC and B are bit addressable!
	ACC=out_byte;
	
	BB_MOSI=ACC_7; BB_SCLK=1; B_7=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_6; BB_SCLK=1; B_6=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_5; BB_SCLK=1; B_5=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_4; BB_SCLK=1; B_4=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_3; BB_SCLK=1; B_3=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_2; BB_SCLK=1; B_2=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_1; BB_SCLK=1; B_1=BB_MISO; BB_SCLK=0;
	BB_MOSI=ACC_0; BB_SCLK=1; B_0=BB_MISO; BB_SCLK=0;
	
	return B;
}

unsigned char _c51_external_startup(void)
{
	AUXR=0B_0001_0001; // 1152 bytes of internal XDATA, P4.4 is a general purpose I/O

	P0M0=0x00; P0M1=0x00;    
	P1M0=0x00; P1M1=0x00;    
	P2M0=0x00; P2M1=0x00;    
	P3M0=0x00; P3M1=0x00;
	
	// Initialize the pins used for SPI
	ADC_CE=0;  // Disable SPI access to MCP3008
	BB_SCLK=0; // Resting state of SPI clock is '0'
	BB_MISO=1; // Write '1' to MISO before using as input
	
	// Configure the serial port and baud rate
    PCON|=0x80;
	SCON = 0x52;
    BDRCON=0;
    #if (CLK/(16L*BAUD))>0x100
    #error Can not set baudrate
    #endif
    BRL=BRG_VAL;
    BDRCON=BRR|TBCK|RBCK|SPD;
    
	CLKREG=0x00; // TPS=0000B

    return 0;
}

void wait_us (unsigned char x)
{
	unsigned int j;
	
	TR0=0; // Stop timer 0
	TMOD&=0xf0; // Clear the configuration bits for timer 0
	TMOD|=0x01; // Mode 1: 16-bit timer
	
	if(x>5) x-=5; // Subtract the overhead
	else x=1;
	
	j=-ONE_USEC*x;
	TF0=0;
	TH0=j/0x100;
	TL0=j%0x100;
	TR0=1; // Start timer 0
	while(TF0==0); //Wait for overflow
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) wait_us(250);
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

/*Read 10 bits from the MCP3008 ADC converter*/
unsigned int volatile GetADC(unsigned char channel)
{
	unsigned int adc;
	unsigned char spid;

	ADC_CE=0; // Activate the MCP3008 ADC.
	
	SPIWrite(0x01);// Send the start bit.
	spid=SPIWrite((channel*0x10)|0x80);	//Send single/diff* bit, D2, D1, and D0 bits.
	adc=((spid & 0x03)*0x100);// spid has the two most significant bits of the result.
	spid=SPIWrite(0x00);// It doesn't matter what we send now.
	adc+=spid;// spid contains the low part of the result. 
	
	ADC_CE=1; // Deactivate the MCP3008 ADC.
		
	return adc;
}

float Measure_Period(void)
{
    float period;
    float myof;

    TR0 = 0; // Stop timer 0
    TMOD &= 0B_1111_0000; // Set timer 0 as 16-bit timer (step 1)
    TMOD |= 0B_0000_0001; // Set timer 0 as 16-bit timer (step 2)
    TH0 = 0; TL0 = 0; myof = 0; // Reset the timer and overflow counter
    TF0 = 0; // Clear overflow flag
    while(REF_SIGNAL == 1); // Wait for the signal to be zero
    while(REF_SIGNAL == 0); // Wait for the signal to be one
    TR0 = 1; // Start timing
    while (REF_SIGNAL == 1){ // Wait for the signal to be zero
        if (TF0){
            TF0 = 0;
            myof++;
        }
    }
    TR0 = 0; // Stop timer 0
    period = (myof*65536.0 + TH0*256.0 + TL0) * 2.0; //convert timer values to period
    period /= CLK; //convert to 1/clk in units

    return period;
}

float Get_Phase_Difference(float period)
{
    float time;
    float myof;
	float phase = 0.0;

    TR0 = 0; // Stop timer 0
    TMOD &= 0B_1111_0000; // Set timer 0 as 16-bit timer (step 1)
    TMOD |= 0B_0000_0001; // Set timer 0 as 16-bit timer (step 2)
    TH0 = 0; TL0 = 0; myof = 0; // Reset the timer and overflow counter
    TF0 = 0; // Clear overflow flag
    while(REF_SIGNAL == 1); // Wait for the signal to be zero
    while(REF_SIGNAL == 0); // Wait for the signal to be one
    TR0 = 1; // Start timing

	/*
	After zero cross is detected for REF_SIGNAL timer counts the amount of time TEST_SIGNAL takes to have a zero cross
	To get this first the amount of time spend as 1 is measured
	Then amount of time spend as 0 is measured
	Then measuring stops when signals becomes 1 again (zero cross)
	*/
    while (TEST_SIGNAL == 1){ // Wait for the signal to be zero
        if (TF0){
            TF0 = 0;
            myof++;
        }
    }
	while (TEST_SIGNAL == 0){ // Wait for the signal to be one
        if (TF0){
            TF0 = 0;
            myof++;
        }
    }
    TR0 = 0; // Stop timer 0
    time = (myof*65536.0 + TH0*256.0 + TL0);
    time /= CLK; //in seconds

	time *= 1000; //in milliseconds
	period *= 1000; //in milliseconds

	//convert time value to degrees for the phase shift/difference value
	if (time != 0.0){
		phase = time * 360.0 / period;

		// phase must be between -180 and 180, do necessary calculations 
		if(phase > 180.0){
			phase = 360.0 - phase + 2.7; // an offset of 2.7 is added upon tests for higher accuracy (readings were consistently off by 2.5-3.0)
		}
		else{
			phase = 0.0 - phase;
		}
	}

    return phase;
}

void LCD_UPDATE(float frequency, float Vr_peak, float Vt_peak, float phase)
{
	float Vr_rms;
	float Vt_rms;
	char buffer1[CHARS_PER_LINE];
	char buffer2[CHARS_PER_LINE];
	float f = frequency;
	float p = phase;

	// Convert peak voltages to RMS voltage by diving by sqrt2 (multiply by 0.707107)
	Vr_rms = Vr_peak * 0.707107;
	Vt_rms = Vt_peak * 0.707107;

	// convert float numbers to strings
	sprintf(buffer1,"Vr=%3.2f Vt=%3.2f", Vr_rms, Vt_rms);
	sprintf(buffer2,"Fq=%3.1f Ph=%3.2f", frequency, phase);

	// print the strings
	LCDprint(buffer1,1,1);
	LCDprint(buffer2,2,1);


}


#define VREF 4.096

void main (void)
{
    float period;
    float freq;
    float Vref_peak;
    float Vtest_peak;
	float phase_diff = 0.0;

	waitms(500);

	LCD_4BIT();	
	
	while(1)
	{
        period = Measure_Period(); // get the period of reference signal
        freq = 1.0 / period;       // calculate frequency

        while(REF_SIGNAL == 1); // Wait for the signal to be zero
        while(REF_SIGNAL == 0); // Wait for the signal to be one

        waitms(period*1000/4); // wait period/4 sec for the signal to reach peak value
        Vref_peak = (GetADC(REF_CHANNEL)*VREF)/1023.0; // get the peak voltage value

        while(TEST_SIGNAL == 1); // Wait for the signal to be zero 
        while(TEST_SIGNAL == 0); // Wait for the signal to be one

        waitms(period*1000/4); // wait period/4 sec for the signal to reach peak value
        Vtest_peak = (GetADC(TEST_CHANNEL)*VREF)/1023.0; // get the peak voltage value

		phase_diff = Get_Phase_Difference(period); // get the phase difference between reference and test signal

		//print to Putty for testing purposes
		printf("freq = %5.3f  Vref_peak = %5.3f  Vtest_peak = %5.3f  Phase = %5.3f\n", freq, Vref_peak, Vtest_peak, phase_diff);

		//print values on the LCD Module
        LCD_UPDATE(freq,Vref_peak,Vtest_peak,phase_diff);  

		waitms(100); // wait and then repeat 

	}
}
