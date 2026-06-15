#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

//#include <sys/ioctl.h>

#define I2C_DEV_NAME		"/dev/i2c-10"
#define	I2C_ADDR_IMX392	 	0x1A  //0xA2>>1  
#define	I2C_MTX_TRIG_ATTINY	0x30
/*
 * I2C driver params
 */
//#define BYTES_PER_PAGE  32      /* one eeprom page is 256 byte */
#define MAX_BYTES       128  	/* max number of bytes to write in one piece */
#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/
#define I2C_RD		0x01

extern int DeadDelayMS(int ms);

struct reg_8
{
  unsigned short addr;		// slave address			
  unsigned char data;		// pointer to msg data			
};
struct camera_common_frmfmt
{
	unsigned int	*widthxheight;
	int		*framerates;
	int		id;
	int		res1;
	int		res2;
};
#include "imx219_mode_tbls.h"

extern unsigned int IMX264Shutter[33];
#define IMX264_I2C_REG_SHUTTER_SWEEP	0x3240
//#include "IMX264ShutterGain.h"

static int eeprom_test_write(int fd, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i;
  unsigned char _buf[MAX_BYTES];

  _buf[0]=slave_addr;

  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;

  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = 0;
  i2cmsg.len   = 1;
  i2cmsg.buf   = _buf;
  
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
    return -1;
  }  
  return 0;
}

static int eeprom_write(int fd, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i, j;
  unsigned char _buf[MAX_BYTES];

	i = 0;
	if(slave_addr == I2C_ADDR_IMX392)
	{
	  _buf[i++] = (unsigned char)(offset>>8);    //_buf[1] is the offset addr MS!
	  _buf[i++] = (unsigned char)offset;    //_buf[0] is the offset addr LS!
	  len += 2;
	}
	else
	{
	  _buf[i++] = offset;    //_buf[0] is the offset addr!
	  len += 1;
	}
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
	//pri ntf("retval=%d:errno=%d:i2c Data Write Error %s\n",i,errno, strerror(errno));
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
	if(slave_addr == I2C_ADDR_IMX392)
	{
	  _buf[i++] = (unsigned char)(offset>>8);    //_buf[1] is the offset addr MS!
	  _buf[i++] = (unsigned char)offset;    //_buf[0] is the offset addr LS!
	  len += 2;
	}
	else
	{
	  _buf[i++] = (unsigned char)offset;    //_buf[0] is the offset addr!
	  len += 1;
	}
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
      //pri ntf("retval=%d:errno=%d:i2c Addr Error, %s\n",i,errno, strerror(errno));
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

	if(slave_addr == I2C_MTX_TRIG_ATTINY)
		usleep(500);

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
     // prin tf("retval=%d:errno=%d:i2c Read Error %s\n",i,errno, strerror(errno));
      return -1;
    }
  }  
  return 0;
}
/*
static void display_buffer(unsigned char * buffer, unsigned int size)
{
	int i, data_len, count = 0, stcnt;
	char c;
	data_len = size & 0xff8;
	while (data_len)
	{ 
		prin tf("\n%04x:",count);
		stcnt = count;
		for(i = 0; i < 8; i++)
		{
			pr intf(" %02x",buffer[stcnt++]);
		}
		pri ntf("  ");
		stcnt = count;
		for(i = 0; i < 8; i++)
		{
			c= buffer[stcnt++];
			if((c >= 0x30) && (c <= 0x39))
				prin tf("%c",c);
			else if((c >= 65) && (c < 91))
				pri ntf("%c",c);
			else if((c >= 97) && (c < 123))
				pri ntf("%c",c);
			else 
				pri ntf(".");
		}
		count = stcnt;
		data_len -= 8;
	}
	pri ntf("\n");
}
*/
int ahextoi_1(char * chptr,char count)
{
  int i,j;
  int ret = 0;
  for(i = 0; i < count; i++)
  {
    if(chptr[i] != 0)
    {
      j = 0;
      if((chptr[i] >= 0x30) && (chptr[i] < 0x3a))
      {
	j = (chptr[i] - 0x30);
      }
      else if((chptr[i] >= 0x41) && (chptr[i] < 0x47))
      {
	j = ((chptr[i] - 0x41)+10);
      }
      else if((chptr[i] >= 0x61) && (chptr[i] < 0x67))
      {
	j = ((chptr[i] - 0x61)+10);
      }
      else
      break;     
      ret <<= 4; 
      ret += j;
    }
    else
      break;
  }
  return ret;
}

