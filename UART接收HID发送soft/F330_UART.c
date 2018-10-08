//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include <c8051f330.h>                 // SFR declarations
#include <stdio.h>
#include "mytype.h"
#include "config.h"
#include "F330_UART.h"

static uint16 idletimeNET=300;//无按键按下超时,5min
static uint16 idletime=0;//无按键按下超时
static uint8  resetflag=0;//重新连接标志
static uint8  poweroffFlag=0;//关机标志
static uint8  bat_val=0;//电池电压
static uint8  adc_flag=0;//电池电压测试标志，10min
static uint16 adc_timeout=0;//电压测试时间
uint8 code batt[16]= {0x27,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x05,0x04,0x17,0x10,0x0C,0x11}; //0~9,bat,min



//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------
void ADC0_Init(void);
void SYSCLK_Init (void);
void UART0_Init (void);
void PORT_Init (void);
void Timer0_Init (void);
void KEY_VAL(uint8 tmp);
void delay_ms(uint16 ms);
//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
static uint8 tx_buf[12]= {0x0C,0x00,0xA1,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; //HID发送缓冲区
static uint8 tx_count;//发送字节数，实际发送12个字节
uint8 fn_flag;//Fn键按下状态
#define FnDown	0x00
#define FnUp	0x01

//key
#define LCtrl	0x14
#define LShift 	0x12
#define LAlt	0x10
#define LGui	0x07
//#define RCtrl	0x??
#define RShift 	0x59
#define RAlt	0x13
//#define RGui	0x??
#define Fn		0x02
//fn_key,用于测试。实际使用时查表
#define Home	0x5E
#define End		0x2F
#define PgUp	0x28
#define PgDown	0x60
#define Esc		0x0E
#define F1		0x16
#define	F3		0x26

//uart缓冲区使用变量
#define UART_BUFFERSIZE 32
uint8 UARTgetBuf[UART_BUFFERSIZE];
uint8 u8Rxindex = 0;
uint8 u8getQunindex = 0;
uint8 u8sendQunindex = 0;
uint8 TxLock =1;
static uint8 Byte;

//KEY
uint8 code key_code[128]=
{

	0x00,0x00,0x65,0x00,0x00,0x00,0x00,0xE3,0x29,0x00,0x00,0x00,0x00,0x2B,0x35,0x00,

	0xE2,0x00,0xE1,0xE6,0xE0,0x14,0x1E,0x00,0x00,0x00,0x1D,0x16,0x04,0x1A,0x1F,0x00,

	0x00,0x06,0x1B,0x07,0x08,0x21,0x20,0x00,0x52,0x00,0x19,0x09,0x17,0x15,0x22,0x4F,

	0x00,0x11,0x05,0x0B,0x0A,0x1C,0x23,0x00,0x00,0x00,0x10,0x0D,0x18,0x24,0x25,0x00,

	0x00,0x36,0x0E,0x0C,0x12,0x27,0x26,0x00,0x00,0x37,0x38,0x0F,0x33,0x13,0x2D,0x00,

	0x00,0x00,0x34,0x00,0x2F,0x2E,0x00,0x00,0x39,0xE5,0x28,0x30,0x2C,0x31,0x50,0x00,

	0x51,0x00,0x00,0x00,0x00,0x00,0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

	0x00,0x2A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

//Fn_KEY
uint8 code fn_key_code[128]=
{

	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x29,0x00,

	0x00,0x00,0x00,0x00,0x00,0x00,0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3B,0x00,

	0x00,0x00,0x00,0x00,0x00,0x3D,0x3C,0x00,0x4B,0x00,0x00,0x00,0x00,0x00,0x3E,0x4D,

	0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x41,0x00,

	0x00,0x00,0x00,0x00,0x00,0x43,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x00,

	0x00,0x00,0x00,0x00,0x00,0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4A,0x00,

	0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

//-----------------------------------------------------------------------------
// MAIN Routine
//-----------------------------------------------------------------------------

void main (void)
{
	uint8 i,tmp;
	PCA0MD &= ~0x40;                    // WDTE = 0 (clear watchdog timer
	// enable)
	PORT_Init();                        // Initialize Port I/O
	SYSCLK_Init ();                     // Initialize Oscillator
	UART0_Init();
	Timer0_Init ();                  //1ms定时器

	vcc_ctrl	=	vcc_on;// 打开系统总电源,1开0关
	bl_conn_ctrl=	0;// 根据外部按钮的状态控制蓝牙模块
	zb_led2		=	0;// 1为亮，0为灭，LED2未连接
	//bl_state	为蓝牙模块状态输入
	//key_int0  按键中断及开机
	//zb_adc0   电池电压判断
	idletime=0;
	resetflag=0;//重新连接标志
	poweroffFlag=0;
	adc_flag=0;


	//'0c 00 a1 01 00 00 27 00 00 00 00 00
	tx_buf[0]=0x0C;//固定开头
	tx_buf[1]=0x00;
	tx_buf[2]=0xA1;
	tx_buf[3]=0x01;

	tx_buf[4]=0x00;//功能键shift、alt、ctrl、win
	tx_buf[5]=0x00;//固定00

	tx_buf[6]=0x00;//后面6个字节为按键码
	tx_buf[7]=0x00;
	tx_buf[8]=0x00;
	tx_buf[9]=0x00;
	tx_buf[10]=0x00;
	tx_buf[11]=0x00;

	fn_flag=FnUp;

	ADC0_Init();
	bl_vcc_ctrl	=	bl_on;/// /1为关闭蓝牙电源，0为开启
	EA = 1;


	while(1)
	{
		delay_ms(10);

		if((TxLock == 1 )&& (u8Rxindex != 0))
		{
			TxLock = 0;                 // Set the flag to zero

			// If a new word is being output
			if ( u8Rxindex == u8getQunindex )
			{
				u8sendQunindex = 0;
			}

			tmp = UARTgetBuf[u8sendQunindex];

			KEY_VAL(tmp);  //break;

			u8sendQunindex++;            // Update counter

			u8Rxindex--;             // Decrease array size

			TI0 = 1;                      // Set transmit flag to 1

		}
		else
		{
			u8Rxindex = 0;            // Set the array size to 0
			TxLock = 1;                    // Indicate transmission complete
		}

		if(resetflag)
		{
			idletime=0;//重新连接按下则空闲等待置0，从新计数
			resetflag=0;

			bl_conn_ctrl=1;//蓝牙清楚记忆，重新连接
			delay_ms(2000);
			bl_conn_ctrl=0;

		}

		if(poweroffFlag)
		{
			poweroffFlag=0;
			vcc_ctrl=vcc_off;//总电源关闭
		}

		if(adc_flag)
		{
			adc_flag=0;
			ADC0_Init();
		}
	}
}

//-----------------------------------------------------------------------------
// Initialization Subroutines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PORT_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// Configure the Crossbar and GPIO ports.
//
// P0.4   digital   push-pull    UART TX
// P0.5   digital   open-drain   UART RX
//
//-----------------------------------------------------------------------------
void PORT_Init (void)
{
	P1MDIN    = 0x7F;
	P0MDOUT   = 0x14;
	P1MDOUT   = 0x48;
	XBR0    = 0x01;                     // Enable UART on P0.4(TX) and P0.5(RX)
	XBR1    = 0x40;                     // Enable crossbar and weak pull-ups
}
//ADC Init P1.7-GND
void ADC0_Init()
{
	uint8 k;
	uint16 adc_val;

	EA=0;
	delay_ms(50);


	AMX0P     = 0x0F;//P1.7
	AMX0N     = 0x11;//GND
	ADC0CF   |= 0x04;//LEFT ADJUST
	AD0EN	  = 1;
	adc_val	  = 0;
	for(k=0; k<10; k++)
	{
		AD0INT=0;
		AD0BUSY=1;//转换ADC
		while(!AD0INT);//等待转换完成
		if(k>4)adc_val += ADC0H;
	}
	AD0EN = 0;

	k=(uint8)(adc_val / 5)+3;//补偿3-36mv，4-48mv

	if(k>115)bat_val=100;//2776
	else if(k>110)bat_val=75;//2655
	else if(k>105)bat_val=50;//2535
	else if(k>100)bat_val=40;//2414
	else if(k>95)bat_val=30;//2293
	else if(k>90)bat_val=20;//2173
	else if(k>85)bat_val=10;//2052
	else if(k>80)bat_val=5;//1931
	else //if(k>75)//1811
	{
		bat_val=0;
		poweroffFlag=1;
	}
	EA=1;
}
//-----------------------------------------------------------------------------
// SYSCLK_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// This routine initializes the system clock to use the internal oscillator
// at its maximum frequency.
// Also enables the Missing Clock Detector.
//-----------------------------------------------------------------------------

void SYSCLK_Init (void)
{

	OSCICN    = 0x81;					//6.125MHz

	RSTSRC  = 0x04;                     // Enable missing clock detector

}
//1ms定时器
void Timer0_Init (void)
{
	TH0 =TIMER0_RELOAD_HIGH;
	CKCON |=  0x02;//48分频

	TL0 = TH0;                          // init Timer1
	TMOD &= ~0x0F;                      // TMOD: timer 1 in 8-bit autoreload
	TMOD |=  0x02;
	TR0 = 1;
	ET0=1;
}
//-----------------------------------------------------------------------------
// UART0_Init
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
// Configure the UART0 using Timer1, for <BAUDRATE> and 8-N-1.
//-----------------------------------------------------------------------------
void UART0_Init (void)
{
	SCON0 = 0x10;                       // SCON0: 8-bit variable bit rate
	//        level of STOP bit is ignored
	//        RX enabled
	//        ninth bits are zeros
	//        clear RI0 and TI0 bits
	if (SYSTEMCLOCK/BAUDRATE/2/256 < 1)
	{
		TH1 = -(SYSTEMCLOCK/BAUDRATE/2);
		CKCON &= ~0x0B;                  // T1M = 1; SCA1:0 = xx
		CKCON |=  0x08;
	}
	else if (SYSTEMCLOCK/BAUDRATE/2/256 < 4)
	{
		TH1 = -(SYSTEMCLOCK/BAUDRATE/2/4);
		CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 01
		CKCON |=  0x01;
	}
	else if (SYSTEMCLOCK/BAUDRATE/2/256 < 12)
	{
		TH1 = -(SYSTEMCLOCK/BAUDRATE/2/12);
		CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 00
	}
	else
	{
		TH1 = -(SYSTEMCLOCK/BAUDRATE/2/48);
		CKCON &= ~0x0B;                  // T1M = 0; SCA1:0 = 10
		CKCON |=  0x02;
	}

	TL1 = TH1;                          // init Timer1
	TMOD &= ~0xf0;                      // TMOD: timer 1 in 8-bit autoreload
	TMOD |=  0x20;
	TR1 = 1;                            // START Timer1
	TxLock = 1;                       // Flag showing that UART can transmit
	IP |= 0x10;                         // Make UART high priority
	ES0 = 1;                            // Enable UART0 interrupts
}

//-----------------------------------------------------------------------------
// Interrupt Service Routines
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Timer0_ISR
//-----------------------------------------------------------------------------
void Timer0_ISR (void) interrupt 1
{
	static int counter = 0;

	if((counter++) >= 1000)
	{
		counter = 0;
		idletime++;
		adc_timeout++;

		if(idletime>=idletimeNET)//5min
		{
			idletime=0;
			poweroffFlag=1;
		}
		if(adc_timeout>=600)//10min
		{
			adc_timeout=0;
			adc_flag=1;
		}
	}
}
//-----------------------------------------------------------------------------
// UART0_Interrupt
//-----------------------------------------------------------------------------
//
// This routine is invoked whenever a character is entered or displayed on the
// Hyperterminal.
//
//-----------------------------------------------------------------------------

void UART0_Interrupt (void) interrupt 4
{
	idletime=0;//有按键接收则，空闲等待时间置0，从新计数

	if (RI0 == 1)
	{
		if( u8Rxindex == 0)
		{
			// If new word is entered
			u8getQunindex = 0;
		}

		RI0 = 0;                           // Clear interrupt flag

		Byte = SBUF0;                      // Read a character from UART

		if (u8Rxindex < UART_BUFFERSIZE)
		{
			UARTgetBuf[u8getQunindex] = Byte; // Store in array

			u8Rxindex++;             // Update array's size

			u8getQunindex++;             // Update counter
		}
	}

	if (TI0 == 1)                   // Check if transmit flag is set
	{
		TI0 = 0;                           // Clear interrupt flag

		if (tx_count <12)         // 总共发送12个字节
		{

			// Store a character in the variable byte
			SBUF0= tx_buf[tx_count++];

		}
		else
		{
			tx_count = 0;            // Set the array size to 0
			TxLock = 1;                    // Indicate transmission complete
		}
	}
}

//键值处理
void KEY_VAL(uint8 tmp)
{
	switch (tmp)
	{
	case LCtrl:
	{
		tx_buf[4]|=0x01;
		break;
	}
	case LShift:
	{
		tx_buf[4]|=0x02;
		break;
	}
	case LAlt:
	{
		tx_buf[4]|=0x04;
		break;
	}
	case LGui:
	{
		tx_buf[4]|=0x08;
		break;
	}
	case RShift:
	{
		tx_buf[4]|=0x20;
		break;
	}
	case RAlt:
	{
		tx_buf[4]|=0x40;
		break;
	}
	case LCtrl+0x80:
	{
		tx_buf[4]&=~0x01;
		break;
	}
	case LShift+0x80:
	{
		tx_buf[4]&=~0x02;
		break;
	}
	case LAlt+0x80:
	{
		tx_buf[4]&=~0x04;
		break;
	}
	case LGui+0x80:
	{
		tx_buf[4]&=~0x08;
		break;
	}
	case RShift+0x80:
	{
		tx_buf[4]&=~0x20;
		break;
	}
	case RAlt+0x80:
	{
		tx_buf[4]&=~0x40;
		break;
	}
	case Fn:
	{
		fn_flag=FnDown;
		tx_buf[6]=0x00;
		break;
	}
	case Fn+0x80:
	{
		fn_flag=FnUp;
		tx_buf[6]=0x00;
		tx_buf[7]=0x00;
		tx_buf[8]=0x00;
		tx_buf[9]=0x00;
		tx_buf[10]=0x00;
		tx_buf[11]=0x00;
		break;
	}
	default :
	{
		if(tmp & 0x80)
		{
			tx_buf[6]=0x00;
			tx_buf[7]=0x00;
			tx_buf[8]=0x00;
			tx_buf[9]=0x00;
			tx_buf[10]=0x00;
			tx_buf[11]=0x00;
		}
		else
		{
			if(fn_flag==FnUp)
			{
				tx_buf[6]=key_code[tmp];
			}

			if(fn_flag==FnDown)	//Fn键按下
			{
				if(tmp==0x2B)//Fn+F//重新连接
				{
					tx_buf[6]=0x00;
					resetflag=1;//重新连接标志置1
				}
				if(tmp==0x5D)//Fn+A=1c///0x5D(ok)//关机标志
				{
					tx_buf[6]=0x00;
					poweroffFlag=1;//关机标志置1
				}
				if(tmp==0x64)//Fn+S=1b///0x4A/67(BAT)//电池电量
				{
					ADC0_Init();
					tx_buf[6]=batt[bat_val/100];
					tx_buf[7]=batt[bat_val%100/10];
					tx_buf[8]=batt[bat_val%10];
					tx_buf[9]=batt[10];//B
					tx_buf[10]=batt[11];//A
					tx_buf[11]=batt[12];//T

				}
				if(tmp==0x1B)//Fn+S=1b//延长自动关机时间至
				{
					if(idletimeNET<600)
					{
						idletimeNET=1800;//30min

						tx_buf[6]=batt[3];
						tx_buf[7]=batt[0];
					}
					else
					{
						idletimeNET=300;//5min
						tx_buf[6]=batt[0];
						tx_buf[7]=batt[5];
					}

					tx_buf[8]=batt[13];//m
					tx_buf[9]=batt[14];//i
					tx_buf[10]=batt[15];//n
					tx_buf[11]=0x00;
				}
				else
				{
					tx_buf[6]=fn_key_code[tmp];
				}
			}
		}

		break;
	}
	}

}


/*=================================================================*/
/*
	delay_ms(1);  //n=600,1ms,11.0592MHz
	delay_us(1);  //1us

*/
/******************************delay time**********************************
void delay_us(uint8 us)
{
 while(us--);
} 																		*/
//**************************
void delay_ms(uint16 ms)
{
	uint16 n ;
	while(ms--)
	{
		n  =  500;
		while(n--);
	}
}
//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
