/*****************************************************************/
/*          Frequency Counter OLED SSD1306       128x32          */
/*  ************************************************************ */
/*  MUC:              ATMEL AVR ATmega88, 16 MHz                 */
/*                                                               */
/*  Compiler:         GCC (GNU AVR C-Compiler)                   */
/*  Author:           Peter Baier (DK7IH)                        */
/*  Last change:      MAR 2024                                   */
/*****************************************************************/
//PORTS
//TWI OLED
//PC4=SDA, PC5=SCL: I²C-Bus lines: 

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <util/twi.h>

#define INTERFREQUENCY 9000000

#define CPUCLK 16
#define CHRW 15

#define OLEDADDR 0x78
#define OLEDCMD 0x00   //Command follows
#define OLEDDATA 0x40  //Data follows

#define FONTW 6
#define FONTH 8

#define S_SETLOWCOLUMN           0x00
#define S_SETHIGHCOLUMN          0x10
#define S_PAGEADDR               0xB0
#define S_SEGREMAP               0xA0

#define S_LCDWIDTH               128
#define S_LCDHEIGHT              64
//////////////////////////////////////
//   O L E D Functions
//////////////////////////////////////

///////////////////////////
//     DECLARATIONS
///////////////////////////
//
//I²C
void twi_init(void);
void twi_start(void);
void twi_stop(void);
void twi_write(uint8_t u8data);

//OLED
void oled_command(int value);
void oled_init(void);
void oled_byte(unsigned char);
void oled_gotoxy(unsigned int, unsigned int);
void oled_cls(void);
void oled_write_section(int, int, int, int);
void oled_data(unsigned int *, unsigned int);

//Digits
void n0(int);
void n1(int);
void n2(int);
void n3(int);
void n4(int);
void n5(int);
void n6(int);
void n7(int);
void n8(int);
void n9(int);

//Bars for digitx
void vbar(int, int, int);
void hbar(int, int);
void vbarblack(int, int, int);
void hbarblack(int, int);
void clear_digit(int);

//Decimal
void dec(int);

//MISC
int main(void);
void show_freq(unsigned long);