int init_i2c(int * pi2c_dev_handle, char *i2cdev_name)
{
	int ret = 0;
	int i2c_dev_handle = open(i2cdev_name, O_RDWR);
	if(i2c_dev_handle > 0)
	{
//		pri ntf("i2c %s opened\n",i2cdev_name);
		*pi2c_dev_handle = i2c_dev_handle;
		ret = 1;
	}
	else
	{
		*pi2c_dev_handle = 0;
		//pri ntf("i2c %s not opened, %s\n",i2cdev_name, strerror(errno));
	}
	return i2c_dev_handle;
}

void close_i2c(int * pi2c_dev_handle)
{
	if(*pi2c_dev_handle > 0)
		close(*pi2c_dev_handle);
	*pi2c_dev_handle = 0;
}
void check_if_dev_present(int * pi2c_dev_handle, char *i2cdev_name, char addr)
{
  unsigned char data1[8] = {0xff};
  init_i2c(pi2c_dev_handle, i2cdev_name);
  if(*pi2c_dev_handle)
  {
  	eeprom_read(*pi2c_dev_handle, addr, 0, data1, 1);
//	  if(eeprom_read(*pi2c_dev_handle, addr, 0, data1, 1) < 0)
//		pri ntf("read error at addr %x\n", addr);
//	  else
//		pri ntf("read data %x at addr %x\n", data1[0],addr);
  }
}


int CloseSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 

	close_i2c(&i2c_dev_handle);
	return 0;
}

int InitSensorI2c1(int * pi2c_dev_handle)
{
	char i2cdev_name[32] = I2C_DEV_NAME; 
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	int i2c_dev_handle = 0;
	unsigned int vmax = 0, vmaxbak, shutter0 = 1500, gain0 = 16, shutter1 = 1200, gain1 = 32;

//	pri ntf("opening %s with addr= 0x%x\n",i2cdev_name, i2cdev_addr);

	if(init_i2c(&i2c_dev_handle, i2cdev_name) == 0)//failed
		return 0;

	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	data[0] = 0x0F;	//tout12sel Exposure pulse out
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3435, data, 1);	//tout12 sel Exposure pulse out

	data[0] = 0x21;	//
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x343A, data, 1);	//tout12 pulse trigger out

	data[0] = 0x01;	//tout0sel Exposure period monitoring
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3436, data, 1);	//tout0sel Exposure period monitoring
	data[0] = 0xC0;	//HS VS out enable...
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x343C, data, 1);	//tout0sel Exposure period monitoring

	data[0] = 0x01;	// vertical inverted readout....
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3204, data, 1);	//

//	vmax = 0xF00;
//	data[2] = (unsigned char)(vmax >> 16);
//	data[1] = (unsigned char)(vmax >> 8);
//	data[0] = (unsigned char)vmax;
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x30D4, data, 3);


	data[0] = 0x00;	//disable 
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3208, data, 1);	// multi frame set output


//	vmax = 1328;// set hmax..
//	data[0] = (unsigned char)vmax;
//	data[1] = (unsigned char)(vmax >> 8);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x30D8, data, 2);// read Vmax for shutter calculation

	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	
	*pi2c_dev_handle = i2c_dev_handle;
//	CloseSensorI2c(i2c_dev_handle);
	return 0;
}
int InitSensorI2c2(int * pi2c_dev_handle, int res)
{
	char i2cdev_name[32] = I2C_DEV_NAME; 
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	int i2c_dev_handle = 0;
	unsigned int vmax = 0, vmaxbak, shutter0 = 1500, gain0 = 16, shutter1 = 1200, gain1 = 32;

	i2c_dev_handle = *pi2c_dev_handle;

	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	data[0] = 0x0F;	//tout12sel Exposure pulse out
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3435, data, 1);	//tout12 sel Exposure pulse out

	data[0] = 0x21;	//
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x343A, data, 1);	//tout12 pulse trigger out

	data[0] = 0x01;	//tout0sel Exposure period monitoring
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3436, data, 1);	//tout0sel Exposure period monitoring



//	data[0] = 0x00;	//disable 
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3208, data, 1);	// multi frame set output

	if(res == 0)// 12mp
	{
		data[0] = 0x06;	//
		data[1] = 0xC;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3479, data, 3);	// pulse1 start..
		data[0] = 0x38;	//
		data[1] = 0xC;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x347C, data, 3);	// pulse1 end..
	}
	else
	{
		data[0] = 0xFA;	//
		data[1] = 0x8;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3479, data, 3);	// pulse1 start..
		data[0] = 0x2C;	//
		data[1] = 0x9;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x347C, data, 3);	// pulse1 end..
	}
	data[0] = 0x1;	// 
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3478, data, 1);	// pulse1 enable..

