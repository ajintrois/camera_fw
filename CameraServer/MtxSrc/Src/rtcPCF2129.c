#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
//#include <sys/ioctl.h>

#define	I2C_ADDR_PCF2129 	0x51  //0xA2>>1  
/*
 * I2C driver params
 */
//#define BYTES_PER_PAGE  32      /* one eeprom page is 256 byte */
#define MAX_BYTES       128  	/* max number of bytes to write in one piece */
#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/
#define I2C_RD		0x01


#define BCD_2_BIN00(hex)	((hex >> 4) * 10 + (hex & 0xf))
#define BIN_2_BCD00(hex)	((((hex % 100) / 10) << 4) + ((hex % 100) % 10))


struct timecode
{
	unsigned char seconds;
	unsigned char minutes;
	unsigned char hour;
	unsigned char month;
	unsigned char date;
	unsigned char day_of_week;
	unsigned char year;
};

int battery_low = 0;
int power_loss	= 0;

int bin2bcd_int(int l)
{
	int k = 0, c = 0;
	while(l)
	{
		k |= ((l%10) << (c++*4));
		l /= 10;
	}
	return k;
}

char bin2bcd_char(char l)
{
	int k = 0, c = 0;
	while(l)
	{
		k |= ((l%10) << (c++*4));
		l /= 10;
	}
	return (char)k;
}

char bcd2bin_char(char l)
{
	int k = 0, c = 0;
	c= ((l & 0xf0) >> 4)*10;
	k = c + (l & 0x0f);
	return (char)k;
}

static int eeprom_write(int fd, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i, j;
  unsigned char _buf[MAX_BYTES];

  i = 0;
  _buf[i++] = offset;    //_buf[0] is the offset addr!
  len += 1;

  for(j=0;i<len;i++) // i presetted, copy buf[0..n] -> _buf[1..n+1] 
    _buf[i]=buf[j++];

  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;

  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = 0;
  i2cmsg.len   = len;
  i2cmsg.buf   = _buf;
  
again:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
     if(i == -1&& (errno==EINTR||errno==121))
     {
       goto again;
     }
     else
     {
	return -1;
     }
  }  
  return 0;
}


static int eeprom_addr_write(int fd,unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i, j;  
  unsigned char _buf[MAX_BYTES];

  i = 0;
  _buf[i++] = offset;    //_buf[0] is the offset addr!
  len += 1;
   for(j=0;i<len;i++) // i presetted, copy buf[0..n] -> _buf[1..n+1] 
    _buf[i]=buf[j++];

  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;

  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = 0;
  i2cmsg.len   = len;
  i2cmsg.buf   = _buf;

again:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
    if(i == -1&& (errno==EINTR||errno==121))
    {
      goto again;
    }
    else
    {
      return -1;
    }
  }
  return 0;
}

static int eeprom_read(int fd, unsigned int slave_addr, unsigned int offset,unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;  
  int i;

  // first write the addr to the slave device 
  if(eeprom_addr_write(fd,slave_addr,offset,NULL,0)<0)
    return -1;

  msg_rdwr.msgs = &i2cmsg;  
  msg_rdwr.nmsgs = 1;
  
  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = I2C_RD;
  i2cmsg.len   = len;
  i2cmsg.buf   = buf;

again:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
    if(i == -1&& (errno==EINTR||errno==121))
    {
      goto again;
    }
    else
    {
      return -1;
    }
  }  
  return 0;
}


/*
control REG 0
-------------
BIT 7	EXT_TEST -> External clock test mode
BIT 6	0	 -> Res..
BIT 5	STOP	 -> stop bit
BIT 4	TSF1	 -> timestamp flag 1
BIT 3	POR_OVRD -> power-on reset (POR) override
BIT 2	12_24	 -> 12/24 hour mode
BIT 1	MI	 -> minute interrupt
BIT 0	SI	 -> second interrupt

control REG 1
-------------
BIT 7	MSF	-> minute/second flag
BIT 6	WDTF	-> watchdog timer flag
BIT 5	TSF2	-> timestamp flag 2
BIT 4	AF	-> alarm flag
BIT 3	0	
BIT 2	TSIE	-> timestamp interrupt enable
BIT 1	AIE	-> alarm interrupts enable
BIT 0	0

control REG 2
-------------
BIT 7	
BIT 6		
BIT 5	PWRMNG
BIT 4	BF
BIT 3	BLF
BIT 2	BIE
BIT 1	BTSE
BIT 0	BLIE
*/

void check_rtc(int handle)// init rtc and check for errors and correct 
{
  unsigned char data1[8];
//  int ret = 0;
  //control reg 0 default data = 0
  eeprom_read(handle, I2C_ADDR_PCF2129, 0, data1, 1);
  if((data1[0] & 0xA8) != 0)
  {
//    printf("rtc ctrl 0 reg default value not found val = 0x%x\n",data1[0]);
    data1[0] = 0;
    eeprom_write(handle, I2C_ADDR_PCF2129, 0, data1, 1);
    //ret = -1;
  }
  //control reg 1 default data = 0.. not important.
  data1[0] = 0;
  eeprom_write(handle, I2C_ADDR_PCF2129, 1, data1, 1);// flags should be cleared..
  //control reg 2 default data = 0.. not important.
  eeprom_read(handle, I2C_ADDR_PCF2129, 2, data1, 1);
  if((data1[0] & 0x04) != 0)
  {
//    printf("rtc ctrl battery low detected val = 0x%x\n", data1[0]);
    battery_low = 1;
  }
  //data1[0] = 0;
  eeprom_write(handle, I2C_ADDR_PCF2129, 2, data1, 1);// flags should be cleared..
  eeprom_read(handle, I2C_ADDR_PCF2129, 3, data1, 1);
  if((data1[0] & 0x80) != 0)
  {
 //   printf("rtc sec, time stopped or invalid time val = 0x%x\n", data1[0]);
    power_loss = 1;// clock stoppped
    data1[0] = 0;
    eeprom_write(handle, I2C_ADDR_PCF2129, 3, data1, 1);// time cleared..
    data1[0] = 1;
    eeprom_write(handle, I2C_ADDR_PCF2129, 8, data1, 1);// month set..
  }
//  ret -= get_time_rtc_PCF2129(ptm);
//  data1[0] = 6;// 500ms timer pulse..
//  eeprom_write(handle, I2C_ADDR_PCF2129, 0x0f, data1, 1);// clockout 1Hz
  data1[0] = 1;// 16kHz timer pulse..
  eeprom_write(handle, I2C_ADDR_PCF2129, 0x0f, data1, 1);// clockout 16kHz
//  data1[0] = 0x21;
//  eeprom_write(handle, I2C_ADDR_PCF2129, 0x0f, data1, 1);// clockout 1kHz
  return;
}