//Buffer for old values
int old[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
long runseconds100th = 0;
long pulses0 = 0;

///////////////////////////
//
//         TWI
//
///////////////////////////

//Delaytime fck = 8.000 MHz 
void wait_ms(int ms)
{
    int t1, t2;

    for(t1 = 0; t1 < ms; t1++)
    {
        for(t2 = 0; t2 < 137 * CPUCLK; t2++)
        {
            asm volatile ("nop" ::);
        }   
     }    
}

void twi_init(void)
{
    //set SCL to 400kHz
    TWSR = 0x00;
    TWBR = 0x0C;
	
    //enable TWI
    TWCR = (1<<TWEN);
}

//Send start signal
void twi_start(void)
{
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while ((TWCR & (1<<TWINT)) == 0);
}

//send stop signal
void twi_stop(void)
{
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}

void twi_write(uint8_t u8data)
{
    TWDR = u8data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while ((TWCR & (1<<TWINT)) == 0);
}

////////////////////////////////
//
// OLED commands
//
///////////////////////////////
//Send comand to OLED
void oled_command(int value)
{
   twi_start();
   twi_write(OLEDADDR); //Device address
   twi_write(OLEDCMD);  //Command follows
   twi_write(value);    //Send value
   twi_stop();
} 

//Send a 'number' bytes of data to display - from RAM
void oled_data(unsigned int *data, unsigned int number)
{
   int t1;
   twi_start();
   twi_write(OLEDADDR); //Device address
   twi_write(OLEDDATA); //Data follows
   
   for(t1 = 0; t1 < number; t1++)
   {
      twi_write(data[t1]); //send the byte(s)
   }   
   twi_stop ();   
}

//Set "cursor" to current position to screen
void oled_gotoxy(unsigned int x, unsigned int y)
{
   int x2 = x + 2;
   twi_start();
   twi_write(OLEDADDR); //Select display  I2C address
   twi_write(OLEDCMD);  //Be ready for command
   twi_write(S_PAGEADDR + y); //Select display row
   twi_write(S_SETLOWCOLUMN + (x2 & 0x0F)); //Col addr lo byte
   twi_write(S_SETHIGHCOLUMN + ((x2 >> 4) & 0x0F)); //Col addr hi byte
   twi_stop();
}


//Initialize OLED
void oled_init(void)
{
    oled_command(0xae);
	
    oled_command(0xa8);//Multiplex ratio
    oled_command(0x3F);
	
    oled_command(0xd3);
    oled_command(0x00);
    oled_command(0x40);
    oled_command(0xa0);
    oled_command(0xa1);
	
	oled_command(0x20); //Adressing mode
    oled_command(0x02); //VERT
	
    oled_command(0xc0);
    oled_command(0xc8);
    oled_command(0xda);
    oled_command(0x12);
    oled_command(0x81); //Contrast
    oled_command(0xff);
    oled_command(0xa4); //Display ON with RAM content
    oled_command(0xa6); //Normal display (Invert display = A7)
    oled_command(0xd5);
    oled_command(0x80);
    oled_command(0x8d);
    oled_command(0x14);
	//oled_command(0x40); //Set display start line
    oled_command(0xAF); //Display ON
   
} 

//Write 1 byte pattern to screen using vertical orientation 
void oled_byte(unsigned char value)
{
   twi_start();
   twi_write(OLEDADDR); //Device address
   twi_write(OLEDDATA); //Data follows
   twi_write(value);
   twi_stop ();   
}

void oled_cls(void)
{
    unsigned int row, col;

    //Just fill the memory with zeros
    for(row = 0; row < S_LCDHEIGHT / 8; row++)
    {
        oled_gotoxy(0, row); //Set OLED address
        twi_start();
        twi_write(OLEDADDR); //Select OLED
        twi_write(OLEDDATA); //Data follows
        for(col = 0; col < S_LCDWIDTH; col++)
        {
            twi_write(0); //normal
        }
        twi_stop();
    }
    oled_gotoxy(0, 0); //Return to 0, 0
}

//Write number of bitmaps to one row of screen
void oled_write_section(int x1, int x2, int row, int number)
{
    int t1;
    oled_gotoxy(x1, row);
    	
    twi_start();
    twi_write(OLEDADDR); //Device address
    twi_write(OLEDDATA); //Data follows
   
    for(t1 = x1; t1 < x2; t1++)
    {
       twi_write(number); //send the byte(s)
    }    
    twi_stop ();   
}

void clear_digit(int pos)
{
	int x = pos * CHRW + pos * 2;
	
	vbarblack(x, 0, 0);
    vbarblack(x, 1, 0);
    vbarblack(x, 0, 1);
    vbarblack(x, 1, 1);
    hbarblack(x, 0);
    hbarblack(x, 1);
    hbarblack(x, 2);
    
    x = pos * CHRW + pos * 2 - CHRW / 2;
	
	vbarblack(x, 0, 1);
    vbarblack(x, 1, 1);
}	

void vbar(int x, int line, int pos)
{
	int t1;
	
	for(t1 = 0; t1 < 4; t1++)
	{
		if(!pos)
		{
			oled_gotoxy(x + 0, line * 4 + t1);
	        oled_byte(255);
	        oled_gotoxy(x + 1, line * 4 + t1);
	        oled_byte(255);
	    }
	    else    
	    {
			oled_gotoxy(x + 0 + CHRW - 2, line * 4 + t1);
	        oled_byte(255);
	        oled_gotoxy(x + 1 + CHRW - 2, line * 4 + t1);
	        oled_byte(255);
	    }
	}    
}	

void vbarblack(int x, int line, int pos)
{
	int t1;
	
	for(t1 = 0; t1 < 4; t1++)
	{
		if(!pos)
		{
			oled_gotoxy(x + 0, line * 4 + t1);
	        oled_byte(0);
	        oled_gotoxy(x + 1, line * 4 + t1);
	        oled_byte(0);
	    }
	    else    
	    {
			oled_gotoxy(x + 0 + CHRW - 2, line * 4 + t1);
	        oled_byte(0);
	        oled_gotoxy(x + 1 + CHRW - 2, line * 4 + t1);
	        oled_byte(0);
	    }
	}    
}	

void hbar(int x, int pos)
{
	int t1, y0 = 0, y1 = 0;
	
	switch(pos)
	{
		case 0: y0 = 0;
		        y1 = 2;
		        break;
		case 1: y0 = 3;
		        y1 = 128;
		        break;
		case 2: y0 = 7;
		        y1 = 128;
		        break;
	}	        
		        
	
	for(t1 = 0; t1 < CHRW - 4; t1++)
	{
		oled_gotoxy(x + t1 + 2, y0);
	    oled_byte(y1);
	}
}	


void hbarblack(int x, int pos)
{
	int t1, y0 = 0;
	
	switch(pos)
	{
		case 0: y0 = 0;
		        break;
		case 1: y0 = 3;
		        break;
		case 2: y0 = 7;
		        break;
	}	        
	
	for(t1 = 0; t1 < CHRW - 4; t1++)
	{
		oled_gotoxy(x + t1 + 2, y0);
	    oled_byte(0);
	}
}	

void show_freq(unsigned long f0)
{
	long f1, p10 = 1E09;
	int i;
	int pos = 0;
	int ok = 0;
	
	f1 = f0;
	
	while(p10)
	{
		i = f1 / p10;
		if(i)
		{
			ok = 1;
		}
			
		f1 -= p10 * i;
		p10 /= 10;
		
		if(ok)
		{
			if((i != old[pos]) && (pos != 5))
			{
                clear_digit(pos);
                old[pos] = i;    
            		
		        switch(i)
		        {
			        case 0: n0(pos);
			                break; 
			        case 1: n1(pos);
			                break; 
			        case 2: n2(pos);
			                break; 
			        case 3: n3(pos);
			                break; 
			        case 4: n4(pos);
			                break; 
			        case 5: n5(pos);
			                break; 
			        case 6: n6(pos);
			                break; 
			        case 7: n7(pos);
			                break; 
			        case 8: n8(pos);
			                break; 
			        case 9: n9(pos);
			                break; 
			    }            
			}
			 
			pos++;
			 
			if(pos == 5)
			{
				dec(pos++);                       
			}
			
		}
	}
}
	
void n0(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 0, 0);
    vbar(x, 1, 0);
    vbar(x, 0, 1);
    vbar(x, 1, 1);
    hbar(x, 0);
    hbar(x, 2);
}    

