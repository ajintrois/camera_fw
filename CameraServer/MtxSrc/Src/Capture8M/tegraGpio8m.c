/********************************************************************************************/
/*		Project	:	ORIN						       */
/*		Filename	:	GPIO.c						       */
/*		Functionality	:	GPIO Process					       */
/*		Author		:	Maheen Rasheed					       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#include <stdio.h>
#include <unistd.h>  
#include <sys/stat.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <gpiod.h>


//sudo apt install libgpiod-dev python3-libgpiod
//sudo apt install busybox

//https://www.ics.com/blog/gpio-programming-exploring-libgpiod-library
//https://kernel.googlesource.com/pub/scm/libs/libgpiod/libgpiod/+/v0.2.x/README.md

//g++ GPIO.cpp -o gpio -lgpiod

/********************************************************************************************/
/*                          Defines	                                                    */
/********************************************************************************************/
/*
 *GPIO
 */
#define GPIO01		1
#define GPIO02		2
#define GPIO03		3
#define GPIO05		5
#define GPIO07		7
#define GPIO08		8
#define GPIO09		9
#define GPIO10		10
#define GPIO11		11
#define GPIO12		12
#define GPIO13		13
#define GPIO14		14
#define UART1_RTS	15
#define UART1_CTS	16

#define INPUT		0
#define OUTPUT		1

#define LVL_LOW		0
#define LVL_HIGH	1

#define LOW		0
#define HIGH		1
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
const char *_GpioValue[]=
{
	"0",
	"1"
};
const char *_GpioName[]=
{
	"",	
	"PQ.05",
	"PP.06",
	"PCC.00",
	"",
	"PCC.02",
	"",
	"PG.06",
	"PQ.02",
	"PAC.06",
	"PEE.02",
	"PQ.06",
	"PN.01",
	"PH.00",
	"PX.03",
	"PR.04",//UART1-RTS
	"PR.05"	//UART1-CTS
};
const int GpioLine[]=
{
	0,	
	105,
	98,
	12,
	0,
	14,
	0,
	41,
	102,
	144,
	25,
	106,
	85,
	43,
	117,
	112,	//UART1-RTS
	113	//UART1-CTS
};
const char *_GpioChip[]=
{
	"",	
	"gpiochip0",
	"gpiochip0",
	"gpiochip1",
	"",
	"gpiochip1",
	"",
	"gpiochip0",
	"gpiochip0",
	"gpiochip0",
	"gpiochip1",
	"gpiochip0",
	"gpiochip0",
	"gpiochip0",
	"gpiochip0",
	"gpiochip0",//UART1-RTS
	"gpiochip0" //UART1-CTS
};
const char *_GpioNumber[]=
{
	"",	
	"453",
	"446",
	"328",
	"",
	"330",
	"",
	"389",
	"450",
	"492",
	"341",
	"454",
	"433",
	"391",
	"465",
	"460",	//UART1-RTS
	"461"	//UART1-CTS
};

const char *_GpioUserName[]=
{
	"",	
	"GPIO01",
	"GPIO02",
	"GPIO03",
	"",
	"GPIO05",
	"",
	"GPIO07",
	"GPIO08",
	"GPIO09",
	"GPIO010",
	"GPIO011",
	"GPIO012",
	"GPIO013",
	"GPIO014",
	"UART1-RTS",	//UART1-RTS
	"UART1-CTS"	//UART1-CTS
};