/*	data[0] = 0xA6;	//
	data[1] = 0xC;	//
	data[2] = 0;	//
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3481, data, 3);	// pulse2 start..
	data[0] = 0xE0;	//
	data[1] = 0xC;	//
	data[2] = 0;	//
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3484, data, 3);	// pulse2 end..
	data[0] = 0x1;	// 
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3480, data, 1);	// pulse2 enable..
*/
	// for vmax 3288...
	if(res == 0)
	{
		data[0] = 0x38;	//
		data[1] = 0x18;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3481, data, 3);	// pulse2 start..
		data[0] = 0x6A;	//
		data[1] = 0x18;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3484, data, 3);	// pulse2 end..
	}
	else
	{
		data[0] = 0x20;	//
		data[1] = 0x12;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3481, data, 3);	// pulse2 start..
		data[0] = 0x50;	//
		data[1] = 0x12;	//
		data[2] = 0;	//
		eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3484, data, 3);	// pulse2 end..
	}
	data[0] = 0x1;	// 
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3480, data, 1);	// pulse2 enable..


	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	// -------------------------------------------
	// WRITE BITS/PIX AND FRAMESIZE...
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	data[0] = 0x05;	// 10 bits per pixel...
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3200, data, 1);	// ADBIT A/D selection / bits per pixel..
	
//	if(res == 0)// 12mp
//	{
//	}
//	else// 8Mp
//	{
//	}
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);

	// read---------------------------------------
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x343C, data, 1);	//tout0sel Exposure period monitoring
	//pr intf("syncsel = %x\n",data[0]);
	vmax = 0;
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x30D2, data, 2);	// read Vmax for shutter calculation
	vmax = data[0];
	vmax |= (data[1] << 8);
	//prin tf("sensor EBD size  = %u\n", vmax);
	vmax = 0;
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x30D4, data, 3);	// read Vmax for shutter calculation
	vmax = data[0];
	vmax |= (data[1] << 8);
	vmax |= (data[2] << 16);
	//pri ntf("sensor vmax = %u\n", vmax);
	vmaxbak = vmax;
	vmax = 0;
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x30D8, data, 2);	// read Vmax for shutter calculation
	vmax = data[0];
	vmax |= (data[1] << 8);
	//pri ntf("sensor hmax = %u\n", vmax);
//	CloseSensorI2c(i2c_dev_handle);

	return 0;
}

int SetShutterLineSensorI2c(int i2c_dev_handle, int shutterLines)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	
	data[0] = (unsigned char)shutterLines;
	data[1] = (unsigned char)(shutterLines >> 8);
	data[2] = (unsigned char)(shutterLines >> 16);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240, data, 3);
//	data[0] = (unsigned char)(shutterLines >> 8);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240+1, data, 1);
//	data[0] = (unsigned char)(shutterLines >> 16);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240+2, data, 1);

	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	return 0;
}

int RegInitSensorI2c(int i2c_dev_handle, int *vamx, int *hmax)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned int vmax1 = 0, hmax1 = 0;
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
//	init_sony_sensor(i2c_dev_handle, i2cdev_addr);
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x30D4, data, 3);// read Vmax for shutter calculation
	vmax1 = data[0];
	vmax1 |= (data[1] << 8);
	vmax1 |= (data[2] << 16);
	*vamx = vmax1;
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x30D8, data, 2);// read Vmax for shutter calculation
	hmax1 = data[0];
	hmax1 |= (data[1] << 8);
	*hmax = hmax1;
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	return vmax1;
}
int GetShutterSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned int vmax1 = 0;
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x3240, data, 3);// read  shutter after calculation
	vmax1 = data[0];
	vmax1 |= (data[1] << 8);
	vmax1 |= (data[2] << 16);
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	return vmax1;
}

