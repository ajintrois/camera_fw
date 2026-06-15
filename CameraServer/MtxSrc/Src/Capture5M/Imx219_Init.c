#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "IMX264ShutterGain.h"

//#include <sys/ioctl.h>

#define I2C_DEV_NAME		"/dev/i2c-9"
#define	I2C_ADDR_IMX392	 	0x10  //0xA2>>1  
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
#if DEBUG
       printf("i2c error EINTR\n");
#endif
       goto again;
     }
     else
     {
	printf("retval=%d:errno=%d:i2c Data Write Error %s\n",i,errno, strerror(errno));
#if LANDEBUG    
    DEBUGCPY("retval=%d:errno=%d:i2c Data Write Error\n")
    DEBUGPRINT(debug_buffer,i,errno)
#endif 
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
#if DEBUG
      printf("i2c error EINTR\n");
#endif      
      goto again;
    }
    else
    {
      printf("retval=%d:errno=%d:i2c Addr Error, %s\n",i,errno, strerror(errno));
#if LANDEBUG    
    DEBUGCPY("retval=%d:errno=%d:i2c Addr Error\n")
    DEBUGPRINT(debug_buffer,i,errno)
#endif       
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
#if DEBUG
      printf("i2c error EINTR\n");
#endif
      goto again;
    }
    else
    {
      printf("retval=%d:errno=%d:i2c Read Error %s\n",i,errno, strerror(errno));
#if LANDEBUG    
    DEBUGCPY("retval=%d:errno=%d:i2c Read Error\n")
    DEBUGPRINT(debug_buffer,i,errno)
#endif         
      return -1;
    }
  }  
  return 0;
}

static void display_buffer(unsigned char * buffer, unsigned int size)
{
	int i, data_len, count = 0, stcnt;
	char c;
	data_len = size & 0xff8;
	while (data_len)
	{ 
		printf("\n%04x:",count);
		stcnt = count;
		for(i = 0; i < 8; i++)
		{
			printf(" %02x",buffer[stcnt++]);
		}
		printf("  ");
		stcnt = count;
		for(i = 0; i < 8; i++)
		{
			c= buffer[stcnt++];
			if((c >= 0x30) && (c <= 0x39))
				printf("%c",c);
			else if((c >= 65) && (c < 91))
				printf("%c",c);
			else if((c >= 97) && (c < 123))
				printf("%c",c);
			else 
				printf(".");
		}
		count = stcnt;
		data_len -= 8;
	}
	printf("\n");
}

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
//		printf("i2c %s opened\n",i2cdev_name);
		*pi2c_dev_handle = i2c_dev_handle;
		ret = 1;
	}
	else
	{
		*pi2c_dev_handle = 0;
		printf("i2c %s not opened, %s\n",i2cdev_name, strerror(errno));
	}
	return ret;
}
void close_i2c(int * pi2c_dev_handle)
{
	if(*pi2c_dev_handle > 0)
		close(*pi2c_dev_handle);
	*pi2c_dev_handle = 0;
}

int ApplyXclearSensor(int i2c_dev_handle, char i2cdev_addr)
{
	unsigned char data[5] = {0};
	eeprom_test_write(i2c_dev_handle, 0x63, 0, data, 1);
	eeprom_test_write(i2c_dev_handle, 0x65, 0, data, 1);
	eeprom_test_write(i2c_dev_handle, 0x67, 0, data, 1);
	eeprom_test_write(i2c_dev_handle, 0x6A, 0, data, 1);
	eeprom_test_write(i2c_dev_handle, 0x6B, 0, data, 1);
//	eeprom_test_write(i2c_dev_handle, 0x6C, 0, data, 1);
	return 0;
}

int InitSensorI2c(int * pi2c_dev_handle)
{
	char i2cdev_name[32] = I2C_DEV_NAME; 
	char i2cdev_addr = I2C_ADDR_IMX392; 

	unsigned char data[5] = {0};

	int i2c_dev_handle = 0;

	if(init_i2c(&i2c_dev_handle, i2cdev_name) == 0)//failed
		return 0;

	*pi2c_dev_handle = i2c_dev_handle;
	return 0;
}

int RegInitSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned int vmax = 0;
	unsigned char data[5] = {0};
	
	eeprom_read(i2c_dev_handle, i2cdev_addr, 0x3010, data, 3);// read Vmax for shutter calculation
	vmax = data[0];
	vmax |= (data[1] << 8);
	vmax |= (data[2] << 16);
	return vmax;
}
int CloseSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 

//	stop_streaming_sony_sensor(i2c_dev_handle, i2cdev_addr);
	close_i2c(&i2c_dev_handle);
	return 0;
}

int SetShutterSensorI2c(int i2c_dev_handle, int vmax, int shutterindex)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int shutter = 0;

	shutter = (IMX264Shutter[shutterindex] + (2088 - IMX264Shutter[0]));
	if(vmax > shutter)
		shutter = vmax - (2088 - shutter);
	data[2] = (unsigned char)(shutter >> 16);
	data[1] = (unsigned char)(shutter >> 8);
	data[0] = (unsigned char)shutter;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_PULSE1_START, data, 3);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_PULSE2_START, data, 3);
//	printf("Applying settings only shutter=%d\n", shutter);

	return 0;
}

int check_ShutterSensorI2c(int i2c_dev_handle, int vmax, int shutterindex)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int shutter = 0, shutter_read = 0;

	shutter = (IMX264Shutter[shutterindex] + (2088 - IMX264Shutter[0]));
	if(vmax > shutter)
		shutter = vmax - (2088 - shutter);

	eeprom_read(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);

	shutter_read = data[0];
	shutter_read |= data[1] << 8;
	shutter_read |= data[2] << 16;
//	if(shutter != shutter_read)
//		printf("Applying shutter error value = %d read = %d\n", shutter, shutter_read);
	return 0;
}

int SetGainSensorI2c(int i2c_dev_handle, int gainindex, int aperture)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int gain = 0;

	gain = IMX264AnalogGain[gainindex]+IMX264DigitalGain[aperture];
	data[1] = (unsigned char)(gain >> 8);
	data[0] = (unsigned char)gain;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_GAIN_ADDRESS, data, 2);
	return 0;
}

int SetShutterLineSensorI2c(int i2c_dev_handle, int shutterLines)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	
	
	data[0] = (unsigned char)shutterLines;
	data[1] = (unsigned char)(shutterLines >> 8);
	data[2] = (unsigned char)(shutterLines >> 16);
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
	return 0;
}

int GetShutterGainSensorI2c(int i2c_dev_handle)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int shutter = 0;
	unsigned int gain = 0;

	eeprom_read(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
	shutter = data[2] << 16;
	shutter |= data[1] << 8;
	shutter |= data[0];

	eeprom_read(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_GAIN_ADDRESS, data, 2);
	gain = data[1] << 8;
	gain |= data[0];

//	printf("Read settings gain=%d shutter=%d\n", gain, shutter);
	return 0;
}

int SetShutterGainSensorI2c(int i2c_dev_handle, int vmax, int shutterindex, int gainindex, int aperture)
{
	char i2cdev_addr = I2C_ADDR_IMX392; 
	unsigned char data[5] = {0};
	unsigned int shutter = 0;
	unsigned int gain = 0;

	shutter = (IMX264Shutter[shutterindex] + (2088 - IMX264Shutter[0]));
//	shutter = IMX264Shutter[shutterindex];
//	data[2] = (unsigned char)(shutter >> 16);
//	data[1] = (unsigned char)(shutter >> 8);
//	data[0] = (unsigned char)shutter;
//	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
	gain = IMX264AnalogGain[gainindex]+IMX264DigitalGain[aperture];
	data[1] = (unsigned char)(gain >> 8);
	data[0] = (unsigned char)gain;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_GAIN_ADDRESS, data, 2);
	if(vmax > shutter)
		shutter = vmax - (2088 - shutter);
	data[2] = (unsigned char)(shutter >> 16);
	data[1] = (unsigned char)(shutter >> 8);
	data[0] = (unsigned char)shutter;
	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_SHUTTER_SWEEP, data, 3);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_PULSE1_START, data, 3);
//	eeprom_write(i2c_dev_handle, i2cdev_addr, IMX264_I2C_REG_PULSE2_START, data, 3);
//	printf("Applying settings gain=%d shutter=%d\n", gain, shutter);

	return 0;
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