const char *_GpioBusyBoxOut[]=
{
	"",	
	"busybox devmem 0x02430068 w 0x008",
	"busybox devmem 0x02430030 w 0x000",
	"busybox devmem 0x0c302048 w 0x1001",
	"",
	"busybox devmem 0x0c302028 w 0x001",
	"",
	"busybox devmem 0x02434080 w 0x005",
	"busybox devmem 0x02430050 w 0x1001",
	"busybox devmem 0x02448030 w 0x00A",
	"busybox devmem 0x0c301050 w 0x002",
	"busybox devmem 0x02430070 w 0x008",
	"busybox devmem 0x02440020 w 0x005",
	"busybox devmem 0x02434040 w 0x004",
	"busybox devmem 0x0243d098 w 0x000",
	"busybox devmem 0x0243009c w 0x000",	//UART1-RTS
	"busybox devmem 0x02430090 w 0x005"	//UART1-CTS
};
const char *_GpioBusyBoxIn[]=
{
	"",	
	"busybox devmem 0x02430068 w 0x058",
	"busybox devmem 0x02430030 w 0x050",
	"busybox devmem 0x0c302048 w 0x1051",
	"",
	"busybox devmem 0x0c302028 w 0x051",
	"",
	"busybox devmem 0x02434080 w 0x055",
	"busybox devmem 0x02430050 w 0x1051",
	"busybox devmem 0x02448030 w 0x05A",
	"busybox devmem 0x0c301050 w 0x052",
	"busybox devmem 0x02430070 w 0x058",
	"busybox devmem 0x02440020 w 0x055",
	"busybox devmem 0x02434040 w 0x054",
	"busybox devmem 0x0243d098 w 0x050",
	"busybox devmem 0x0243009c w 0x050",	//UART1-RTS
	"busybox devmem 0x02430090 w 0x055"	//UART1-CTS
};
const char *_GpioDirection[]=
{
	"in",
	"out"
};

unsigned long _GpioPinmuxMemAddr[]=
{
	0,	
	0x02430068,
	0x02430030,
	0x0c302048,
	0,
	0x0c302028,
	0,
	0x02434080,
	0x02430050,
	0x02448030,
	0x0c301050,
	0x02430070,
	0x02440020,
	0x02434040,
	0x0243d098,
	0x0243009c,
	0x02430090	//UART1-CTS
};
const char *chipname[] = 
{
	"gpiochip0",
	"gpiochip1"
};

struct gpiod_chip *chip[17];
struct gpiod_line *gpioFd[17];

/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
void GpioActivate(int gpionumber,int direction);
void GpioSet(int gpionumber,int digval);
int GpioSense(int gpionumber);
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
void GpioActivate(int gpionumber,int direction)
{
	int gpio_fd = -1;
	int ret, i;
	char buffer[100];
	DIR* dir;
	unsigned long pinmux_val;
	FILE *fp;

	
 
	if(direction==OUTPUT)
	{
		snprintf(buffer, 100,"busybox devmem 0x%lx",_GpioPinmuxMemAddr[gpionumber]);
		fp = popen(buffer, "r");
		i = fscanf(fp, "%lx", &pinmux_val);
		pclose(fp);	
		//To use a pin as the GPIO, ensure that the E_IO_HV field is disabled in the corresponding pinmux register of the GPIO pin. Bit 5
		//Set GPIO to Bit 10 = 0, For the output, set Bit 4 = 0 ; Bit 6 = 0
		snprintf(buffer, 100,"busybox devmem 0x%lx w 0x%lx",_GpioPinmuxMemAddr[gpionumber],(pinmux_val & 0xFFFFFB8F)); 
		//printf("%s\n",buffer);
		fp = popen(buffer, "r");
		pclose(fp);	
//		sys tem(buffer) ;
		
		//sys tem(_GpioBusyBoxOut[gpionumber]);
	}
	else
	{
		if(direction==INPUT)
		{
			snprintf(buffer, 100,"busybox devmem 0x%lx",_GpioPinmuxMemAddr[gpionumber]);
			fp = popen(buffer, "r");
			i = fscanf(fp, "%lx", &pinmux_val);
			pclose(fp);	
			//To use a pin as the GPIO, ensure that the E_IO_HV field is disabled in the corresponding pinmux register of the GPIO pin. Bit 5
			//Set GPIO to Bit 10 = 0, For Input, set Bit 4 = 1 ; Bit 6 = 1.
			snprintf(buffer,100, "busybox devmem 0x%lx w 0x%lx",_GpioPinmuxMemAddr[gpionumber],(pinmux_val & 0xFFFFFB8F)|0x00000050); 
			printf("%s\n",buffer);
			fp = popen(buffer, "r");
			pclose(fp);	
			//sy stem(buffer) ;

			//sy stem(_GpioBusyBoxIn[gpionumber]);
		}
	}
	
	chip[gpionumber] = gpiod_chip_open_by_name(_GpioChip[gpionumber]);
	// Open GPIO lines
	gpioFd[gpionumber] = gpiod_chip_get_line(chip[gpionumber], GpioLine[gpionumber]);

	if(direction==OUTPUT)
	{
		// Open lines for output
		gpiod_line_request_output(gpioFd[gpionumber], _GpioUserName[gpionumber], 0);
	}
	else
	{
		if(direction==INPUT)
		{
			gpiod_line_request_input(gpioFd[gpionumber], _GpioUserName[gpionumber]);
		}
	}

  
  	
  	
  	
  	
  	 // Open switch line for input
	//ret = gpiod_line_get_value(gpioFd);	
	//printf("%d\n",ret);
	
	//while(1);
	
	//gpiod_line_release(gpioFd);

}