void read_time(int handle, struct timecode *t)
{
  unsigned char data[8];
//  int ret = 0;

  eeprom_read(handle, I2C_ADDR_PCF2129, 2, data, 1);
  if(data[0] & 0x04)
    battery_low = 1;
  eeprom_read(handle, I2C_ADDR_PCF2129, 3, data, 7);
  if((data[0] & 0x80) != 0)
  {
//    printf("rtc sec, time stopped or invalid time val = 0x%x\n", data[0]);
//    ret = -1;
    power_loss = 1;
//    battery_low = 1;
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 1;
	data[4] = 1;
	data[5] = 1;
	data[6] = 0;// year = 2000
  }
  data[0] = data[0] & 0x7F;
  t->seconds = BCD_2_BIN00(data[0]);// 0-59
  t->minutes = BCD_2_BIN00(data[1]);// 0-59
  t->hour = BCD_2_BIN00(data[2]);// always 24 hr 0-23
  t->date = BCD_2_BIN00(data[3]);// 1-31
  if(data[5] == 0)
  {
//    printf("rtc mon fatal error val = %d\n",data[5]);
//    ret = -1;
    data[5] = 1;
  }
  t->month = ((BCD_2_BIN00(data[5]))-1);// 0-11 according to tm
  t->year = (BCD_2_BIN00(data[6]))+100;// since 1900 so 100+data(14)
//		printf("RTCTime READ = %02d:%02d:%02d %02d-%02d-%d\n", t->hour, t->minutes, t->seconds, t->date, t->month, t->year+1900);
  return;
}


static int i2c_dev_handle = 0;
void init_rtc()
{
	FILE *fptr=NULL;
	char i2cdev_name[32] = "/dev/i2c-1"; 
	i2c_dev_handle = open(i2cdev_name, O_RDWR);
	if(i2c_dev_handle > 0)
	{
//		printf("rtc ready to access %d \n", i2c_dev_handle);
		//check_rtc(i2c_dev_handle);
		fptr=fopen("OSF","rb");
		if(fptr!=NULL)
		{
			power_loss = 1;	
//			printf("Oscillator Stop Flag Set\n");		
			fclose(fptr);
		}
		fptr=fopen("BATLOW","rb");
		if(fptr!=NULL)
		{
			battery_low = 1;			
//			printf("Battery Low Flag Set\n");		
			fclose(fptr);
		}		
	}
	else
		i2c_dev_handle = 0;

}
void close_rtc()
{
	if(i2c_dev_handle > 0)
	close(i2c_dev_handle);
}

void get_rtc_time( volatile unsigned char *ptimecode)
{
	/*struct timecode time_val;
	if(i2c_dev_handle > 0)
	{
		read_time(i2c_dev_handle, &time_val);
		ptimecode[0] = time_val.seconds;//seconds
		ptimecode[1] = time_val.minutes;//minutes
		ptimecode[2] = time_val.hour;//hour
		ptimecode[3] = time_val.date;//date
		ptimecode[4] = time_val.month;//month
		ptimecode[5] = time_val.year;//year
	}
	else
		printf("rtc error read access %d \n", i2c_dev_handle);*/

	struct timeval tval;
	struct tm *ltime, stime;
	struct tm		brokentime;


	gettimeofday(&tval,NULL);
	localtime_r(&tval.tv_sec,&brokentime);
	ptimecode[0] = brokentime.tm_sec;//seconds
	ptimecode[1] = brokentime.tm_min;//minutes
	ptimecode[2] = brokentime.tm_hour;//hour
	ptimecode[3] = brokentime.tm_mday;//date
	ptimecode[4] = brokentime.tm_mon;//month
	ptimecode[5] = brokentime.tm_year;//year
	//printf("Time = %d:%d:%d %d:%d:%d\n",ptimecode[2],ptimecode[1],ptimecode[0],ptimecode[3],ptimecode[4]+1,ptimecode[5]+1900);
		

}

void set_rtc_time( volatile unsigned char *ptimecode)
{
	/*struct timecode time_val;
	if(i2c_dev_handle > 0)
	{
		time_val.seconds = ptimecode[0];//seconds
		time_val.minutes = ptimecode[1];//minutes
		time_val.hour = ptimecode[2];//hour
		time_val.date = ptimecode[3];//date
		time_val.month = ptimecode[4];//month
		time_val.year = ptimecode[5];//year

		write_time(i2c_dev_handle, &time_val);
		printf("RTCTime = %02d:%02d:%02d %02d-%02d-%d\n", time_val.hour, time_val.minutes, time_val.seconds, time_val.date, time_val.month, time_val.year+1900);
	}
	else
			printf("rtc error write access %d \n", i2c_dev_handle);*/
}