void n1(int x0)    
{
	int x = x0 * CHRW + x0 * 2 - CHRW / 2;
	
	vbar(x, 0, 1);
    vbar(x, 1, 1);
}    

void n2(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 1, 0);
    vbar(x, 0, 1);
    hbar(x, 0);
    hbar(x, 1);
    hbar(x, 2);
}    

void n3(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
    vbar(x, 0, 1);
    vbar(x, 1, 1);
    hbar(x, 0);
    hbar(x, 1);
    hbar(x, 2);
}    

void n4(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 0, 0);
    vbar(x, 0, 1);
    vbar(x, 1, 1);
    hbar(x, 1);
}    

void n5(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 0, 0);
    vbar(x, 1, 1);
    hbar(x, 0);
    hbar(x, 1);
    hbar(x, 2);
}    

void n6(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	n5(x0);
	vbar(x, 1, 0);
}    

void n7(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 0, 1);
    vbar(x, 1, 1);
    hbar(x, 0);
}    

void n8(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	n0(x0);
	hbar(x, 1);
}    

void n9(int x0)    
{
	int x = x0 * CHRW + x0 * 2;
	
	vbar(x, 0, 0);
    //vbar(x, 1, 0);
    vbar(x, 0, 1);
    vbar(x, 1, 1);
    hbar(x, 0);
    hbar(x, 1);
    hbar(x, 2);
}    

void dec(int x0)
{
	int x = x0 * CHRW + x0 * 2 + CHRW / 2;
		
	oled_gotoxy(x, 7);
	oled_byte(168);
	oled_gotoxy(x + 1, 7);
	oled_byte(168);
	oled_gotoxy(x + 2, 7);
	oled_byte(168);
}	

ISR(TIMER1_COMPA_vect)
{
	runseconds100th++; 
}

ISR(PCINT0_vect)
{
	if(PINB & (1 << 2)) //Rising edge
	{
		pulses0++;
	}	
}    

int main(void)
{
	unsigned long timx, x;
	unsigned long fdisp = 0;
	unsigned  long pulses1 = 0;
		
	DDRB |= (1 << 0); // Control pulse out PB0
	
	//Timer 1 as counter for 100th seconds
    timx = 3124; //2550; //f=100Hz 2499
    TCCR1A = 0;  // normal mode, no PWM
    TCCR1B = (1 << CS10) | (1 << CS11) | (1<<WGM12);   // Prescaler = 1/64 based on system clock 16 MHz, CTC mode
                                                       //-> f.cntr = 250.000 kHz
                                                       // 250.000 incs/sec
                                                       // and enable reset of counter register
	OCR1AH = (timx >> 8);                             //Load compare values to registers
    OCR1AL = (timx & 0x00FF);
	TIMSK1 |= (1<<OCIE1A);
	
	//Interrupt for input signal  
	PCMSK0 |= (1<<PCINT2);  //Enable pins PB2 as interrupt source
	PCICR |= (1<<PCIE0);   // Enable pin change interupts 

	//Init procedures
	//TWI
	twi_init();
	//OLED
	oled_init();
	oled_cls();	
	    
	sei();    
    for(;;)
    {	
		PORTB &= ~(1 << 0);
	    PORTB |= (1 << 0);    
		fdisp = ((pulses1 << 7) + INTERFREQUENCY) / 100;
				
	    if(runseconds100th >= 100)
		{
		   pulses1 = pulses0;
           pulses0 = 0;
		   runseconds100th = 0;
		   if(fdisp > 100000) //Wait for correct measurement
		   {
		       show_freq(fdisp);
		   }    
		}		
    }
	return 0;
}
