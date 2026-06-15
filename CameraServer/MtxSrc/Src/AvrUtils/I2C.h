/********************************************************************************************/
/*		Project		:	OV78X Home Wifi Camera				    */
/*		Filename	:	I2C.c						    */
/*		Functionality	:	i2c Control routine header file			    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/
 
/********************************************************************************************/
/*                          Macros	                                                    */
/********************************************************************************************/

#define I2C_DEVICE	"/dev/i2c-1"

/*
 * Devices
 */
#define ISL1208 	0	//old rtc
#define _24LC64		1	//eeprom for configuration
#define _24LC64_WP	2	//eeprom for product details
#define OVT10633	3	//Omnivison CMOS sensor
#define	PCF2129 	4       //new rtc
#define	PCA9557 	5       //gpio expander

/*
 * I2C address of devices
 */
#define I2C_ADDR_ISL1208 	0x6F  //0xDF>>1
#define	I2C_ADDR_24LC64	 	0x50  //0xA0>>1
#define I2C_ADDR_24LC64_WP	0x54  //0xA8>>1
#define I2C_ADDR_OVT10633	0x30  //0x60>>1
#define	I2C_ADDR_PCF2129 	0x51  //0xA2>>1  
#define	I2C_ADDR_PCA9557 	0x18   //0x18>>1 
/*
 * I2C driver params
 */
//#define BYTES_PER_PAGE  32      /* one eeprom page is 256 byte */
#define MAX_BYTES       128  	/* max number of bytes to write in one piece */
#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/
#define I2C_RD		0x01
/********************************************************************************************/
/*                         Defines 		                                            */
/********************************************************************************************/
// struct i2c_message_mtx
// {
//   unsigned short addr;		// slave address			
//   unsigned short flags;		// read/ write flags			
//   unsigned short len;		// msg length				
//   unsigned char *buf;		// pointer to msg data			
// };