unsigned int	shuttersweep8M[34] = {
2338,// 0
2336,// 1
2336,// 2
2334,// 3
2332,// 4
2330,// 5
2329,// 6
2327,// 7
2325,// 8
2323,// 9
2319,// 10
2317,// 11
2312,// 12
2307,// 13
2302,// 14
2298,// 15
2293,// 16
2245,// 17
2198,// 18
2150,// 19
2102,// 20
2055,// 21
1960,// 22
1864,// 23
1769,// 24
1674,// 25
1579,// 26
1483,// 27
1388,// 28
1293,// 29
1150,// 30
674,// 31
4,// 32

};
/*
shutter index = 0 time = 45 sweep value = 	2339
shutter index = 1 time = 89 sweep value = 	2338
shutter index = 2 time = 102 sweep value = 	2338
shutter index = 3 time = 145 sweep value = 	2337
shutter index = 4 time = 180 sweep value = 	2336
shutter index = 5 time = 210 sweep value = 	2336
shutter index = 6 time = 250 sweep value = 	2335
shutter index = 7 time = 290 sweep value = 	2334
shutter index = 8 time = 320 sweep value = 	2333
shutter index = 9 time = 370 sweep value = 	2332
shutter index = 10 time = 450 sweep value = 	2330
shutter index = 11 time = 500 sweep value = 	2329
shutter index = 12 time = 600 sweep value = 	2326
shutter index = 13 time = 710 sweep value = 	2324
shutter index = 14 time = 800 sweep value = 	2322
shutter index = 15 time = 900 sweep value = 	2319
shutter index = 16 time = 1000 sweep value = 	2317
shutter index = 17 time = 2000 sweep value = 	2294
shutter index = 18 time = 3000 sweep value = 	2270
shutter index = 19 time = 4000 sweep value = 	2247
shutter index = 20 time = 5000 sweep value = 	2223
shutter index = 21 time = 6000 sweep value = 	2200
shutter index = 22 time = 8000 sweep value = 	2153
shutter index = 23 time = 10000 sweep value = 	2106
shutter index = 24 time = 12000 sweep value = 	2060
shutter index = 25 time = 14000 sweep value = 	2013
shutter index = 26 time = 16000 sweep value = 	1966
shutter index = 27 time = 18000 sweep value = 	1919
shutter index = 28 time = 20000 sweep value = 	1872
shutter index = 29 time = 22000 sweep value = 	1826
shutter index = 30 time = 25000 sweep value = 	1755
shutter index = 31 time = 35000 sweep value = 	1521
shutter index = 32 time = 50000 sweep value = 	1170
*/
int SetShutterSensorI2c(int i2c_dev_handle, int vmax, int shutterindex)
{
int ShutterTimeUS[33] = {
55,//0
89,
102,
145,
180,
210,
250,
290,
320,
370,
450,//10
500,
600,
710,
800,
900,
1000,//16
2000,
3000,
4000,
5000,//20
6000,
8000,
10000,
12000,
14000,//25
16000,
18000,
20000,
22000,
25000,//30
35000,
40000
};
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	int shutter = 0;
	const float lineperiod = 26.9;//uSec
	// 1line = 1/37.125 * hmax = 26.9ns * 1586 = 42.72uSec..
	
	if(vmax > ((int)(ShutterTimeUS[shutterindex]/lineperiod)))
		shutter = vmax - ((int)(ShutterTimeUS[shutterindex]/lineperiod));
	else
		shutter = 16;
//	printf("-----------------shutter = %d ------index = %d ----------------\n", shutter, shutterindex);
/*	shutter = (IMX264Shutter[shutterindex] + (3008 - IMX264Shutter[0]));
	if(vmax > shutter)
		shutter = vmax - (3008 - shutter);*/
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	
	data[0] = (unsigned char)shutter;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240, data, 1);
	data[0] = (unsigned char)(shutter >> 8);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240+1, data, 1);
	data[0] = (unsigned char)(shutter >> 16);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3240+2, data, 1);

/*	data[2] = (unsigned char)(shutterindex >> 16);
	data[1] = (unsigned char)(shutterindex >> 8);
	data[0] = (unsigned char)shutterindex;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
*/	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);

/*	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	
	data[2] = (unsigned char)(shutterindex >> 16);
	data[1] = (unsigned char)(shutterindex >> 8);
	data[0] = (unsigned char)shutterindex;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);*/

	return shutter;
}

int Pulse1WidthExtTrig(int i2c_dev_handle, int val)// val =shutter in microsecs
{
	char i2cdev_addr = I2C_MTX_TRIG_ATTINY; 
	unsigned char data[5] = {0};
	int value = 0;

	value = val/10;
	data[0] = (unsigned char)(value >> 8);// data 0
	data[1] = (unsigned char)(value);// data 1
//	data[0] = 0x07;// data 0
//	data[1] = 0xd0;// data 1
	data[2] = 0x03 + data[0] + data[1];// checksum
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x03, data, 3);
//	pri ntf("pulse 1 i2c data -val=%d- 0x03 ---0x%02x 0x%02x 0x%02x\n", val, data[0], data[1], data[2]);
	return(0);
}

