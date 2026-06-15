/********************************************************************************************/
/*		Project		:	OV78X Home Wifi Camera				    */
/*		Filename	:	I2C.c						    */
/*		Functionality	:	i2c Control routine				    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#include <stdio.h>
#include <errno.h>      
#include <fcntl.h>
#include <stdlib.h>      
#include <strings.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/fcntl.h> 
#include <sys/stat.h>
#include <sys/ioctl.h> 
#include "I2C.h" 
/********************************************************************************************/
/*                          Defines 		                                            */
/********************************************************************************************/
//-L/home/mtx/Maheen/ -li2c

//-L/home/mtx/Maheen/ -li2c
/********************************************************************************************/
/*                          Extern Prototypes                                               */
/********************************************************************************************/
//https://github.com/amaork/libi2c
/********************************************************************************************/
/*                          Variables 		                                            */
/********************************************************************************************/
static int I2CFd;
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/

/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/
void ConfigureI2C(void)
{
	if((I2CFd=open(I2C_DEVICE,O_RDWR | O_SYNC))<0)
	{
		printf("Could not open i2c at %s\n",I2C_DEVICE);
		//perror(I2C_DEVICE);
	}  
}

void I2cUnInit(void)
{
	close(I2CFd);
}

int I2CWrite(unsigned char device, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg i2cmsg;
	int i=0, j=0;
	unsigned char _buf[MAX_BYTES];

	switch(device)
	{
		case _24LC64:
		case _24LC64_WP:
				i = 0;
				_buf[i++] = offset >> 8;//_buf[0] is the offset addr(10-8)into the eeprom page!
				_buf[i++] = offset;    	// _buf[1] is the offset addr(7-0) into the eeprom page!
				len += 2;
				break;   
		case ISL1208:
		case PCF2129:
		case PCA9557:
				i = 0;
				_buf[i++] = offset;    //_buf[0] is the offset addr!
				len += 1;
				break;   
		case OVT10633:
				i = 0;
				_buf[i++] = offset >> 8;//_buf[0] is the offset addr(10-8)into the eeprom page!
				_buf[i++] = offset;    	// _buf[1] is the offset addr(7-0) into the eeprom page!
				len += 2;
				break;   
	}

	for(j=0;i<len;i++) // i presetted, copy buf[0..n] -> _buf[1..n+1] 
	{
		_buf[i]=buf[j++];
	}

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = slave_addr;
	i2cmsg.flags = 0;
	i2cmsg.len   = len;
	i2cmsg.buf   = _buf;

again:
	if((i=ioctl(I2CFd,I2C_RDWR,&msg_rdwr))<0)
	{
		if(i == -1&& (errno==EINTR||errno==121))
		{
			goto again;
		}
		else
		{
			printf("retval=%d:errno=%d:i2c Data Write Error\n",i,errno);
			return -1;
		}
	}  
	return 0;
}


static int I2CAddrWrite(unsigned char device, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg i2cmsg;
	int i=0, j=0;  
	unsigned char _buf[MAX_BYTES];

	switch(device)
	{
		case _24LC64:
		case _24LC64_WP:
				i = 0;
				_buf[i++] = offset >> 8;//_buf[0] is the offset addr(10-8)into the eeprom page!
				_buf[i++] = offset;    	// _buf[1] is the offset addr(7-0) into the eeprom page! 
				len += 2;
				break;
		case ISL1208:
		case PCF2129:
		case PCA9557:
				i = 0;
				_buf[i++] = offset;    //_buf[0] is the offset addr!
				len += 1;
				break;
		case OVT10633:      
				i = 0;
				_buf[i++] = offset >> 8;//_buf[0] is the offset addr(10-8)into the eeprom page!
				_buf[i++] = offset;    	// _buf[1] is the offset addr(7-0) into the eeprom page! 
				len += 2;
				break;
	}

	for(j=0;i<len;i++) // i presetted, copy buf[0..n] -> _buf[1..n+1] 
	{
		_buf[i]=buf[j++];
	}

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = slave_addr;
	i2cmsg.flags = 0;
	i2cmsg.len   = len;
	i2cmsg.buf   = _buf;

again:
	if((i=ioctl(I2CFd,I2C_RDWR,&msg_rdwr))<0)
	{
		if(i == -1&& (errno==EINTR||errno==121))
		{
			goto again;
		}
		else
		{
			printf("retval=%d:errno=%d:i2c Addr Error\n",i,errno);
			return -1;
		}
	}
	return 0;
}

int I2CRead(unsigned char device, unsigned int slave_addr, unsigned int offset,unsigned char *buf, unsigned char len)
{
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg i2cmsg;  
	int i;

	// first write the addr to the slave device 
	if(I2CAddrWrite(device,slave_addr,offset,NULL,0)<0)
	{
		return -1;
	}

	msg_rdwr.msgs = &i2cmsg;  
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = slave_addr;
	i2cmsg.flags = I2C_RD;
	i2cmsg.len   = len;
	i2cmsg.buf   = buf;

again:
	if((i=ioctl(I2CFd,I2C_RDWR,&msg_rdwr))<0)
	{
		if(i == -1&& (errno==EINTR||errno==121))
		{
			goto again;
		}
		else
		{
			printf("retval=%d:errno=%d:i2c Read Error\n",i,errno);
			return -1;
		}
	}  
	return 0;
}



