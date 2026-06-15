#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>


#define I2C_DEV_NAME		"/dev/i2c-1"
#define	I2C_ADDR_LUX_SENSOR 	0x4D  
/*
 * I2C driver params
 */
//#define BYTES_PER_PAGE  32      /* one eeprom page is 256 byte */
#define MAX_BYTES       128  	/* max number of bytes to write in one piece */
#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/
#define I2C_RD		0x01

extern void display_buffer (unsigned char * source, unsigned short buffer_length);



#define MAX_LUX_VALS 8
static unsigned char luxtable[MAX_LUX_VALS];
static unsigned char tableptr = 0;
static int num_vals = 0;

unsigned char check_table(unsigned char val)
{
	static int avg = 0;
	int tempavg = 0, tempval, diffval;
	int temp_num_vals = 0, i, j;// initially
	unsigned char	ret = 0;
	unsigned char tempvals[MAX_LUX_VALS];
	if(num_vals == 0)
	{
		//populate all 
		for(i = 0; i < MAX_LUX_VALS; i++)
			luxtable[i] = val;
		ret = avg = val;
		num_vals = MAX_LUX_VALS;
	}
	else
	{
		luxtable[tableptr] = val;
		avg = 0;
		for(i = 0; i < MAX_LUX_VALS; i++)
			avg+=luxtable[i];
		tempval = (int)(avg/MAX_LUX_VALS);
		ret = tempval;
		tableptr++;
		if(tableptr >= MAX_LUX_VALS)
			tableptr = 0;
	}
	return(ret);
}

/*
unsigned char check_table(unsigned char val)
{
	static int avg = 0;
	int tempavg = 0, tempval, diffval;
	int temp_num_vals = 0, i, j;// initially
	unsigned char	ret = 0;
	unsigned char tempvals[MAX_LUX_VALS];
	if(num_vals == 0)
	{
		//populate all 
		for(i = 0; i < MAX_LUX_VALS; i++)
			luxtable[i] = val;
		ret = avg = val;
		num_vals = MAX_LUX_VALS;
	}
	else
	{
		tempval = val;
		luxtable[tableptr] = val;
		diffval = 0;
		if(tempval > avg)
		{
			// going high..
			diffval = tempval-avg;
		}
		else if(tempval < avg)
		{
			// going down
			diffval = avg - tempval;
		}
		if(diffval)
		{
			if((diffval > 0) && (diffval < 4)) 
			{
				if(num_vals < 16)
				{
					num_vals+=8;
				}
				else if(num_vals < 24)
				{
					num_vals+=4;
				}
				else if(num_vals < 30)
				{
					num_vals+=2;
				}
				else
					num_vals = 32;
			}
			else if((diffval >= 4) && (diffval < 8)) 
			{
				if(num_vals < 8)
				{
					num_vals+=6;
				}
				else if(num_vals < 16)
				{
					num_vals+=4;
				}
				else if(num_vals < 20)
				{
					num_vals+=2;
				}
				else
					num_vals = 24;
			}
			else if((diffval >= 8) && (diffval < 16)) 
			{
				if(num_vals < 4)
				{
					num_vals+=4;
				}
				else if(num_vals < 10)
				{
					num_vals+=3;
				}
				else if(num_vals < 14)
				{
					num_vals+=2;
				}
				else
					num_vals = 16;
			}
			else if((diffval >= 16) && (diffval < 24)) 
			{
				if(num_vals < 2)
				{
					num_vals+=3;
				}
				else if(num_vals < 4)
				{
					num_vals+=2;
				}
				else if(num_vals < 6)
				{
					num_vals++;
				}
				else
					num_vals = 8;
			}
			else if((diffval >= 24) && (diffval < 48)) 
			{
				if(num_vals < 2)
				{
					num_vals++;
				}
				else
					num_vals = 4;
			}
			else if((diffval >= 48) && (diffval < 96)) 
			{
				num_vals = 2;
			}
			else if((diffval >= 96) && (diffval < 200)) 
			{
				num_vals = 1;
			}
			else if(num_vals < MAX_LUX_VALS)
				num_vals++;
			avg = 0;
			for(i = 0; i < MAX_LUX_VALS; i++)
				avg+=luxtable[i];
			avg = (int)(avg/MAX_LUX_VALS);
			tempavg = 0;
			for(i = 0, j = tableptr; i < num_vals; i++)
			{
				tempavg+=luxtable[j];
				if(j == 0)
					j = MAX_LUX_VALS;
				j--;
			}
			tempavg = (int)(avg/num_vals);
			ret = tempavg;
		}
		else
		{
			ret = val;
		}
		tableptr++;
		if(tableptr >= MAX_LUX_VALS)
			tableptr = 0;
	}
	return(ret);
}
*/

static int leeprom_read(int fd, unsigned int slave_addr, unsigned int offset,unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;  
  int i;

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

int InitLuxADCI2c(int * pi2c_dev_handle)
{
	char i2cdev_name[32] = I2C_DEV_NAME; 
	char i2cdev_addr = I2C_ADDR_LUX_SENSOR; 
	int ret = 0;
	int i2c_dev_handle = 0;

//	printf("probing i2c device %x on bus %s\n", i2cdev_addr, i2cdev_name);
	i2c_dev_handle = open(i2cdev_name, O_RDWR );
	if(i2c_dev_handle > 0)
	{
//		printf("i2c %s opened\n",i2cdev_name);
		*pi2c_dev_handle = i2c_dev_handle;
		ret = 1;
	}
	else
	{
		*pi2c_dev_handle = 0;
		ret = 0;
	}
	return ret;
}
void CloseLuxADCI2c(int * pi2c_dev_handle)
{
	if(*pi2c_dev_handle > 0)
		close(*pi2c_dev_handle);
	*pi2c_dev_handle = 0;
}

int GetLuxValue(int i2c_dev_handle, int* lux)
{
	char i2cdev_name[32] = I2C_DEV_NAME; 
	char i2cdev_addr = I2C_ADDR_LUX_SENSOR; 

	unsigned char data[5] = {0};
	unsigned int shutter = 0;
	unsigned char val = 0;

	if(i2c_dev_handle <= 0)
		return 0;

//	if(InitLuxADCI2c(&i2c_dev_handle) == 0)//failed
//		return 0;
//	printf("reading i2c device %x on bus %s\n", i2cdev_addr, i2cdev_name);
	leeprom_read(i2c_dev_handle, i2cdev_addr, 0, data, 2);

	val = data[0] << 4;
	val |= data[1] >> 4;
//	printf("read data 0x%02x%02x val = %x = %d\n", data[0], data[1], val, val);

	*lux = check_table(val);
//	*lux = val;
//	printf("LUX Val = %d avg = %d\n",val, *lux);
//	display_buffer(luxtable, 16);
//	CloseLuxADCI2c(&i2c_dev_handle);
	return 0;
}