int Pulse2WidthExtTrig(int i2c_dev_handle, int val)// val =shutter in microsecs
{
	char i2cdev_addr = I2C_MTX_TRIG_ATTINY; 
	unsigned char data[5] = {0};
	int value = 0;

	value = val/10;
	data[0] = (unsigned char)(value >> 8);// data 0
	data[1] = (unsigned char)(value);// data 1
//	data[0] = 0x0f;// data 0
//	data[1] = 0xa0;// data 1
	data[2] = 0x04 + data[0] + data[1];// checksum
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x04, data, 3);
//	prin tf("pulse 2 i2c data -val = %d- 0x04 ---0x%02x 0x%02x 0x%02x\n", val, data[0], data[1], data[2]);
	return(0);
}

int checkExtTrigger(int i2c_dev_handle, int sh1, int sh2)
{
	int tsh1 = 0;
	int tsh2 = 0;
	char i2cdev_addr = I2C_MTX_TRIG_ATTINY; 
	unsigned char data[5] = {0};
	
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x83, data, 4);
	if(data[3] == data[0] + data[1]+ data[2])
	{
		tsh1 = (data[1] << 8) | data[2];
	}
//	pri ntf("Read pulse 2 i2c data -sh1 = %d----0x%02x 0x%02x 0x%02x 0x%02x\n", sh1, data[0], data[1], data[2], data[3]);
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x84, data, 4);
	if(data[3] == data[0] + data[1]+ data[2])
	{
		tsh2 = (data[1] << 8) | data[2];
	}
//	prin tf("Read pulse 2 i2c data -sh2 = %d----0x%02x 0x%02x 0x%02x 0x%02x\n", sh2, data[0], data[1], data[2], data[3]);
	if((tsh1 * 10 == sh1) && (tsh2 * 10 == sh2))
		return 0;
	else
		return 1;
}
int StartExtTrig(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_MTX_TRIG_ATTINY; 
	unsigned char data[5] = {0};
	
	data[0] = 0x02;// data 0
	data[1] = 0x00;// data 1
	data[2] = 0x03;// checksum
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x01, data, 3);
	return(0);
}
int StopExtTrig(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_MTX_TRIG_ATTINY; 
	unsigned char data[5] = {0};
	
	data[0] = 0x01;// data 0
	data[1] = 0x00;// data 1
	data[2] = 0x02;// checksum
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x01, data, 3);
	return(0);
}

int SetExtTriggerSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);// reg hold
//	data[0] = 0x00;// data 0
//	data[1] = 0x00;// data 1
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3010, data, 2);
	data[0] = 0x09;// data 0
	data[1] = 0x00;// data 1
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3400, data, 2);
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);// reg update
	return(0);
}

int SetNormalSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);// reg hold
//	data[0] = 0x01;// data 0
//	data[1] = 0x00;// data 1
//	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3010, data, 2);
	data[0] = 0;// ls
	data[1] = 0;// ms
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3400, data, 2);
	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);// reg update
	return(0);
}


int SetGainValSensorI2c(int i2c_dev_handle, int gainval)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	data[0] = (unsigned char)(gainval);
	data[1] = (unsigned char)(gainval >> 8);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3514, data, 2);
	return(0);
}

int SetGainSensorI2c(int i2c_dev_handle, int gainindex, int aperture)
{
int IMX264AnalogGain[8] = {
0,
34,
69,
103,
137,
171,
206,
240
};

	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	
	data[0] = (unsigned char)(IMX264AnalogGain[gainindex]);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3514, data, 1);
	data[0] = (unsigned char)(IMX264AnalogGain[gainindex] >> 8);
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3515, data, 1);

	data[0] = 0;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3034, data, 1);
	return(IMX264AnalogGain[gainindex]);
	
}

int GetSensorTempI2c(int i2c_dev_handle, int *temp)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int shutter = 0;
	int ret = 0;
	static int tempreadstart = 0;
	float	temperature_val;
	
	if(tempreadstart)
	{
		eeprom_read(i2c_dev_handle, i2cdev_addr, 0x3594, data, 3);
		if((data[2] & 1) == 0)
		{
			temperature_val = data[0];
			*temp = (int)((temperature_val - 51.784) / 1.3125);
			ret = 1;
		}
	}
	usleep(100);
	data[0] = 1;
	eeprom_write(i2c_dev_handle, i2cdev_addr, 0x3596, data, 1);
	tempreadstart = 1;
	return ret;
}	