/*	--------- OLD CODE/CONFIG -------------
Tegra Jetson Nano GPIO definitions ad Naming...
-----------------------------------------------
PIN NAME  CONNECTOR Number	tegra GPIO number
--------  ----------------	-----------------
UART1 CTS	208		GPIO51				
UART1 RTS	207		GPIO50
GPIO09		211		GPIO216
GPIO12		218		GPIO194
GPIO01		118		GPIO149
GPIO11		216		GPIO200
GPIO13		228		GPIO38
GPIO07		206		GPIO168
*/

int initGpio()
{
    //  read physical memory (needs root)

/*	--------- OLD CODE/CONFIG -------------
	tegra_gpio_enable(168);
	tegra_gpio_enable(194);
	tegra_gpio_enable(149);//ICREN
	tegra_gpio_enable(51); //ICRDIR
	tegra_gpio_enable(38);
	tegra_gpio_enable(200);
	tegra_gpio_direction_output(168, 0);
	tegra_gpio_direction_input(194);
	tegra_gpio_direction_output(38, 0);
	tegra_gpio_set(38, 0);
	tegra_gpio_direction_output(149, 1);
	tegra_gpio_direction_output(51, 1);
	tegra_gpio_direction_output(200, 1);
*/
	//GpioActivate(GPIO01,OUTPUT);
	//GpioActivate(GPIO02,OUTPUT);
	//GpioActivate(GPIO03,OUTPUT);
	//GpioActivate(GPIO05,OUTPUT);
	//GpioActivate(GPIO07,OUTPUT);
	//GpioActivate(GPIO08,OUTPUT);
	//GpioActivate(GPIO09,OUTPUT);
	//GpioActivate(GPIO10,OUTPUT);
	//GpioActivate(GPIO11,OUTPUT);
	GpioActivate(GPIO12,INPUT);
	GpioActivate(GPIO13,OUTPUT);
	//GpioActivate(GPIO14,OUTPUT);
	//GpioActivate(UART1_RTS,OUTPUT);
	//GpioActivate(UART1_CTS,OUTPUT);
	//GpioSet(GPIO01,HIGH);
	//GpioSet(GPIO11,HIGH);
	//GpioSet(GPIO13,HIGH);
	//GpioSet(UART1_CTS,HIGH);

    return 0;
}

int closeGpio()
{
	return 0;
}

int GetVsync()
{
	return(GpioSense(GPIO12));
}


#if 0
int main(void)
{
	int i;
	initGpio();
	while(1)
	{
		if(GpioSense(GPIO12))
		{
			if(i == 0)
			{
				printf("gpioval = 1");
				i = 1;
			}
		}
		else
		{
			if(i == 1)
			{
				printf("gpioval = 0");
				i = 0;
			}
		}
		usleep(1000);
	}
	closeGpio();
}
#endif



void GpioSet(int gpionumber,int digval)
{
	gpiod_line_set_value(gpioFd[gpionumber], digval);
}

int GpioSense(int gpionumber)
{

	int level = 0;
	level = gpiod_line_get_value(gpioFd[gpionumber]);

	return level;
}


