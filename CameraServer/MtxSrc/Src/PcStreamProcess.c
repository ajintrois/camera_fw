/********************************************************************************************/
/*		Project		:	TK1 Image Analytics				    */
/*		Filename	:	PcStreamProcess.c				    */
/*		Functionality	:	Pc image Stream Tx Processing and config Server app */
/*		Author		:	Manoj Kumar D					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h> 
#include <pthread.h>
#include <linux/unistd.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/statvfs.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <ctype.h>

#include "defines.h"
#include "common_shm.h"
#include "eeprom.h"
#include "avsDefines.h"

/********************************************************************************************/
/*                          Defines                                                         */
/********************************************************************************************/
#define APP_NAME_USR		"/usr/Camera/firmware.gz"
#define APP_NAME_USR_BKP	"/usr/Camera/firmwareBKUP.gz"
#define APP_NAME_USR_SIG	"/usr/Camera/firmware.sig"
#define APP_NAME	"/run/trois/bin/flash/firmware1.gz"
#define APP_NAME_SIG	"/run/trois/bin/flash/firmware1.sig"
#define APP_NAME_ENC	"/run/trois/bin/flash/firmware1.gz.enc"
#define MAXINPUTSIZE 	(1024*1024*8)

#define GCM_KEY_SIZE 32   // 256 bits
#define GCM_IV_SIZE  12   // Recommended for GCM
#define GCM_TAG_SIZE 16   // 128-bit authentication tag
#define GCM_MAX_ENC_BLOCK_SIZE (128*1024)
#define GCM_MAX_ENC_BLOCK_SIZE_ENC (128*1024 + 4096)

#define MAXREMOTE_RX_DATA_FIFO_LENGTH	(32*1024)
#define MAXREMOTE_TX_DATA_FIFO_LENGTH	(1024*1024)

#define MAX_TX_DATA_BUFFER		(1 << 20)
#define MAX_RX_DATA_BUFFER		(16*1024)

#define REMOTE_KEEPALIVETIMEOUT		18

#define MAX_SOCKET_DATA_TR		SOCKET_SEND_BUFF_LENGTH
#define MAX_TX_DATA_SIZE_MASK		((MAX_TX_DATA_BUFFER-1) ^ (MAX_SOCKET_DATA_TR-1))

#define ETH_CABLE_CONNECTION	"//sys//class//net//enP8p1s0//carrier"

#define ETH_NETWORK_SPEED	"//sys//class//net//enP8p1s0//speed"

#define MAX_MTX_CIPHER_SIZE (1024*128)
#define MTX_CIPHER_ALLIGNMENT (0x00007F)

#define FILENAME "/sys/bus/nvmem/devices/fuse/nvmem"

typedef struct packvar_t{
	char	Prefix[16];
	int	Model;
	int	Type;
	int 	MVer;
	int	mVer;
	int	Patch;
	char	extn[6];
} T_PACKVARS;

struct IPCONFIGDATA {
unsigned char ip[4];
unsigned char sm[4];
unsigned char gw[4];
unsigned char ds[4];
};

/********************************************************************************************/
/*                          Global Vars                                                     */
/********************************************************************************************/
char			FirmwareV2[32] = {"--NOT_CURRENT_VERSION--"};
static unsigned int 	expected_len[NO_OF_SERVER_SOCKETS] = {0};
static unsigned char 	rx_in_progress[NO_OF_SERVER_SOCKETS] = {0};

static unsigned int	client_dev_type[NO_OF_SERVER_SOCKETS] = {0};
static unsigned int	systemPGMMode = 99;// 99=no user in Config mode(normal) 0-no of sockets user in that mode

static unsigned int	SecondsSinceLastRestart = 0;
static int		verifyEepromAll = 0;
static int		verifyEepromLuxTable = 0;
static int		connected_users;
static int		saveEepromAll = 0;
static int		saveEepromLuxTable = 0;

static int		event_status[NO_OF_SERVER_SOCKETS][3] = {0};
static unsigned char	eventdatauser[NO_OF_SERVER_SOCKETS][3][48];
static int		tempLuxTable = 0;
int 			EEoffset=0;
static int		VPUConnected = 0;

static unsigned short	FWversion = 0;
static unsigned short	fsfreesize = 0;
static unsigned short	NetworkSpeed = 0;
static unsigned	int	comkey_ok[NO_OF_SERVER_SOCKETS] = {0};
/********************************************************************************************/
/*                          Extern Vars                                                     */
/********************************************************************************************/
extern unsigned short	keepalive_timer[MAX_WDT_COUNT], keepalive_timer_enabled[MAX_WDT_COUNT], keepalive_timer_reload[MAX_WDT_COUNT], aux_keep_alive_timer[MAX_WDT_COUNT];
extern int 		rtcMtimer, socktimer[NO_OF_SERVER_SOCKETS];
extern int		wdt_rd_pipe, wdt_wr_pipe;
extern unsigned int	process_timer[];
extern unsigned char	Aeskey[32], DecIV[16];
extern struct cirfifo	*ProcessDebugFifo;
//extern char 		DebugStr[1024];
//extern int 		DebugStrSize;
extern char 		firmware[];
extern int		processIDVar, displaymsg_cnt;

extern int		lux, prevLux, prev_ampm_flag, ampm_flag, crnt_table;

extern struct gnrl 	general_details,new_general_details;
extern struct cam 	camera_parameters[4],new_camera_parameters[4];
extern struct ip 	ip_details,new_ip_details;
extern struct remo 	remote_user[MAX_REMOTE_USER],new_remote_user[MAX_REMOTE_USER],blank_remote_user[MAX_REMOTE_USER];
extern struct nocs 	no_checksum_data,new_no_checksum_data;
extern struct chsm 	checksum,new_checksum;
extern struct wrpr 	write_protected_data;
extern struct sch 	schedule[MAX_SCH_TIMING][MAX_SCH_CHANGES],new_schedule[MAX_SCH_TIMING][MAX_SCH_CHANGES];
extern struct alrm 	alarm_data,new_alarm_data;
extern struct wb 	wb_details,new_wb_details;
extern struct dydns 	dydns_data,new_dydns_data;
extern struct womensafety_cam womensafety_camera_parameters[65],new_womensafety_camera_parameters[65];

extern struct rtsp_stream rtsp_details,new_rtsp_details;
extern struct eeprom_verify ev;

extern int 		battery_low;
extern int 		power_loss;
extern int 		configWrite_trigger;// 0=no trigger, 1= timer run
extern int 		configWrite_timer; // when trigger=1 countdown starts and writes when expires.

// for pc..
struct gnrl_avs		general_details_avs;
struct cam_avs 		camera_parameters_avs[4];
struct ip_avs 		ip_details_avs;
struct remo_avs 	remote_user_avs[16];
struct nocs_avs 	no_checksum_data_avs;
struct chsm_avs 	checksum_avs;
struct wrpr_avs 	write_protected_data_avs;
struct sch_avs  	schedule_avs[9][8];
struct alrm_avs 	alarm_data_avs;
struct dydns_avs 	dydns_data_avs;

/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
extern void ProcessDebugFifoDataWrite(char * buff,unsigned long size);
extern void getSharedConfigData(void * data);
extern void verify_womensafety_fram (void);
extern void verify_fram (void);
extern void default_ip_details(void);
extern void verify_write_protected_data(void);
extern void save_womensafety_programmed_data(void);
extern void save_camera_parameters(void);
extern void verify_camera_parameters (void);
extern int initI2Ceeprom();
extern void save_programmed_data(void);
extern void SendSerialDataNTXD0(int serial4_handle, char * data, int count);
extern void init_rtc();
extern void close_rtc();
extern void get_rtc_time( volatile unsigned char *ptimecode);
extern void set_rtc_time( volatile unsigned char *ptimecode);
extern void *serialPort4RxAccess(void *tdata);
extern void saveconfigdataenc(void);
extern void Config_rebuild();
extern void NanoReset(int pipefd);
extern int DeadDelayMS(int ms);
extern int DeadDelayUS(int us);
extern void loganevent(const char *processname, const char *eventstr);

extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);
extern int safe_atoi(const char *nptr, int *value);
extern char* safe_strncpy( char* dest, const char* src, size_t count);
extern size_t safe_strlen(const char *str, size_t max_len);
extern int mtx_sc_anf(const char *str, const char *format, ...);

int  getCableStatus(void);
int  server_tx(struct tst *tst);
int  server_rx(struct tst *tst);
void process_tx(struct tst *tst);
void process_rx(struct tst *tst);
void copy_status_flags_n_send(unsigned int tx_size, struct tst *tst);
void update_view_list(struct tst *tst);
int safe_bkupNcopy_fw(void);
int safe_signwith_cskey(unsigned char *pin);

void* threadedSocketCommunicationThread(void * arg);
void read_mac_address(unsigned char *mac_address);
unsigned short getNetSpeed(void);
/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/

int validate_ver(T_PACKVARS old, T_PACKVARS newn)// returns 0 if ok else error no. 1=No Prefix, 2=Mode/Type err, 3=No ext, 4=old ver
{
	int oldv, newv;
	if(strncmp(newn.Prefix,"FWCV", 4))// returns 1 if fails..
		return 1;
	if((old.Model != newn.Model) || (old.Type != newn.Type))
		return 2;
	if(strncmp(old.extn, newn.extn, 3))
		return 3;
	if(strncmp(newn.extn,"bin", 3))// returns 1 if fails..
		return 3;
	oldv = (((old.MVer & 0x000003ff) << 10) | ((old.mVer & 0x0000003f) << 4) | (old.Patch & 0x000000f));
	newv = (((newn.MVer & 0x000003ff) << 10) | ((newn.mVer & 0x0000003f) << 4) | (newn.Patch & 0x000000f));
//	printf("version Old = %8d, %6x\n", oldv, oldv);
//	printf("version New = %8d, %6x\n", newv, newv);
	if( newv > oldv)
		return 0;// success...
	else
		return 4;
}
static void display_buffer (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j;
	char c;

	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		for(j = 0; j < 8; j++)
			printf(" %02x",(unsigned char) *(source+j+i*8));
		printf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
	}
}


/*==================================================================================================*/
// Function to decrypt AES-GCM in file mode
int decrypt_aes_gcm_embtifile(char *cryptfilename, 
			char 	*outfilename,
			unsigned char 	*key)
{
	EVP_CIPHER_CTX *ctx;
	FILE *crfile, *outfile;
	unsigned char inbuff[GCM_MAX_ENC_BLOCK_SIZE_ENC], outbuff[GCM_MAX_ENC_BLOCK_SIZE];
	const char head[6] = "ZRTI";
	unsigned char iv[GCM_IV_SIZE];
	unsigned char tag[GCM_TAG_SIZE];
	unsigned char header[32];
	int len;
	int crlen;
	int crsize;
	int ret;
	
	// init.
	ret = -1;
	crfile = fopen(cryptfilename, "rb");
	if(crfile != NULL)
	{
		len = fread(header, 1, 32, crfile);
		if((header[0] == head[0]) && (header[1] == head[1]) && (header[2] == head[2]) && (header[3] == head[3]))
		{
			outfile = fopen(outfilename, "wb");
			if(outfile != NULL)
			{
				const char *aad = "Trois_AppEnc.Tag"; 
				int aad_len = 16;
				mempcpy(tag, &header[4], GCM_TAG_SIZE);
				mempcpy(iv, &header[4+GCM_TAG_SIZE], GCM_IV_SIZE);
				// Create and initialize the context
				if (!(ctx = EVP_CIPHER_CTX_new()))
				{
					printf("EVP_CIPHER_CTX_new failed");
					goto cleanup_dec;
				}
				// Initialize decryption operation with AES-128-GCM
				if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
				{
					printf("EVP_DecryptInit_ex failed");
					EVP_CIPHER_CTX_free(ctx);
					goto cleanup_dec;
				}
				// Set IV length if default 12 bytes is not used
				if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, NULL))
				{
					printf("EVP_CIPHER_CTX_ctrl SET_IVLEN failed");
					EVP_CIPHER_CTX_free(ctx);
					goto cleanup_dec;
				}
				// Initialize key and IV
				if (1 != EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) 
				{
					printf("EVP_DecryptInit_ex key/iv failed");
					EVP_CIPHER_CTX_free(ctx);
					goto cleanup_dec;
				}
				// Provide AAD data
				if (1 != EVP_DecryptUpdate(ctx, NULL, &len, (unsigned char *)aad, aad_len)) 
				{
					printf("EVP_DecryptUpdate AAD failed");
					EVP_CIPHER_CTX_free(ctx);
					goto cleanup_dec;
				}
				ret = 0;
				crsize = GCM_MAX_ENC_BLOCK_SIZE_ENC;
				while(crsize > 0)
				{
					crlen = fread(inbuff, 1, crsize, crfile);
					if(crlen < crsize)
						crsize = 0;// last block..
					len = 0;
					if(crlen > 0)
					{
						if (1 != EVP_DecryptUpdate(ctx, outbuff, &len, inbuff, crlen))
						{
							printf("EVP_DecryptUpdate ciphertext failed");
							EVP_CIPHER_CTX_free(ctx);
							ret = -1;
							break;
						}
						ret += len;
						fwrite(outbuff, len, 1, outfile);
					}
					//printf("dec: size=%d, len=%d, out= %d size= %d\n", crsize, crlen, len, ret);
				}
				if(ret < 0)
					goto cleanup_dec;
				if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, tag)) 
				{
					printf("EVP_CIPHER_CTX_ctrl SET_TAG failed");
					EVP_CIPHER_CTX_free(ctx);
					goto cleanup_dec;
				}

				len = 0;
				// Finalize decryption
				crsize = EVP_DecryptFinal_ex(ctx, outbuff, &len);
				if(len > 0)
				{
					fwrite(outbuff, len, 1, outfile);
					ret += len;
				}
				// Free decryption context
				EVP_CIPHER_CTX_free(ctx);
				if(crsize <= 0)
				{
					printf("EVP_DecryptFinal_ex failed");
					ret = crsize;
				}
				printf("finished = %d\n", ret);
		
				fclose(outfile);
			}
		}
		fclose(crfile);
	}
cleanup_dec:
	return ret;
}	

/*==================================================================================================*/
int verify_signature(const unsigned char *message, size_t message_len,
                     const unsigned char *signature, size_t signature_len,
                     EVP_PKEY *public_key) {
	EVP_MD_CTX *mdctx = NULL;
	int result = 0;

	// Create and initialize the digest verification context
	mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) 
	{
		printf("Error creating EVP_MD_CTX\n");
		return 0;
	}

	// Initialize the verification operation
	if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, public_key) <= 0) 
	{
		printf("Error initializing DigestVerify\n");
		goto cleanup;
	}

	// Provide the message to be verified
	if (EVP_VerifyUpdate(mdctx, message, message_len) <= 0) 
	{
		printf("Error updating DigestVerify\n");
		goto cleanup;
	}

	// Verify the signature
	result = EVP_DigestVerifyFinal(mdctx, signature, signature_len);
	if (result == 1) 
	{
		printf("Signature is valid.\n");
	} 
	else if (result == 0) 
	{
		printf("Signature is invalid.\n");
	} 
	else 
	{
		printf("Error during DigestVerifyFinal\n");
	}

cleanup:
	EVP_MD_CTX_free(mdctx);
	return result;
}

/*==================================================================================================*/

int verifyIPSettings(struct IPCONFIGDATA * ipdata)// return 0 on sucess, 1 on error, 2 on change(req restart)
{
/*
sudo cat /etc/NetworkManager/system-connections/Wired\ connection\ 1.nmconnection 

[connection]
id=Wired connection 1
uuid=ae8b93d0-b373-340b-8469-38594863644c
type=ethernet
autoconnect-priority=-999
interface-name=eth0
permissions=
timestamp=1704188361

[ethernet]
mac-address-blacklist=

[ipv4]
address1=192.168.3.158/20,192.168.1.1
dns=8.8.8.8;192.168.1.254;
dns-search=
ignore-auto-dns=true
method=manual

[ipv6]
addr-gen-mode=stable-privacy
dns-search=
method=auto

[proxy]

orin Jetpack 6.2
---------------------------------------------------------------------
[connection]
id=Wired connection 1
uuid=e23484d3-7909-3a5d-bb67-2592fb3f683b
type=ethernet
autoconnect-priority=-999
interface-name=enP8p1s0

[ethernet]

[ipv4]
address1=192.168.3.159/20,192.168.1.1
dns=8.8.8.8;192.168.1.1;
method=manual

[ipv6]
addr-gen-mode=stable-privacy
method=auto

[proxy]



*/
	char filebuffer[1024], newfilebuffer[1024]={0}, databuffer[1024], *strstart;
	int  i=0, dataerror = 0;
	unsigned char maskval = 0, maskchk[32] = {   0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01,
						0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01,
						0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01,
						0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
	FILE *fptr;

	for(maskval = 0; maskval < 32; maskval++)
	{
		if(maskval < 8)
		{
			if((ipdata->sm[0] & maskchk[maskval]) == 0)
				break;
		}
		else if(maskval < 16)
		{
			if((ipdata->sm[1] & maskchk[maskval]) == 0)
				break;
		}
		else if(maskval < 24)
		{
			if((ipdata->sm[2] & maskchk[maskval]) == 0)
				break;
		}
		else
		{
			if((ipdata->sm[3] & maskchk[maskval]) == 0)
				break;
		}
	}

	fptr = fopen("/etc/NetworkManager/system-connections/Wired connection 1.nmconnection", "r");
	if(fptr == NULL)
	{
		dataerror = 4;
		
	}
	else
	{
		//ok continue
		i = fread(filebuffer, 1, 1024, fptr);
		fclose(fptr);
		if(i <= 0)
			return 1;
		// else ok continue
//		printf("nmconnection file read %d bytes\n", i);
		filebuffer[i] = 0;
		printf("ip file \n%s\n", filebuffer);
		//display_buffer((unsigned char*)filebuffer, 256);
		
		snprintf(databuffer, 45, "address1=%d.%d.%d.%d/%d,%d.%d.%d.%d", ipdata->ip[0], ipdata->ip[1], ipdata->ip[2], ipdata->ip[3], maskval, ipdata->gw[0], ipdata->gw[1], ipdata->gw[2], ipdata->gw[3]);
		//if( <= 0)
		//	printf("ipaddress not configured. old /default persists\n");
//		printf("ipaddr check \n%s\n", databuffer);
		strstart = NULL;
		strstart = strstr(filebuffer, databuffer);
		if(strstart == NULL)// error
		{
			dataerror = 1;
		}
		snprintf(databuffer, 21, "dns=%d.%d.%d.%d", ipdata->ds[0], ipdata->ds[1], ipdata->ds[2], ipdata->ds[3]);
		//if(safe_ <= 0)
		//	printf("dns not set. old/default persists");
//		printf("dns check \n%s\n", databuffer);
		strstart = NULL;
		strstart = strstr(filebuffer, databuffer);
		if(strstart == NULL)// error
		{
			dataerror |= 2;
		}
		
	}
//	printf("ip ckeck %s error\n", (dataerror==1)?"ip or subnet or gateway":(dataerror==2)?"dns":(dataerror==3)?"all":(dataerror==4)?"FILE": "no");
	if(dataerror == 4)
	{

		snprintf(newfilebuffer, 400, "[connection]\nid=Wired connection 1\nuuid=e23484d3-7909-3a5d-bb67-2592fb3f683b\ntype=ethernet\nautoconnect-priority=-999\ninterface-name=enP8p1s0\n\n[ethernet]\nmac-address-blacklist=\n\n[ipv4]\naddress1=%d.%d.%d.%d/%d,%d.%d.%d.%d\ndns=%d.%d.%d.%d;\nmethod=manual\n\n[ipv6]\naddr-gen-mode=stable-privacy\ndns-search=\nmethod=auto\n\n[proxy]\n", 
					ipdata->ip[0], ipdata->ip[1], ipdata->ip[2], ipdata->ip[3], maskval, 
					ipdata->gw[0], ipdata->gw[1], ipdata->gw[2], ipdata->gw[3], 
					ipdata->ds[0], ipdata->ds[1], ipdata->ds[2], ipdata->ds[3]);
/*		if(safe _snprintf(newfilebuffer, 400, "[connection]\nid=Wired connection 1\nuuid=e23484d3-7909-3a5d-bb67-2592fb3f683b\ntype=ethernet\nautoconnect-priority=-999\ninterface-name=enP8p1s0\n\n[ethernet]\nmac-address-blacklist=\n\n[ipv4]\naddress1=%d.%d.%d.%d/%d,%d.%d.%d.%d\ndns=%d.%d.%d.%d;\nmethod=manual\n\n[ipv6]\naddr-gen-mode=stable-privacy\ndns-search=\nmethod=auto\n\n[proxy]\n", 
					ipdata->ip[0], ipdata->ip[1], ipdata->ip[2], ipdata->ip[3], maskval, 
					ipdata->gw[0], ipdata->gw[1], ipdata->gw[2], ipdata->gw[3], 
					ipdata->ds[0], ipdata->ds[1], ipdata->ds[2], ipdata->ds[3]) <= 0)
			printf(" new config not set. old /default persists\n");*/
//		printf("new ip file \n%s\n", newfilebuffer);
//display_buffer((unsigned char*)newfilebuffer, 164);
		i = safe_strlen(newfilebuffer, 400);
		fptr = fopen("/etc/NetworkManager/system-connections/Wired connection 1.nmconnection", "w");
		if(fptr == NULL)
			return 1;
		fwrite(newfilebuffer, 1, i, fptr);
		fclose(fptr);
		sync();
		usleep(100000);
		//sp rintf(buffer,"%s","(nmcli con down id \"Wired connection 1\" && nmcli con up id \"Wired connection 1\")&");
		return 2;
	}
	else if(dataerror > 0)
	{
		fptr = fopen("/etc/NetworkManager/system-connections/Wired connection 1.nmconnection", "r");
		if(fptr != NULL)
		{
			//ok continue
			i = fread(filebuffer, 1, 1024, fptr);
			fclose(fptr);
			filebuffer[i] = 0;
		}
		snprintf(databuffer, 8, "[ipv4]");
//		if(safe _snprintf(databuffer, 8, "[ipv4]") <=0)
//			printf(" load ip check not ok. ");
		strstart = NULL;
		strstart = strstr(filebuffer, databuffer);
		if(strstart != NULL)// error
		{
			if(snprintf(databuffer, 86, "[ipv4]\naddress1=%d.%d.%d.%d/%d,%d.%d.%d.%d\ndns=%d.%d.%d.%d;\nmethod=manual\n\n", 
						ipdata->ip[0], ipdata->ip[1], ipdata->ip[2], ipdata->ip[3], maskval, 
						ipdata->gw[0], ipdata->gw[1], ipdata->gw[2], ipdata->gw[3], 
						ipdata->ds[0], ipdata->ds[1], ipdata->ds[2], ipdata->ds[3]) <= 0)
				printf(" new config not set. old /default persists\n");
/*			if(safe _snprintf(databuffer, 86, "[ipv4]\naddress1=%d.%d.%d.%d/%d,%d.%d.%d.%d\ndns=%d.%d.%d.%d;\nmethod=manual\n\n", 
						ipdata->ip[0], ipdata->ip[1], ipdata->ip[2], ipdata->ip[3], maskval, 
						ipdata->gw[0], ipdata->gw[1], ipdata->gw[2], ipdata->gw[3], 
						ipdata->ds[0], ipdata->ds[1], ipdata->ds[2], ipdata->ds[3]) <= 0)
				printf(" new config not set. old /default persists\n");*/
			if(safe_strncpy(strstart, databuffer, 86) == NULL)
				printf(" Ip not corrected old ip will remain\n");
		}
		i = safe_strlen(filebuffer, 1024);
		fptr = fopen("/etc/NetworkManager/system-connections/Wired connection 1.nmconnection", "w");
		if(fptr == NULL)
			return 1;
		fwrite(filebuffer, 1, i, fptr);
		fclose(fptr);
		sync();
		usleep(100000);
		return 2;
	}
	else
		return 0;
}
void PcStreamProcessFunction(SHARED_RESOURCES *shared_data, COMPRESS_PC_SHARED_RESOURCES *compressPcShr, int *pipes)
{
	int 			i, j, sock_handle, socket_handle, pipefd;
//	unsigned int		jp, ip;
	static pthread_t 	sock_thread[NO_OF_SERVER_SOCKETS], serialThrHandle;
	static struct tst 	sock_thread_data[NO_OF_SERVER_SOCKETS];
	struct sockaddr_in 	socket_addr, client_socket_addr;
	socklen_t		addr_len;
	int			attempt = 0, cableStatus = 0, msecond_count = 0;
	char			ipaddrs[18] = {"127.0.0.0"};
	//char			ipaddrs[18] = {"0.0.0.0"};
	char 			port_no[8], c;
	SHARED_CONFIG_DATA 	*sharedConfigData;
	struct IPCONFIGDATA 	ipconfigdata;
	struct timeval 		tval, tvalrtc;
	struct tm		brokentime, brokentimertc;
	int 			i2cret=0;
	int			IcrControl = 0x100;
	unsigned int		seccount, timeinMsec;
	char 			DebugStr[1024];
	int 			DebugStrSize;
	struct statvfs		fsstat;
	FILE *			fptr;
	char 			buffer[8];
	int			carrier_status = 1;// preent...
	// initialize all socket related parameters
	fptr = fopen("../version.txt","rb");
	if(fptr!=NULL)
	{
		fread(FirmwareV2,1, 32, fptr);
		FirmwareV2[31] = 0;
		fclose(fptr);

		buffer[0] = FirmwareV2[5];
		buffer[1] = FirmwareV2[6];
		buffer[2] = FirmwareV2[7];
		buffer[3] = 0;
		if(safe_atoi((const char *)&buffer, &i2cret) > 0)
			FWversion = i2cret;
		else
			FWversion = 0;
	}
	i2cret = 0;
	pipefd = pipes[NO_OF_SERVER_SOCKETS];
	
	init_rtc();
	
	//-------------------Maheen---------------------
	if(power_loss==1)
	{
		shared_data->time_error = 1;
	}
	else
	{
		shared_data->time_error = 0;
	}	
	//shared_data->time_error = 0;
	get_rtc_time(shared_data->rtctimecode);
//	DEBUG_PRINT("PCStream:Read Time %d:%d:%d %d-%d-%d\n", shared_data->rtctimecode[2], shared_data->rtctimecode[1], shared_data->rtctimecode[0], shared_data->rtctimecode[3], shared_data->rtctimecode[4]+1, shared_data->rtctimecode[5]+1900);
	//-------------------Maheen---------------------
	
		
/*	init_rtc();
	get_rtc_time(shared_data->rtctimecode);
	DEBUG_PRINT("read rtc %d:%d:%d %d-%d-%d\n", shared_data->rtctimecode[2], shared_data->rtctimecode[1], shared_data->rtctimecode[0], shared_data->rtctimecode[3], shared_data->rtctimecode[4]+1, shared_data->rtctimecode[5]+1900);

	brokentime.tm_sec = shared_data->rtctimecode[0];//seconds
	brokentime.tm_min = shared_data->rtctimecode[1];//minutes
	brokentime.tm_hour= shared_data->rtctimecode[2];//hour
	brokentime.tm_mday= shared_data->rtctimecode[3];//date
	brokentime.tm_mon = shared_data->rtctimecode[4];//month
	brokentime.tm_year= shared_data->rtctimecode[5];//year
	tvalrtc.tv_sec = mktime(&brokentime);
	tvalrtc.tv_usec = 0;
//		tval.tv_sec += ((5*60)+30)*60;// if not on utc, for utc to local time. subtract .5Hrand 30mins.
	if(settimeofday(&tvalrtc, NULL) != 0)
		DEBUG_PRINT("set time error\n");*/
	i2cret=initI2Ceeprom();
	if(i2cret<0)
	{
		printf("**************EEPROM Init Failed ************\n");
		return;
	}
	verify_write_protected_data();
	read_mac_address(&write_protected_data.mac_address[0]);
	verify_fram();
//	verify_womensafety_fram();
	getSharedConfigData((void *)shared_data->configdata);
	ipconfigdata.ip[0] = ip_details.ip_address[0];
	ipconfigdata.ip[1] = ip_details.ip_address[1];
	ipconfigdata.ip[2] = ip_details.ip_address[2];
	ipconfigdata.ip[3] = ip_details.ip_address[3];

	ipconfigdata.sm[0] = ip_details.subnet_mask[0];
	ipconfigdata.sm[1] = ip_details.subnet_mask[1];
	ipconfigdata.sm[2] = ip_details.subnet_mask[2];
	ipconfigdata.sm[3] = ip_details.subnet_mask[3];

	ipconfigdata.gw[0] = ip_details.gateway[0];
	ipconfigdata.gw[1] = ip_details.gateway[1];
	ipconfigdata.gw[2] = ip_details.gateway[2];
	ipconfigdata.gw[3] = ip_details.gateway[3];

	ipconfigdata.ds[0] = 8;
	ipconfigdata.ds[1] = 8;
	ipconfigdata.ds[2] = 8;
	ipconfigdata.ds[3] = 8;

	// verify ethernet ip settings..// return 0 on sucess, 1 on error, 2 on change(req restart)
	i = verifyIPSettings(&ipconfigdata);// return 0 on sucess, 1 on error, 2 on change(req restart)
/*	if(i == 2)// restart to change settings..
	{
		printf("restart with new settings\n");
	}
	else if(i == 1)// error continue.. now.
	{
		printf("error in ip settings continue\n");
	}
	// else ok continue..
*/
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;
	shared_data->focusDayAdj = sharedConfigData->camera_parameters[0].FocusAdjustDay;
	shared_data->focusNightAdj = sharedConfigData->camera_parameters[0].FocusAdjustNight;
/*
		shared_data->CaptureWidth = SENSOR_IMG_WIDTH;
		shared_data->CaptureHeight = SENSOR_IMG_HEIGHT;
		shared_data->CaptureVoffset = 0;
		shared_data->Stream2Voffset = 0;
		shared_data->CameraType = 1;//0=lane,1=evidence;
		shared_data->CameraNumber = 0;
*/
//	sharedConfigData->general_details.primary_stream_resolution = EIGHT_MP;
/*	if(sharedConfigData->general_details.primary_stream_resolution == TWELVE_MP)// full res in test mode / else cropped mode..
	{
		shared_data->CaptureWidth = SENSOR_IMG_WIDTH;
		shared_data->CaptureHeight = SENSOR_IMG_HEIGHT;
		shared_data->CaptureVoffset = 0;
		shared_data->Stream2Voffset = 0;
	}
	else// 8MP fixed......
	{*/
		shared_data->CaptureWidth = CAPTURE_IMG_WIDTH;
		shared_data->CaptureHeight = CAPTURE_IMG_HEIGHT;
		shared_data->CaptureVoffset = 0;
		shared_data->Stream2Voffset = 0;
	//}
	shared_data->Zoom_init = 0;// 0= no zoom, 1 zoom on socket0
	shared_data->Zoom_left = 0;// 0 = no action, 1=go left
	shared_data->Zoom_right = 0;
	shared_data->Zoom_up = 0;
	shared_data->Zoom_down = 0;
	if(camera_parameters[0].Lane_or_Evidence > 3)
	{
		shared_data->CameraType = 1;//0=lane,1=evidence;
		shared_data->CameraNumber = camera_parameters[0].Lane_or_Evidence - 4;//lane number/ evidence number..
	}
	else
	{
		shared_data->CameraType = 0;//0=lane,1=evidence;
		shared_data->CameraNumber = camera_parameters[0].Lane_or_Evidence;//lane number/ evidence number..
	}
	shared_data->SyncStatus = 1;// to sync after eeprom config read and start camera..
	
//	InitializeLensModulepc(shared_data->Serialport4fd);

	for(i = 0; i < 4; i++)
		port_no[i] = ip_details.port_num[i];
	port_no[i] = 0;
//	display_buffer((unsigned char *)&womensafety_camera_parameters[0], sizeof(womensafety_camera_parameters)*65);
//	DEBUG_PRINT("-------------Init Server socket ..on port %d\n", at oi(port_no));
//	if(at oi(port_no) <= 0)
//		s printf(port_no,"3030");
//	sn printf(port_no, 5, "3132");
	inet_aton(ipaddrs,(struct in_addr *)(&socket_addr.sin_addr));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(3132);// always in 3132...
	addr_len = sizeof(struct sockaddr_in);
	socket_handle = socket(PF_INET, SOCK_STREAM, 0);
	j = (128 * 1024);//SOCKET_SEND_BUFF_LENGTH*8;
	i = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, &j, sizeof(int));
	j = 1;
	i = setsockopt(socket_handle, SOL_TCP,TCP_NODELAY, &j, sizeof(int));
	j = 1;
	i = setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int));
	process_timer[0] = MAX_PROCESS_LOOP_TIME;// timeout for app exit...
//	if(i < 0)
//	{
//		printf("socket option not set %x value = 0x%x\n",i, j);
//	}
	attempt = 0;
	if (bind(socket_handle,(struct sockaddr *)&socket_addr,addr_len) == -1)
	{
//		printf("Cannot bind addr to socket\n");
		return;
	}
	fcntl(socket_handle,F_SETFL,O_NONBLOCK);
	if (listen(socket_handle, NO_OF_SERVER_SOCKETS) == -1)
	{
//		printf("Listen error\n");
		return;
	}
	
//	DEBUG_PRINT("Listen socket ready...on port %d\n", at oi(port_no));
	shared_data->PcStreamStatus = PROCESSSTATUS_RUN;
//------------------------------------------------------------------------------
	pthread_create(&serialThrHandle,NULL,&serialPort4RxAccess, (void*)shared_data);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//	keepalive_timer[0] = 8;
//	keepalive_timer_reload[0] = 0;// 1sec..//
//	keepalive_timer_enabled[0]  = 1;
	// attempt to accept socket..
	sock_handle = 0;
	shared_data->FwUpgrade = 0;
//	DEBUG_PRINT("process %d init over write%d read %d\n", processIDVar, wdt_wr_pipe, wdt_rd_pipe);

	gettimeofday(&tval,NULL);// for timing...
	localtime_r(&tval.tv_sec, &brokentime);
	seccount = tval.tv_sec;
	rtcMtimer=5;
	timeinMsec = tval.tv_sec *100 + (tval.tv_usec/10000);// seconds count + 100th of seconds;
	
	if(battery_low)// after rtc read - if rtc battery low... or if rtc powerloss...
	{
//		DEBUG_PRINT("MKD: Battery Low\n");
		battery_low=0;
	}
	if(power_loss)// after rtc read - or if rtc powerloss then junk time...... correct load time as jan 1 2010...
	{
		power_loss = 0;
		brokentime.tm_sec = 0;//seconds
		brokentime.tm_min = 0;//minutes
		brokentime.tm_hour= 0;//hour
		brokentime.tm_mday= 1;//date
		brokentime.tm_mon = 0;//month
		brokentime.tm_year= 110;//year
		tval.tv_sec = mktime(&brokentime);
		tval.tv_usec = 0;
		//tval.tv_sec += ((5*60)+30)*60;// if not on utc, for utc to local time. subtract .5Hrand 30mins.
		settimeofday(&tval, NULL);
//		if(settimeofday(&tval, NULL) != 0)
//			DEBUG_PRINT("MKD: PowerLoss set time error\n");
		shared_data->rtctimecode[0] = brokentime.tm_sec;
		shared_data->rtctimecode[1] = brokentime.tm_min;
		shared_data->rtctimecode[2] = brokentime.tm_hour;
		shared_data->rtctimecode[3] = brokentime.tm_mday;
		shared_data->rtctimecode[4] = brokentime.tm_mon;
		shared_data->rtctimecode[5] = brokentime.tm_year;// from 1900
//		set_rtc_time(shared_data->rtctimecode);// pc to rtc
//		DEBUG_PRINT("MKD: PowerLoss \n");
	}	
	shared_data->RtcTimeUpdateSync = 0;
	while(1)
	{
		usleep(10000);// check in 10mSecs
		usleep(20000);// check in 20mSecs
		usleep(30000);// check in 30mSecs
		attempt++;
		if(attempt > 5000)
		{
//			j = 0;
			attempt = 0;
/*			for(i = 0; i < (NO_OF_SERVER_SOCKETS); i++)
			{
				DEBUG_PRINT("PcStream:sock %d = %s, ip= %s\n", i, (sock_thread_data[i].connection_status == SOCK_CONNECTED)?"Connected":((sock_thread_data[i].connection_status == SOCK_DISCONNECTED)?"Disconnected":"Closed"), (sock_thread_data[i].connection_status == SOCK_CONNECTED)?sock_thread_data[i].ip:"0");
				if(sock_thread_data[i].connection_status == SOCK_CONNECTED)
					j++;
			}
			connected_users = j;*/
		}
		sock_handle = accept(socket_handle,(struct sockaddr *)&socket_addr, &addr_len);
		if(sock_handle == -1)
		{
			//    printf("accept error %d again=%d\n", errno, EAGAIN);
			if(errno != EAGAIN)
			{
//				DEBUG_PRINT("Error in accept \n");
				break;
			}
		}
		else
		{
			getpeername(sock_handle, (struct sockaddr *)&(client_socket_addr), &addr_len);
			snprintf(ipaddrs, 17, "%s",inet_ntoa((struct in_addr) client_socket_addr.sin_addr));
			//if(safe_ <= 0)
			//	printf(" clinet ip read error.. no action taken..\n");
			for(i = 0; i < (NO_OF_SERVER_SOCKETS); i++)
			{
/*				if(sock_thread_data[i].connection_status == SOCK_DISCONNECTED)
				{
					DEBUG_PRINT("PcStream listening %d\n",i);
				}
				else
				{
					DEBUG_PRINT("PcStream sock %d connected from %s\n",i, sock_thread_data[i].ip);
				}*/
			}
			for(i = 0; i < (NO_OF_SERVER_SOCKETS); i++)
			{
				if(sock_thread_data[i].connection_status == SOCK_DISCONNECTED)
				{
					if(sock_handle)
					{
						loganevent("pcstream main: connection attempt from", ipaddrs);
//						DEBUG_PRINT("PcStream:sock %d new connection %d started from %s\n", i, sock_handle, ipaddrs);
						sock_thread_data[i].sock_number = i;
						sock_thread_data[i].connection_status = SOCK_CONNECTED;
						sock_thread_data[i].socket_handle = sock_handle;
						snprintf(sock_thread_data[i].ip, 18, "%s", ipaddrs);
						//if(safe_ <= 0)
						//	printf("sock ip error\n");
						snprintf(sock_thread_data[i].port_no, 8, "%s", port_no);
						//if(safe_ <= 0)
						//	printf("sock port error\n");
						sock_thread_data[i].client_keep_alive_tx_count = 4;
						keepalive_timer[i+1] = REMOTE_KEEPALIVETIMEOUT;
						keepalive_timer_reload[i+1] = 0;
						keepalive_timer_enabled[i+1]  = 1;
						sock_thread_data[i].shmem = shared_data;
						sock_thread_data[i].shmemcopc = compressPcShr;
						sock_thread_data[i].streaming_flag = 0;
						sock_thread_data[i].debugpipe = pipes[i];
						sock_thread_data[i].streamSelect = 0;
						sock_thread_data[i].level = GUEST;
						sock_thread_data[i].sentlogtxt = 0;
						expected_len[i] = 0;
						event_status[i][0] = 0;
						event_status[i][1] = 0;
						event_status[i][2] = 0;
						rx_in_progress[i] = 0;

//	ip = sizeof(int);
//	i = getsockopt(sock_handle, SOL_SOCKET, SO_SNDBUF, &jp, &ip);
//	printf("pc socket txbuff size = %d\n",jp);
//	ip = sizeof(int);
//	i = getsockopt(sock_handle, SOL_TCP,TCP_NODELAY, &jp, &ip);
//	printf("pc socket tcp delay = %d, should be 1\n",jp);
						pthread_create(&sock_thread[i],NULL,&threadedSocketCommunicationThread, &sock_thread_data[i]);
						usleep(50000);
//						addr_len = sizeof(int);
//						i = getsockopt(sock_handle, SOL_SOCKET, SO_SNDBUF, &j, &addr_len);
//						printf("buff size = %d\n", j);
						i = NO_OF_SERVER_SOCKETS+1;
					}
					//else
					//	DEBUG_PRINT("PcStream listening %d\n",i);
				}
				//else
				//	DEBUG_PRINT("PcStream sock %d connected from %s\n",i, sock_thread_data[i].ip);
			}
			if(i == (NO_OF_SERVER_SOCKETS))
			{
				close(sock_handle);
			}
			else
			{
				//printf("now process connection in new thread..\n");
				sock_handle = 0;
			}
		}
		if(rtcMtimer==0)
		{
			j = 0;
			for(i = 0; i < (NO_OF_SERVER_SOCKETS); i++)
			{
				if(sock_thread_data[i].connection_status == SOCK_CONNECTED)
					j++;
			}
			if(connected_users != j)
			{
				connected_users = j;
				buffer[0] = 0x30+j;
				buffer[1] = 0;
				if(j)
					loganevent("pcstream main: users connected",(const char*)buffer);
				else
					loganevent("pcstream main", "No users connected");
			}
			rtcMtimer=2;
			gettimeofday(&tval,NULL);// for timing...
			localtime_r(&tval.tv_sec, &brokentime);
			SecondsSinceLastRestart+=(tval.tv_sec - seccount);
			seccount = tval.tv_sec;
			shared_data->rtctimecode[0] = brokentime.tm_sec;
			shared_data->rtctimecode[1] = brokentime.tm_min;
			shared_data->rtctimecode[2] = brokentime.tm_hour;
			shared_data->rtctimecode[3] = brokentime.tm_mday;
			shared_data->rtctimecode[4] = brokentime.tm_mon;
			shared_data->rtctimecode[5] = brokentime.tm_year;// from 1900
			if((seccount & 0xf) == 0)
			{
				statvfs("/", &fsstat);
				fsfreesize = (unsigned short)((fsstat.f_bavail * fsstat.f_bsize) >> 23);
				//printf("fs free Mbytes = %d\n", fsfreesize);// same as df in sectors
			}
			//net speed = cat /sys/class/net/enP8p1s0/speed
			NetworkSpeed = getNetSpeed();
			j = getCableStatus();
			if(carrier_status != j)
			{
				carrier_status = j;
				if(carrier_status == 0)
					loganevent("pcstream main", "cable disconnected");
				else
					loganevent("pcstream main", "cable connected");
				
			}
//			DEBUG_PRINT("-----Time %02d:%02d:%02d %02d-%02d-%d---FS=%d--NS=%d\n", shared_data->rtctimecode[2], shared_data->rtctimecode[1], shared_data->rtctimecode[0], shared_data->rtctimecode[3], shared_data->rtctimecode[4]+1, shared_data->rtctimecode[5]+1900, fsfreesize, NetworkSpeed);
			if(configWrite_trigger)
			{
				if(configWrite_timer > 0)
				{
					configWrite_timer--;
					if(configWrite_timer == 0)
					{
						configWrite_trigger = 0;
						saveconfigdataenc();
					}
				}
				else if(configWrite_timer == 0)
				{
					configWrite_trigger = 0;
					saveconfigdataenc();
				}
				else 
					configWrite_trigger = 0;
			}
		}
//-----------------------------Maheen------------------------------------------------------------------------------
		if(shared_data->settime_flag == 1)
		{
			shared_data->settime_flag = 0;
			shared_data->RTCCmd = 2;//Write RTC
		}
		//0-None 1-Read 2-Write 3-Read Complete 4-Write Complete 5- Read Fail 6-Write Fail
		else if((shared_data->RTCCmd == 4))
		{
			//set_rtc_time(shared_data->setrtctimecode);
			brokentime.tm_sec = shared_data->setrtctimecode[0];//seconds
			brokentime.tm_min = shared_data->setrtctimecode[1];//minutes
			brokentime.tm_hour= shared_data->setrtctimecode[2];//hour
			brokentime.tm_mday= shared_data->setrtctimecode[3];//date
			brokentime.tm_mon = shared_data->setrtctimecode[4];//month
			brokentime.tm_year= shared_data->setrtctimecode[5];//year
//			printf("Write:Linux Set Time = %d:%d:%d %d:%d:%d\n",shared_data->setrtctimecode[2],shared_data->setrtctimecode[1],shared_data->setrtctimecode[0],
//								shared_data->setrtctimecode[3],shared_data->setrtctimecode[4],shared_data->setrtctimecode[5]);
//			DEBUG_PRINT("Write:Maheen:Linux Set Time = %d:%d:%d %d:%d:%d\n",shared_data->setrtctimecode[2],shared_data->setrtctimecode[1],shared_data->setrtctimecode[0],
//								shared_data->setrtctimecode[3],shared_data->setrtctimecode[4],shared_data->setrtctimecode[5]);
			tval.tv_sec = mktime(&brokentime);
			tval.tv_usec = 0;
	//		tval.tv_sec += ((5*60)+30)*60;// if not on utc, for utc to local time. subtract .5Hrand 30mins.
			settimeofday(&tval, NULL);
//			if(settimeofday(&tval, NULL) != 0)
//			{
//				DEBUG_PRINT("Maheen:Write set time error\n");
//				printf("Maheen:Write set time error\n");
//			}
//			mincount = 3200;
			usleep(250);
//			DEBUG_PRINT("Maheen:Write:Linux Time set done %ld\n", tval.tv_sec);
			sync();
			shared_data->RtcTimeUpdateSync = 1;

			shared_data->RTCCmd = 0;

//			printf("Write RTC Complete.. \n");
//			DEBUG_PRINT("Write RTC Complete..\n");					
		}
		else if((shared_data->RTCCmd == 3))
		{
			brokentime.tm_sec = shared_data->setrtctimecode[0];//seconds
			brokentime.tm_min = shared_data->setrtctimecode[1];//minutes
			brokentime.tm_hour= shared_data->setrtctimecode[2];//hour
			brokentime.tm_mday= shared_data->setrtctimecode[3];//date
			brokentime.tm_mon = shared_data->setrtctimecode[4]-1;//month
			brokentime.tm_year= shared_data->setrtctimecode[5]+100;//year
//			printf("Maheen:Read:Linux Set Time = %d:%d:%d %d:%d:%d\n",shared_data->setrtctimecode[2],shared_data->setrtctimecode[1],shared_data->setrtctimecode[0],
//								shared_data->setrtctimecode[3],shared_data->setrtctimecode[4]-1,shared_data->setrtctimecode[5]+100);
//			DEBUG_PRINT("Maheen:Read:Linux Set Time = %d:%d:%d %d:%d:%d\n",shared_data->setrtctimecode[2],shared_data->setrtctimecode[1],shared_data->setrtctimecode[0],
//								shared_data->setrtctimecode[3],shared_data->setrtctimecode[4]-1,shared_data->setrtctimecode[5]+100);
			tval.tv_sec = mktime(&brokentime);
			tval.tv_usec = 0;
	//		tval.tv_sec += ((5*60)+30)*60;// if not on utc, for utc to local time. subtract .5Hrand 30mins.
			settimeofday(&tval, NULL);
/*			if(settimeofday(&tval, NULL) != 0)
			{
				DEBUG_PRINT("Maheen:Read:set time error\n");
				printf("Maheen:Read:set time error\n");
			}*/
//			mincount = 3200;
			usleep(250);
//			DEBUG_PRINT("Maheen:Read:Linux Time set done %ld\n", tval.tv_sec);
			sync();
			shared_data->RtcTimeUpdateSync = 1;

			shared_data->RTCCmd = 0;

//			printf("Read RTC Complete.. \n");
//			DEBUG_PRINT("Read RTC Complete..\n");					
		}
//-----------------------------Maheen------------------------------------------------------------------------------
		

		if(shared_data->FactoryDefaults == 1)
		{
			Config_rebuild();		
			default_ip_details();
			ipconfigdata.ip[0] = new_ip_details.ip_address[0];
			ipconfigdata.ip[1] = new_ip_details.ip_address[1];
			ipconfigdata.ip[2] = new_ip_details.ip_address[2];
			ipconfigdata.ip[3] = new_ip_details.ip_address[3];

			ipconfigdata.sm[0] = new_ip_details.subnet_mask[0];
			ipconfigdata.sm[1] = new_ip_details.subnet_mask[1];
			ipconfigdata.sm[2] = new_ip_details.subnet_mask[2];
			ipconfigdata.sm[3] = new_ip_details.subnet_mask[3];

			ipconfigdata.gw[0] = new_ip_details.gateway[0];
			ipconfigdata.gw[1] = new_ip_details.gateway[1];
			ipconfigdata.gw[2] = new_ip_details.gateway[2];
			ipconfigdata.gw[3] = new_ip_details.gateway[3];

			ipconfigdata.ds[0] = 8;
			ipconfigdata.ds[1] = 8;
			ipconfigdata.ds[2] = 8;
			ipconfigdata.ds[3] = 8;

			// verify ethernet ip settings..// return 0 on sucess, 1 on error, 2 on change(req restart)
			i = verifyIPSettings(&ipconfigdata);// return 0 on sucess, 1 on error, 2 on change(req restart)
			sync();
			configWrite_trigger = 0;
			configWrite_timer = 0;
			saveconfigdataenc();
			sleep(3);
//			display_buffer((unsigned char*)&new_checksum, 32);
			shared_data->SystemReset = 1;
			shared_data->FactoryDefaults = 0;
		}
		if(shared_data->IPDefaults == 1)
		{
			default_ip_details();
			ipconfigdata.ip[0] = new_ip_details.ip_address[0];
			ipconfigdata.ip[1] = new_ip_details.ip_address[1];
			ipconfigdata.ip[2] = new_ip_details.ip_address[2];
			ipconfigdata.ip[3] = new_ip_details.ip_address[3];

			ipconfigdata.sm[0] = new_ip_details.subnet_mask[0];
			ipconfigdata.sm[1] = new_ip_details.subnet_mask[1];
			ipconfigdata.sm[2] = new_ip_details.subnet_mask[2];
			ipconfigdata.sm[3] = new_ip_details.subnet_mask[3];

			ipconfigdata.gw[0] = new_ip_details.gateway[0];
			ipconfigdata.gw[1] = new_ip_details.gateway[1];
			ipconfigdata.gw[2] = new_ip_details.gateway[2];
			ipconfigdata.gw[3] = new_ip_details.gateway[3];

			ipconfigdata.ds[0] = 8;
			ipconfigdata.ds[1] = 8;
			ipconfigdata.ds[2] = 8;
			ipconfigdata.ds[3] = 8;

			// verify ethernet ip settings..// return 0 on sucess, 1 on error, 2 on change(req restart)
			i = verifyIPSettings(&ipconfigdata);// return 0 on sucess, 1 on error, 2 on change(req restart)
			sync();
			configWrite_trigger = 0;
			configWrite_timer = 0;
			saveconfigdataenc();
			sleep(3);
//			display_buffer((unsigned char*)&new_checksum, 32);
			shared_data->SystemReset = 1;
			shared_data->IPDefaults = 0;
		}
		else if(saveEepromAll)// and restart..
		{
			save_programmed_data();
			configWrite_trigger = 0;
			configWrite_timer = 0;
			saveconfigdataenc();
			verifyEepromAll = 0;
			saveEepromLuxTable = 0;
//			DEBUG_PRINT("Reset\n");
//			printf("****PIPEFD %d\n",pipefd);
			//NanoReset(pipefd);
			//shared_data->SystemReset = 25;
			//shared_data->SystemShutdown=1;
			loganevent("pcstream main", "saveEepromAll..restart.!");
			sleep(3);
			//sy stem("shutdown now");
			fptr = popen("shutdown now", "r");// will execute the commands in string..
			sleep(1);
			fread(DebugStr,1, 100, fptr);
			pclose(fptr);// close will wait for the process to terminate and return..
			saveEepromAll=0;
			
			//shared_data->app_exit = 1;// restart....

		}
		else if(saveEepromLuxTable)
		{
			loganevent("pcstream main", "saveEepromLuxTable...!");
			save_womensafety_programmed_data();		  
			verifyEepromLuxTable = 1;
			saveEepromLuxTable = 0;
			configWrite_trigger = 0;
			configWrite_timer = 0;
			saveconfigdataenc();
		}
		
		if(verifyEepromAll)
		{
			verify_fram();
			verifyEepromAll = 0;
			getSharedConfigData((void *)shared_data->configdata);
			verifyEepromLuxTable = 0;
			shared_data->applyCurrentLightSettings = 1;
			configWrite_trigger = 0;
			configWrite_timer = 0;
			saveconfigdataenc();
		}
		else if(verifyEepromLuxTable)
		{
			verify_womensafety_fram();
			safe_memcpy(&womensafety_camera_parameters[0], sizeof(struct womensafety_cam)*65, &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);
			//mem cpy(&womensafety_camera_parameters[0], &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);
			safe_memcpy( sharedConfigData->womensafety_camera_parameters, sizeof(struct womensafety_cam)*65, new_womensafety_camera_parameters, sizeof(struct womensafety_cam)*65);
			//memvcpy( sharedConfigData->womensafety_camera_parameters, new_womensafety_camera_parameters, sizeof(struct womensafety_cam)*65);
			verifyEepromLuxTable = 0;
			shared_data->applyCurrentLightSettings = 1;
		}
		
		
		if(shared_data->app_exit == 1)
		{
//			DEBUG_PRINT("PcStream loop app exit flag\n");
			break;
		}
		if(keepalive_timer[0] == 0)
		{
//			DEBUG_PRINT("pcstream loop keepalive timeout\n");
			break;
		}
		else
		{
			if(keepalive_timer_reload[0] == 0)
			{
				if(read(wdt_rd_pipe, &c, 1) > 0)
				{
					//printf("-----------got wdt in PCSTREAM process\n");
					keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
					write(wdt_wr_pipe, &c, 1);
					usleep(1000);
				}
			}
		}
	}// end dead loop...
	shared_data->PcStreamStatus = PROCESSSTATUS_STOP;
	//------------------------------------------------------
}

void* threadedSocketCommunicationThread(void * arg)
{
	struct tst			*tst_var;
	unsigned char			*remote_data_tx_fifo_buff;	// memory for circular fifo
	unsigned char			*remote_data_rx_fifo_buff;	// memory for circular fifo
	unsigned short			rx_data_buffer[MAX_RX_DATA_BUFFER * 2], tx_data_buffer[MAX_TX_DATA_BUFFER];
	struct cirfifo			remote_tx_fifo, remote_rx_fifo;
	struct pollfd 			socket_poll;
	int 				i, j, polltimeout;
	SHARED_RESOURCES		*shared_data;
	COMPRESS_PC_SHARED_RESOURCES	*compressPcShr;
	int 				timer_no;

	char 		DebugStr[1024];
	int 		DebugStrSize;

	// init
//	printf("PcStream thread_started init\n");
	tst_var = (struct tst *)arg;
	timer_no = tst_var->sock_number+1;

	shared_data = (SHARED_RESOURCES*)tst_var->shmem;
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES*)tst_var->shmemcopc;
	remote_data_tx_fifo_buff = (unsigned char *)malloc(MAXREMOTE_TX_DATA_FIFO_LENGTH);
	remote_data_rx_fifo_buff = (unsigned char *)malloc(MAXREMOTE_RX_DATA_FIFO_LENGTH);
	tst_var->rx_data_buffer = rx_data_buffer;
	tst_var->tx_data_buffer = tx_data_buffer;
	tst_var->remote_tx_fifo = &remote_tx_fifo;
	tst_var->remote_rx_fifo = &remote_rx_fifo;
	fifo_init(&remote_tx_fifo, MAXREMOTE_TX_DATA_FIFO_LENGTH, remote_data_tx_fifo_buff);
	fifo_init(&remote_rx_fifo, MAXREMOTE_RX_DATA_FIFO_LENGTH, remote_data_rx_fifo_buff);
	client_dev_type[tst_var->sock_number] = 0;
	compressPcShr->PcImageStreamReq[0][tst_var->sock_number] = 0;
	compressPcShr->PcImageStreamReq[1][tst_var->sock_number] = 0;
	compressPcShr->PcImageStreamReq[2][tst_var->sock_number] = 0;
	compressPcShr->PcImageStreamReq[3][tst_var->sock_number] = 0;
	compressPcShr->PcImageInit[0][tst_var->sock_number] = 0;
	compressPcShr->PcImageInit[1][tst_var->sock_number] = 0;
	compressPcShr->PcImageInit[2][tst_var->sock_number] = 0;
	compressPcShr->PcImageInit[3][tst_var->sock_number] = 0;
//	DEBUG_PRINT("PcStream connection status = %d\n", tst_var->connection_status);

	// processing.. loop
//	DEBUG_PRINT("PcStream thread_started looping connection status = %d\n",tst_var->connection_status);
	tst_var->clientAccess = 0;
	socktimer[tst_var->sock_number] = 2;
	tst_var->imgstxed = 0;
	tst_var->SecondsConnected = 0;
	while(1)
	{
		//usleep(500000);
		//printf("connection status = %d aux timer = %d timer = %d reload = %d\n", tst_var->connection_status, tst_var->client_keep_alive_tx_count, tst_var->remote_keep_alive_count,tst_var->remote_keep_alive_count_reload);
		//------------------------------------------------------
		if(tst_var->connection_status == SOCK_CONNECTED)
		{
			// poll the socket and do tx and rx, if it fails disconnect.
			if(socktimer[tst_var->sock_number] == 0)
			{
				socktimer[tst_var->sock_number] = 2;
				tst_var->SecondsConnected+=2;
/*				DEBUG_PRINT("Socket %d: %s images %d, since %06dSecs, streams sts %d %d %d %d\n", tst_var->sock_number, tst_var->ip, tst_var->imgstxed, tst_var->SecondsConnected,\
												compressPcShr->PcImageInit[0][tst_var->sock_number],\
												compressPcShr->PcImageInit[1][tst_var->sock_number],\
												compressPcShr->PcImageInit[2][tst_var->sock_number],\
												compressPcShr->PcImageInit[3][tst_var->sock_number]);*/
			}
			if(keepalive_timer[timer_no] == 0)
			{
//				DEBUG_PRINT("PcStream keep alive timout socket..%d disconnecting...\n", tst_var->sock_number);
				tst_var->connection_status = SOCK_CLOSE;
				usleep(10000);
			}
			else
			{
/*				socket_poll.fd = tst_var->socket_handle;
				socket_poll.events = POLLIN|POLLOUT|POLLHUP|POLLNVAL|POLLERR;
//				socket_poll.events = POLLOUT|POLLHUP|POLLNVAL|POLLERR;
				polltimeout = POLL_TIMEOUT_MSEC;
//				if(tst_var->remote_tx_fifo->filled_length > 100)
//					polltimeout = 0;
				if(poll(&socket_poll, 1, polltimeout) > 0)
				{
					if((socket_poll.revents & POLLHUP) || (socket_poll.revents & POLLNVAL) || (socket_poll.revents & POLLERR))
					{
						tst_var->connection_status = SOCK_CLOSE;
						DEBUG_PRINT("PcStream Error POLLHUP \n");
					}
					else if(socket_poll.revents & POLLIN)
					{
						if(server_rx(tst_var))
							process_rx(tst_var);
					}
					else if(socket_poll.revents & POLLOUT)
					{
						j = 0;
						process_rx(tst_var);
						process_tx(tst_var);
						if(server_rx(tst_var))
						{
							process_rx(tst_var);
						}
						else if(tst_var->remote_tx_fifo->filled_length >= SOCKET_SEND_BUFF_LENGTH)
						{
							j = server_tx(tst_var);
							if(j < 0)
							{
								tst_var->connection_status = SOCK_CLOSE;
								DEBUG_PRINT("PcStream TX Error disconnect \n");
							}
							else if(j>0)
							{
								DeadDelayUS(100);
								//usleep(4000);
							}
						}
						else
						{
							//DeadDelayMS(30);
							usleep(4000);
						}
						//DeadDelayMS(10);
					}
					else
					{
						tst_var->connection_status = SOCK_CLOSE;
						DEBUG_PRINT("PcStream Error NO POLLOUT \n");
					}
				}
				else //poll error... may be disconnect..
				{
					//tst_var->connection_status = SOCK_CLOSE;
			    		usleep(4000);
				}
				*/
				socket_poll.fd = tst_var->socket_handle;
				socket_poll.events = POLLIN|POLLOUT|POLLHUP|POLLNVAL|POLLERR;
				polltimeout = POLL_TIMEOUT_MSEC;
//				if(tst_var->remote_tx_fifo->filled_length > 100)
//					polltimeout = 0;
				if(poll(&socket_poll, 1, 10) > 0)
				{
					if((socket_poll.revents & POLLHUP) || (socket_poll.revents & POLLNVAL) || (socket_poll.revents & POLLERR))
					{
						tst_var->connection_status = SOCK_CLOSE;
//						DEBUG_PRINT("PcStream Error POLLHUP \n");
					}
					else if(socket_poll.revents & POLLOUT)
					{
						j = 0;
						j = server_tx(tst_var);
						if(j < 0)
						{
							tst_var->connection_status = SOCK_CLOSE;
//							DEBUG_PRINT("PcStream TX Error disconnect \n");
						}
						if(socket_poll.revents & POLLIN)
						{
							server_rx(tst_var);
							process_rx(tst_var);
						}
						else
						{
							process_rx(tst_var);
							process_tx(tst_var);
//							if(tst_var->remote_tx_fifo->filled_length < 100)
//								usleep(1000);
//							else
//							{
//								if(j)
									usleep(1000);
//							}
						}
					}
					else
					{
						tst_var->connection_status = SOCK_CLOSE;
//						DEBUG_PRINT("PcStream Error NO POLLOUT \n");
					}
				}
				else //poll error... may be disconnect..
				{
					//tst_var->connection_status = SOCK_CLOSE;
			    		usleep(10000);
				}
			}
		}
		//------------------------------------------------------
		else
		{
			// if(tst_var->connection_status == SOCK_CLOSE)
			// or error condition..
			if(tst_var->socket_handle > 0)
				close(tst_var->socket_handle);
			if(systemPGMMode == tst_var->sock_number)
			{
				systemPGMMode = 99;// no user..
				verifyEepromAll = 1;// reset the
//				printf("systemPGMMode == tst_var->sock_number\n");
//				DEBUG_PRINT("systemPGMMode == tst_var->sock_number \n");
				shared_data->SystemReset = 1;
			}
			if(shared_data->Zoom_init == tst_var->sock_number+1)
				shared_data->Zoom_init = 0;
			shared_data->PcConnectionStatus[tst_var->sock_number] = 0;
			compressPcShr->PcImageInit[0][tst_var->sock_number] = 0;
			compressPcShr->PcImageInit[1][tst_var->sock_number] = 0;
			compressPcShr->PcImageInit[2][tst_var->sock_number] = 0;
			compressPcShr->PcImageInit[3][tst_var->sock_number] = 0;
			compressPcShr->PcImageStreamReq[0][tst_var->sock_number] = 0;
			compressPcShr->PcImageStreamReq[1][tst_var->sock_number] = 0;
			compressPcShr->PcImageStreamReq[2][tst_var->sock_number] = 0;
			compressPcShr->PcImageStreamReq[3][tst_var->sock_number] = 0;
//			DEBUG_PRINT("PcStream Server Socket %d closed\n",tst_var->sock_number);
			client_dev_type[tst_var->sock_number] = 0;
			j = 0;
			for(i = 0; i < NO_OF_SERVER_SOCKETS; i++)
			{
				if(client_dev_type[i] == 12)
					j = 1;
			}
			if(j == 0)
				VPUConnected = 0;
			update_view_list(tst_var);
			for(i = 0; i < MAX_RAW_IMAGE_PC_BUFFER; i++)
				compressPcShr->PcImageStatus[0][i][tst_var->sock_number] = 0;
			for(i = 0; i < MAX_RAW_IMAGE_PC_BUFFER; i++)
				compressPcShr->PcImageStatus[1][i][tst_var->sock_number] = 0;
			if(tst_var->connection_status != SOCK_DISCONNECTED)
			{
				tst_var->connection_status = SOCK_DISCONNECTED;
				loganevent("pcstream thrd", "SOCK_DISCONNECTED...!");
			}
			tst_var->clientAccess = 0;
			break;
		}
	}// loop end
	// deinit
	free(remote_data_tx_fifo_buff);
	free(remote_data_rx_fifo_buff);
	return(arg);
}

static int fill_init_data(unsigned short *dataptr)
{
  int i,j;
  char *cptr;
  unsigned short *tempptr, *temptr1;
  i = 0;

  *(dataptr + i++) = 34;			// 00 model no  .. A0 for ats tk1, A1 for RLVD tk1
  *(dataptr + i++) = 0;				// 01 dual codec flag
  *(dataptr + i++) = 4;				// 02 no of channels
  *(dataptr + i++) = 1;				// 03 for pc pal=0,ntsc = 1 -- video system Now -
  
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  *(dataptr + i++) = 0;
  cptr = (char *)(dataptr + i);
  for(j = 0; j < 32; j++)
    *cptr++ = FirmwareV2[j];// + checksum
  i+=32;
  
  for(j = 0; j < 20 ; j++)						// 04-31 left blank for future use
  {
    *(dataptr + i++) = 0;			
  }

  //general_details_avs.device_type=general_details.sensor_type;
  general_details_avs.primary_stream_resolution=general_details.primary_stream_resolution;
  for(j=0;j<1;j++)
  {
    general_details_avs.primary_stream_quality[j]=general_details.primary_stream_quality[j];
    general_details_avs.primary_stream_encode[j]=general_details.primary_stream_enabled[j];
    general_details_avs.primary_stream_tl_mode[j]=general_details.primary_stream_fps[j];
    general_details_avs.second_stream_quality[j]=general_details.second_stream_quality[j];
    general_details_avs.second_stream_tl_mode[j]=general_details.second_stream_fps[j];
  }
  general_details_avs.dome_type=general_details.dome_type;
  general_details_avs.dome_baud=general_details.dome_baud;

  general_details_avs.second_stream_enabled=general_details.second_stream_enabled;
  general_details_avs.second_stream_resolution=general_details.second_stream_resolution;
  general_details_avs.relay_out_polarity=general_details.relay_out_polarity;
  general_details_avs.device_type=general_details.device_type;
  if(safe_strncpy(general_details_avs.name,general_details.name,9) == NULL)
  	printf("avs name not copied default/last remains\n");
  general_details_avs.primary_stream_type=general_details.primary_stream_type;
  general_details_avs.second_stream_type=general_details.second_stream_type;

  tempptr = (unsigned short *)&general_details_avs;
  for(j = 0; j < ((sizeof(struct gnrl_avs)+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  } 
  
  for(j=0;j<1;j++)
  {
    if(safe_strncpy(camera_parameters_avs[j].name,camera_parameters[j].name,9) == NULL)
  	printf("camera name not copied default/last remains\n");
    camera_parameters_avs[j].brightness=camera_parameters[j].brightness;
    camera_parameters_avs[j].contrast=camera_parameters[j].contrast;
    camera_parameters_avs[j].saturation=camera_parameters[j].saturation;
    camera_parameters_avs[j].type=camera_parameters[j].type;
    camera_parameters_avs[j].dome_address=camera_parameters[j].dome_address;
    
    //mem cpy(camera_parameters_avs[j].activity_data,camera_parameters[j].activity_data,64);
  }

  tempptr = (unsigned short *)&camera_parameters_avs[0];
  for(j = 0; j < (((sizeof(struct cam_avs))*4+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }

  tempptr = (unsigned short *)&ip_details;
  for(j = 0; j < ((sizeof(struct ip)+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }
	
  //update_blank_remote_user();
  //tempptr = (unsigned short *)&blank_remote_user_avs[0];
  tempptr = (unsigned short *)&remote_user_avs[0];
  
  remote_user_avs[0].covert_list[0]=camera_parameters[0].Lane_or_Evidence;
  remote_user_avs[0].covert_list[1]=camera_parameters[0].DayNight_ColourMode;
  remote_user_avs[0].covert_list[2]=camera_parameters[0].FocusAdjustDay;
  remote_user_avs[0].covert_list[3]=camera_parameters[0].FocusAdjustNight;
  remote_user_avs[0].nick_name[0]=camera_parameters[0].FramesPerTrigger;
  remote_user_avs[0].nick_name[1]=general_details.gamma_value;	      
  remote_user_avs[0].wb_data_1=wb_details.wb_red_gain;
  remote_user_avs[0].wb_data_2=wb_details.wb_green1_gain;
  remote_user_avs[1].wb_data_1=wb_details.wb_green2_gain;
  remote_user_avs[1].wb_data_2=wb_details.wb_blue_gain;
  remote_user_avs[2].wb_data_1=wb_details.wb_red_offset;
  remote_user_avs[2].wb_data_2=wb_details.wb_green1_offset;
  remote_user_avs[3].wb_data_1=wb_details.wb_green2_offset;
  remote_user_avs[3].wb_data_2=wb_details.wb_blue_offset;	  
  for(j = 0; j < (((sizeof(struct remo_avs))*16+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }
  
  no_checksum_data_avs.second_framerate=no_checksum_data.second_framerate;
  no_checksum_data_avs.second_bitrate=no_checksum_data.second_bitrate;
  no_checksum_data_avs.second_index=no_checksum_data.second_index;
  no_checksum_data_avs.configured=no_checksum_data.configured;
  no_checksum_data_avs.relay_out_onoff=no_checksum_data.relay_out_onoff;
  no_checksum_data_avs.general_details_default_loaded=no_checksum_data.general_details_default_loaded;
  no_checksum_data_avs.camera_parameters_default_loaded=no_checksum_data.camera_parameters_default_loaded;
  no_checksum_data_avs.remote_user_default_loaded=no_checksum_data.remote_user_default_loaded;
  no_checksum_data_avs.ip_details_default_loaded=no_checksum_data.ip_details_default_loaded;
  no_checksum_data_avs.schedule_default_loaded=no_checksum_data.schedule_default_loaded;
  no_checksum_data_avs.dydns_data_default_loaded=no_checksum_data.dydns_data_default_loaded;
  no_checksum_data_avs.alarm_data_default_loaded=no_checksum_data.alarm_data_default_loaded;
	
  tempptr = (unsigned short *)&no_checksum_data_avs;
  for(j = 0; j < ((sizeof(struct nocs_avs)+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }
  
  tempptr = (unsigned short *)&schedule_avs[0][0];
  for(j = 0; j < (((sizeof(struct sch_avs))*8+1)/2); j++) //host_ver:006 only one schedule send (*9 removed)
  {
    *(dataptr + i++) = *tempptr++;
  }

  safe_memcpy(&dydns_data_avs,(sizeof (struct dydns)),&dydns_data,(sizeof (struct dydns)));
  //mem cpy(&dydns_data_avs,&dydns_data,(sizeof (struct dydns)));
  tempptr = (unsigned short *)&dydns_data_avs;
  for(j = 0; j < 64; j++)				//128 word changed to 64 words
  {
    *(dataptr + i++) = *tempptr++;
  }

  alarm_data_avs.input_polarity=alarm_data.input_polarity;
  alarm_data_avs.input_type=alarm_data.input_type;
  alarm_data_avs.duration=alarm_data.duration;
  alarm_data_avs.enabled=alarm_data.enabled;
  
/*  tempptr = (unsigned short *)&alarm_data_avs;
  for(j = 0; j < ((sizeof(struct alrm_avs)+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }*/
  temptr1=(unsigned short*)malloc(32);
  if(temptr1 != NULL)
  {
    safe_memcpy(temptr1, 32, &alarm_data_avs, 32);  
    //mem cpy(temptr1,&alarm_data_avs,32);  
    //tempptr = (unsigned short *)&alarm_data_avs;
    for(j = 0; j < 16; j++)
    {
      //*(dataptr + i++) = *tempptr++;    
      dataptr[i]=temptr1[j];
      i++;
    }
    free(temptr1);
  }
  
  safe_memcpy(write_protected_data_avs.model_num, 9, write_protected_data.model_num, 9);
  //mem cpy(write_protected_data_avs.model_num, write_protected_data.model_num, 9);
  safe_memcpy(write_protected_data_avs.product_ver, 9, write_protected_data.product_ver, 9);  
  //mem cpy(write_protected_data_avs.product_ver, write_protected_data.product_ver, 9);  
  write_protected_data_avs.device_type=write_protected_data.device_type;
  write_protected_data_avs.serial_num=write_protected_data.serial_num;
  write_protected_data_avs.date_of_mfg=write_protected_data.date_of_mfg;
  write_protected_data_avs.month_of_mfg=write_protected_data.month_of_mfg;
  write_protected_data_avs.year_of_mfg=  write_protected_data.year_of_mfg;
  safe_memcpy(write_protected_data_avs.mac_address, 6, write_protected_data.mac_address, 6);  
  //mem cpy(write_protected_data_avs.mac_address,write_protected_data.mac_address, 6);  
  
  tempptr = (unsigned short *)&write_protected_data_avs;
  for(j = 0; j < ((sizeof(struct wrpr_avs)+1)/2); j++)
  {
    *(dataptr + i++) = *tempptr++;
  }

  return i;
}

static unsigned short sequence_no[NO_OF_SERVER_SOCKETS] = {1};

static void update_pc_status(unsigned short *status_data)
{
	unsigned short socket_status_template[32] = {0};
	
	socket_status_template[0] = 0x1234;
	socket_status_template[1] = 0x1234;
	socket_status_template[2] = 0x80;

	socket_status_template[3] = 0;// seq no..
	socket_status_template[4] = 0;// size ls
	socket_status_template[5] = 0;// size ms

	socket_status_template[6] = 0;//ack no;				// 
	socket_status_template[7] = 0;//Enc type				// 0 = no encryption, 1 = all data encrypted, 2= upto 1k encrypted(for images).
	socket_status_template[8] = 0;//search_on;				// any search ?
	socket_status_template[9] = 0;//(unsigned short)image_pos;		// current rec_position in rec disk (64 bits)
	socket_status_template[10] = 0;//(unsigned short)(image_pos >> 16);	// current rec_position in rec disk (64 bits)
	socket_status_template[11] = 0;//(unsigned short)(image_pos >> 32);	// current rec_position in rec disk (64 bits)
	socket_status_template[12] = 0;//(unsigned short)(image_pos >> 48);	// current rec_position in rec disk (64 bits)
	socket_status_template[13] = 0;//(unsigned short)fat_pos;		// current rec_position in rec disk (64 bits)
	socket_status_template[14] = 0;//(unsigned short)(fat_pos >> 16);	// current rec_position in rec disk (64 bits)
	socket_status_template[15] = 0;//(unsigned short)(fat_pos >> 32);	// current rec_position in rec disk (64 bits)
	socket_status_template[16] = 0;//(unsigned short)(fat_pos >> 48);	// current rec_position in rec disk (64 bits)
	socket_status_template[17] = 0;					//current_disk;
	socket_status_template[18] = 0;//total_active_sockets;		// no of users connected..
	socket_status_template[19] = 0;//ftp_lock;				// no ftp = 99, else user no.
	socket_status_template[20] = 0;//tst->sock_number;			// curernt user no...
	socket_status_template[21] = 0;//ALARMRECORD;				//
	socket_status_template[22] = 0;//ExtAlarmCount;
	socket_status_template[23] = 34;//camera 				// model No...
	socket_status_template[24] = 0;					// backup status...
	socket_status_template[25] = 1;	// 
	socket_status_template[26] = 1;	// 
	socket_status_template[28] = 17;			//
	socket_status_template[29] = 0;//(((unsigned char)(shared_data->lux)) << 8) | 0;//no_checksum_data.scheduled_recording;//
	socket_status_template[30] = 0;
	socket_status_template[31] = 0;
	safe_memcpy(status_data, 64, socket_status_template, 64);
	//mem cpy(status_data, socket_status_template, 64);
	// CAUTION: >> max value is 32...
}

void update_view_list(struct tst *tst)
{
	int i, j, k, l[NO_OF_SERVER_SOCKETS], p, q, t;
	COMPRESS_PC_SHARED_RESOURCES	*compressPcShr;
	t = 99;
	for(j = 0; j< NO_OF_SERVER_SOCKETS; j++)
	{
		l[j] = 99;
		if(client_dev_type[j] == 12)
		{
			l[j] = j;
			t=j;
		}
	}
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES*)tst->shmemcopc;

	// 0 = full res
	i = 0; k = 0; p = 0; q = 0;
	for(j = 0; j< NO_OF_SERVER_SOCKETS; j++)
	{
		if(t > 10)// vpu not connected..
		{
			if(compressPcShr->PcImageStreamReq[0][j])
			{
				i = 1;
			}
			if(compressPcShr->PcImageStreamReq[1][j])
			{
				k = 1;
			}
			if(compressPcShr->PcImageStreamReq[2][j])
			{
				p = 1;
			}
			if(compressPcShr->PcImageStreamReq[3][j])
			{
				q = 1;
			}
		}
		else// vpu connected..
		{
			if(j == l[j])
			{
				if(compressPcShr->PcImageStreamReq[0][j])
				{
					i = 1;
				}
				if(compressPcShr->PcImageStreamReq[1][j])
				{
					k = 1;
				}
				if(compressPcShr->PcImageStreamReq[2][j])
				{
					p = 1;
				}
				if(compressPcShr->PcImageStreamReq[3][j])
				{
					q = 1;
				}
			}
			else
			{
				if(compressPcShr->PcImageStreamReq[0][j])// no full res to others..
				{
					compressPcShr->PcImageStreamReq[0][j] = 0;
					if(compressPcShr->PcImageStreamReq[1][j] == 0)
					{
						compressPcShr->PcImageStreamReq[1][j] = 1;
						compressPcShr->PcImageInit[1][j] = 1;
					}
				}
				if(compressPcShr->PcImageStreamReq[1][j])
				{
					k = 1;
				}
				if(compressPcShr->PcImageStreamReq[2][j])
				{
					compressPcShr->PcImageStreamReq[2][j] = 0;
					if(compressPcShr->PcImageStreamReq[1][j] == 0)
					{
						compressPcShr->PcImageStreamReq[1][j] = 1;
						compressPcShr->PcImageInit[1][j] = 1;
					}
				}
				if(compressPcShr->PcImageStreamReq[3][j])
				{
					q = 1;
				}
			}
		}
//		printf("viewlist Sock %d Lane=%d VGA=%d Evid=%d AllVeh=%d\n",j,m,n, r, s);
	}
	compressPcShr->PcViewList[0] = i;
	compressPcShr->PcViewList[1] = k;
	compressPcShr->PcViewList[2] = p;
	compressPcShr->PcViewList[3] = q;
}

void copy_status_flags_n_send(unsigned int tx_size, struct tst *tst)
{
	int temp1;
//	FILE *fptr;
//	char filename[32];
	const unsigned char zerodata[16] = {0};
	unsigned long tx_short_size, temp_tx_size;
	//unsigned char tempdata[MAX_TX_DATA_BUFFER];
	SHARED_RESOURCES 	*shared_data;

	shared_data = (SHARED_RESOURCES*)tst->shmem;
	safe_memcpy((unsigned char*)&(tst->tx_data_buffer[tx_size]), 16, (void *)zerodata, 16);// always copy occurs on a big memeory.. no overflow
	//mem cpy((unsigned char*)&(tst->tx_data_buffer[tx_size]), zerodata, 16);

	temp_tx_size = (((tx_size*2)-18)+16) & 0x1FFFF0;
	tx_short_size = temp_tx_size + (MAX_SOCKET_DATA_TR -1);
	tx_short_size &= MAX_TX_DATA_SIZE_MASK;
	temp_tx_size = tx_short_size;
	
	tx_short_size = (temp_tx_size + 18)>>1;
	
	update_pc_status(tst->tx_data_buffer);
	tst->tx_data_buffer[7] = 0;//2;// Enc type.. 
	tst->tx_data_buffer[SEQUENCE_NO] = sequence_no[tst->sock_number]++;
	tst->tx_data_buffer[PACKET_LENGTH] = tx_short_size;
	tst->tx_data_buffer[PACKET_LENGTH + 1] = tx_short_size >> 16;
	tst->tx_data_buffer[20] = tst->sock_number;
	tst->tx_data_buffer[29] = (((unsigned char)(shared_data->lux)) << 8) ;
	fifo_write(tst->remote_tx_fifo, (unsigned char*)&(tst->tx_data_buffer[0]), tx_short_size*2);
}

void process_rx(struct tst *tst)
{
	unsigned int 		i, j, k;
	unsigned short 		so_buff[32], csum = 0, *eepromdataptr, *eepromdataptr1, temp1, temp2, mcnt;
	unsigned int 		id[2], tempid[2];
	static unsigned int 	fdataptr;
	FILE 			*ispfileptr, *fptr;
	unsigned char		meta_data_buf[4096], tempcharbuffer0[MAX_RX_DATA_BUFFER*2], tempcharbuffer1[MAX_RX_DATA_BUFFER*2], *key_data_ptr;
	char			serialData[32];  
	int 			readcount = 0;
	int	 		temprd = 0, cputemp = 0, gputemp = 0;
	SHARED_RESOURCES 	*shared_data;
	COMPRESS_PC_SHARED_RESOURCES	*compressPcShr;
	int 			timer_no;
	SHARED_CONFIG_DATA 	*sharedConfigData;
	struct IPCONFIGDATA 	ipconfigdata;
	int 			drift;
	struct timeval 		tval;
	struct tm		brokentime;
	struct 			tm tmp_time_st;
	time_t 			tmp_ltime0;
	unsigned char		username[9] = {0}, password[7] = {0}, ip[4] = {0}, userlevel = 0;
	static unsigned char	userip[NO_OF_SERVER_SOCKETS][4], ClientData[NO_OF_SERVER_SOCKETS][32] = {0};
	char 		version_1[256], fver[64], ver[64], h2b[128];
	size_t 		indatasize, sig_len;
	unsigned char 	*data, *signature, *outfile;
	int 		outfilesize, result;
	size_t 		offset;
	unsigned char 	keyfilename[] = "cspublic.pem";
	unsigned char 	keyfilenameenc[] = "cspublic.pem.enc";
	FILE 		*pubkey_file;
	EVP_PKEY 	*public_key;
	char 		over[128], nver[128];
	T_PACKVARS 	oldv, newv;
	unsigned char 	keystr[10] = {0};
	const int 	idvar = 132;
	
	timer_no = tst->sock_number+1;
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES*)tst->shmemcopc;
	shared_data = (SHARED_RESOURCES*)tst->shmem;
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;

	if(rx_in_progress[tst->sock_number] == 0)
	{
		if(tst->remote_rx_fifo->filled_length >= 40)
		{
			fifo_peek(tst->remote_rx_fifo, (unsigned char*)so_buff, 40);
			if((so_buff[0] == 0x1234) && (so_buff[1] == 0x1234))// valid command...
			{
				rx_in_progress[tst->sock_number] = 1;
				expected_len[tst->sock_number] = *((unsigned long*)&so_buff[PACKET_LENGTH])*2;// convert to bytes
			}
			else
			{
				// invalid command.. close socket and disconnect..
				tst->connection_status = SOCK_CLOSE;
				rx_in_progress[tst->sock_number] = 0;
//				DEBUG_PRINT("PcStream camera command error\n");
			}
		}
	}
	else
	{
		if(tst->remote_rx_fifo->filled_length >= expected_len[tst->sock_number])// all data recieved.. process command...
		{
			rx_in_progress[tst->sock_number] = 0;
			fifo_read(tst->remote_rx_fifo, (unsigned char*)tst->rx_data_buffer, expected_len[tst->sock_number]);
			printf("PcStream rxed full command size %u - %x %x %x %x log file %d\n", expected_len[tst->sock_number], tst->rx_data_buffer[7], tst->rx_data_buffer[17], tst->rx_data_buffer[18], tst->rx_data_buffer[19], tst->sentlogtxt);
			tst->tx_data_buffer[32] =  tst->rx_data_buffer[17];// 
			tst->tx_data_buffer[33] =  0x55;
			if((tst->clientAccess == 0) || (tst->clientAccess == 1))
			{
				switch(tst->rx_data_buffer[17])
				{
				case 0x8A:// start command.. //ip no..
					if(tst->clientAccess == 0)
						tst->clientAccess = 1;
					else
						tst->rx_data_buffer[17] = 0;
					break;
				case 0x8B:// start command.. //ip no..
					if(tst->clientAccess != 1)
						tst->rx_data_buffer[17] = 0;
					break;
				default:
					tst->rx_data_buffer[17] = 0;
					break;
				}
			}
			else
			{
				keepalive_timer_reload[timer_no] = REMOTE_KEEPALIVETIMEOUT;
				
			}
			switch(tst->rx_data_buffer[17])
			{
			case 0x8A:// start command.. //ip no..
				userip[tst->sock_number][0] = (unsigned char)tst->rx_data_buffer[18];
				userip[tst->sock_number][1] = (unsigned char)tst->rx_data_buffer[19];
				userip[tst->sock_number][2] = (unsigned char)tst->rx_data_buffer[20];
				userip[tst->sock_number][3] = (unsigned char)tst->rx_data_buffer[21];
//				DEBUG_PRINT("PcStream command 8A rxed----------\n");
				copy_status_flags_n_send(34, tst);
				break;
				
				
			case 0x8B:// start command.. //username password..
//				DEBUG_PRINT("PcStream command 8B rxed----------\n");
				if(tst->clientAccess == 1)// check...
				{
					username[0] = (unsigned char)(tst->rx_data_buffer[18] >> 8);
					username[1] = (unsigned char)tst->rx_data_buffer[18];

					username[2] = (unsigned char)(tst->rx_data_buffer[19] >> 8);
					username[3] = (unsigned char)tst->rx_data_buffer[19];
					
					username[4] = (unsigned char)(tst->rx_data_buffer[20] >> 8);
					username[5] = (unsigned char)tst->rx_data_buffer[20];

					username[6] = (unsigned char)(tst->rx_data_buffer[21] >> 8);
					username[7] = (unsigned char)tst->rx_data_buffer[21];
					
					password[0] = (unsigned char)(tst->rx_data_buffer[22] >> 8);
					password[1] = (unsigned char)tst->rx_data_buffer[22];

					password[2] = (unsigned char)(tst->rx_data_buffer[23] >> 8);
					password[3] = (unsigned char)tst->rx_data_buffer[23];

					password[4] = (unsigned char)(tst->rx_data_buffer[24] >> 8);
					password[5] = (unsigned char)tst->rx_data_buffer[24];

					username[8] = 0;
					password[6] = 0;
					for(i = 0; i < MAX_REMOTE_USER; i++)
					{
						k = 0;
						for(j = 0; j < 8; j++)
						{
							if(username[j] != remote_user[i].name[j])
							{
								k = 1;
								break;
							}	
						}
						if(k == 0) // username match..
						{
							for(j = 0; j < 6; j++)
							{
								if(password[j] != remote_user[i].password[j])
								{
									k = 1;
									break;
								}
							}
							if(k == 0)// password match.. continue..
							{
								break;
							}
						}
						k = 1;
					}
					if(k != 0)
					{
						loganevent("pcstream thrd: user connection failed", (const char *)username);
						tst->connection_status = SOCK_CLOSE;// disconnect
					}
					else
					{
						if((i < MAX_REMOTE_USER) && (i >= 0))
						{
							loganevent("pcstream thrd: user connect", (const char *)remote_user[i].name);
							tst->tx_data_buffer[34] = remote_user[i].level;
							userlevel = remote_user[i].level;
							tst->level = remote_user[i].level;
							copy_status_flags_n_send(35, tst);
							tst->clientAccess = 2;
						}
						else
						{
							loganevent("pcstream thrd: user connection failed", (const char *)username);
							tst->connection_status = SOCK_CLOSE;// disconnect
						}
					}
					if(tst->clientAccess == 2)// connected...
					{
						for(i = 0; i < 3; i++)
						{
							if(event_status[tst->sock_number][i] == 0)// free..
							{
								eventdatauser[tst->sock_number][i][0] = 1;// connected..
								eventdatauser[tst->sock_number][i][1] = 46;// size..
								safe_memcpy(&eventdatauser[tst->sock_number][i][2], 4, userip[tst->sock_number], 4);
								//mem cpy(&eventdatauser[tst->sock_number][i][2], userip[tst->sock_number], 4);
								safe_memcpy(&eventdatauser[tst->sock_number][i][2+4], 8, username, 8);
								//mem cpy(&eventdatauser[tst->sock_number][i][2+4], username, 8);
								eventdatauser[tst->sock_number][i][2+4+8] = userlevel;
								eventdatauser[tst->sock_number][i][2+4+8+1] = (unsigned char)tst->sock_number;
								safe_memcpy(&eventdatauser[tst->sock_number][i][2+4+8+1+1], 32, ClientData[tst->sock_number], 32);
								//mem cpy(&eventdatauser[tst->sock_number][i][2+4+8+1+1], ClientData[tst->sock_number], 32);
								event_status[tst->sock_number][i] = 1;
								break;
							}
						}
					}
				}
				else
				{
					loganevent("pcstream thrd: invalid user connection failed","ivalid user! 8b multiple");
					tst->connection_status = SOCK_CLOSE;// disconnect
				}
				break;
				
			case 0x8C:// start command..  //type.
				if((tst->rx_data_buffer[18] == 0xAAAA) && (tst->rx_data_buffer[19] == 0xcccc))
				{
					if(tst->rx_data_buffer[20] == 16)// Linuxpc..
					{
						client_dev_type[tst->sock_number] = 4;// Violation NVR device..
//						DEBUG_PRINT("client_device_type = NVR-----XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx\n");
					}
					else if(tst->rx_data_buffer[20] == 25)// VPU..
					{
						client_dev_type[tst->sock_number] = 12;// Violation NVR device..
//						DEBUG_PRINT("client_device_type = NVR-----XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx\n");
						VPUConnected = 1;
					}
					else
					{
						client_dev_type[tst->sock_number] = 0;// pc/other device...
//						DEBUG_PRINT("client_device_type = PC-----XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx\n");
					}
				}
				else
				{
					client_dev_type[tst->sock_number] = 0;// pc/other device...
//					DEBUG_PRINT("default client_device_type = OTHER-----XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx\n");
				}
				tst->tx_data_buffer[34] = 34;// ...//TK1// 00 model no  .. A0 for ats tk1, A1 for RLVD tk1
				tst->tx_data_buffer[35] = 0xAA;//mac 0
				tst->tx_data_buffer[36] = 0xBB;//mac 1
				tst->tx_data_buffer[37] = 0xCC;//mac 2
				tst->tx_data_buffer[38] = 0xDD;//mac 3
				tst->tx_data_buffer[39] = 0xEE;//mac 4
				tst->tx_data_buffer[40] = 0xFF;//mac 5
				copy_status_flags_n_send(41, tst);
//				DEBUG_PRINT("PcStream command 8C rxed----------\n");
				break;
		
			case 0x8D:// start command.. init data
//				DEBUG_PRINT("PcStream command 8D rxed----------\n");
				i = fill_init_data(&(tst->tx_data_buffer[34]));
				copy_status_flags_n_send(34 + i, tst);
//				DEBUG_PRINT("PcStream command 8D processed----------\n");
				break;
		
			case 0x2B:// view list ..
//				compressPcShr->PcImageStreamReq[1][tst->sock_number] = (((tst->rx_data_buffer[18])?1:0) | ((tst->rx_data_buffer[19])?1:0));// only rec streams necessary

				compressPcShr->PcImageStreamReq[1][tst->sock_number] = tst->rx_data_buffer[18];//?1:0;	// VGA		// only rec streams necessary
				compressPcShr->PcImageStreamReq[0][tst->sock_number] = tst->rx_data_buffer[19];//?1:0;	// LANE
				compressPcShr->PcImageStreamReq[2][tst->sock_number] = tst->rx_data_buffer[20];//?1:0;	// Evidence
				compressPcShr->PcImageStreamReq[3][tst->sock_number] = tst->rx_data_buffer[21];//?1:0;	// AI
				compressPcShr->PcImageStreamReq[1][tst->sock_number] |= tst->rx_data_buffer[22];//?1:0;	// vga
//				DEBUG_PRINT("0PcStream view list cmd rxed---------- ch0=%d, ch1=%d, ch2=%d, ch3=%d, ch4=%d, list %d %d %d %d\n", 
//				tst->rx_data_buffer[18], tst->rx_data_buffer[19], tst->rx_data_buffer[20], tst->rx_data_buffer[21], tst->rx_data_buffer[22],
//				compressPcShr->PcImageStreamReq[0][tst->sock_number],
//				compressPcShr->PcImageStreamReq[1][tst->sock_number],
//				compressPcShr->PcImageStreamReq[2][tst->sock_number],
//				compressPcShr->PcImageStreamReq[3][tst->sock_number]);

//compressPcShr->PcImageStreamReq[3][tst->sock_number] = 1;

				tst->streamSelect = 0;
				j = 0;
				for(i = 0; i < 4; i++)
				{
					if(compressPcShr->PcImageStreamReq[i][tst->sock_number])
					{
						if(compressPcShr->PcImageInit[i][tst->sock_number] == 0)
							compressPcShr->PcImageInit[i][tst->sock_number] = 1;
						j |= (1 << i);
					}
					else
					{
						compressPcShr->PcImageInit[i][tst->sock_number] = 0;
					}
				}
				tst->streams = j;
				update_view_list(tst);
//				DEBUG_PRINT("1PcStream view list cmd rxed---------- ch0=%d, ch1=%d, ch2=%d, ch3=%d, ch4=%d, list %d %d %d %d\n", 
//				tst->rx_data_buffer[18], tst->rx_data_buffer[19], tst->rx_data_buffer[20], tst->rx_data_buffer[21], tst->rx_data_buffer[22],
//				compressPcShr->PcImageStreamReq[0][tst->sock_number],
//				compressPcShr->PcImageStreamReq[1][tst->sock_number],
//				compressPcShr->PcImageStreamReq[2][tst->sock_number],
//				compressPcShr->PcImageStreamReq[3][tst->sock_number]);
				copy_status_flags_n_send(34, tst);
				break;
		
			case 0x2C:// start streaming of view list ..
//				DEBUG_PRINT("PcStream start stream cmd rxed----------\n");
				tst->streamSelect = 0;
				if(tst->streaming_flag == 0)
				{
					tst->streaming_flag = 1;
					((SHARED_RESOURCES*)tst->shmem)->PcConnectionStatus[tst->sock_number] = 1;
				}
				break;
		
			case 0x2D:// view list ok..
				tst->streaming_flag = 0;
				copy_status_flags_n_send(34, tst);
//				DEBUG_PRINT("PcStream 2D cmd rxed----------\n");
				break;

			case 0x4A:    //Light Settings from PC for online apply
				safe_memcpy(&new_womensafety_camera_parameters[0],(sizeof(struct womensafety_cam))*65,&tst->rx_data_buffer[20],(sizeof(struct womensafety_cam))*65);		
				//mem cpy(&new_womensafety_camera_parameters[0],&tst->rx_data_buffer[20],(sizeof(struct womensafety_cam))*65);		
				if(tst->rx_data_buffer[18] == 1) //Save & exit
				{
					saveEepromLuxTable = 1;
//					printf("Save & Exit\n");
				}
				else if(tst->rx_data_buffer[18] == 2) //Exit without saving
				{
					verifyEepromLuxTable = 1;
//					printf("Exit Without Saving\n");
				}
				else
				{
					tempLuxTable = 3;
					safe_memcpy(sharedConfigData->womensafety_camera_parameters, sizeof(struct womensafety_cam)*65, new_womensafety_camera_parameters, sizeof(struct womensafety_cam)*65);
					//mem cpy(sharedConfigData->womensafety_camera_parameters, new_womensafety_camera_parameters, sizeof(struct womensafety_cam)*65);
					shared_data->applyCurrentLightSettings = 1;
//					printf("\n\n\n\n\n\n apply settings...\n\n\n");
				}
				tst->tx_data_buffer[34] = shared_data->lux;
				tst->tx_data_buffer[35] = 0;
				copy_status_flags_n_send(36, tst);
				break;

			case 0x4F://Download Current Light Settings to PC
				safe_memcpy(&tst->tx_data_buffer[36],(sizeof(struct womensafety_cam))*65,&womensafety_camera_parameters[0],(sizeof(struct womensafety_cam))*65);
				//mem cpy(&tst->tx_data_buffer[36],&womensafety_camera_parameters[0],(sizeof(struct womensafety_cam))*65);
				copy_status_flags_n_send( 36 + (((sizeof(struct womensafety_cam))*65)/2), tst);
				break;

		
				
			case 0x5B:// start command..
				tst->tx_data_buffer[34] = shared_data->lux;
				tst->tx_data_buffer[35] = shared_data->crntLightTable;
				tst->tx_data_buffer[36] = 0;
				tst->tx_data_buffer[37] = 0;
				tst->tx_data_buffer[38] = 0;
				tst->tx_data_buffer[39] = 0;
				tst->tx_data_buffer[40] = 0;
				tst->tx_data_buffer[41] = shared_data->IcrControl;
				tst->tx_data_buffer[42] = 0;
				tst->tx_data_buffer[43] = VPUConnected;
				tst->tx_data_buffer[44] = FWversion;

				fptr=fopen("/sys/devices/virtual/thermal/thermal_zone0/temp","r");
				if(fptr!=NULL)
				{
					if((safe_fgets(serialData,32, 32, fptr))>=0)// returned succes...
						//cputemp=((int)at oi(serialData))/100;
						if(safe_atoi(serialData, &temprd) > 0)
							cputemp=(int)(temprd/100);
						else
							cputemp=0;//

					fclose(fptr);
				}
				fptr=fopen("/sys/devices/virtual/thermal/thermal_zone1/temp","r");
				if(fptr!=NULL)
				{
					if((safe_fgets(serialData,32, 32, fptr))>=0)// returned succes...
						//gputemp=((int)at oi(serialData))/100;
						if(safe_atoi(serialData, &temprd) > 0)
							gputemp=(int)(temprd/100);
						else
							gputemp=0;//
						
					fclose(fptr);
				}
				tst->tx_data_buffer[45] = cputemp;
//				tst->tx_data_buffer[46] = shared_data->SensorTemprature;//gputemp;
				tst->tx_data_buffer[46] = gputemp;
				
				tst->tx_data_buffer[47] = sharedConfigData->camera_parameters[0].Lane_or_Evidence;//cameratype;
				tst->tx_data_buffer[48] = systemPGMMode;//system_mode;
				gettimeofday(&tval,NULL);// for timing...
				localtime_r(&tval.tv_sec, &brokentime);
				tst->tx_data_buffer[49] = brokentime.tm_sec;
				tst->tx_data_buffer[50] = brokentime.tm_min;
				tst->tx_data_buffer[51] = brokentime.tm_hour;
				tst->tx_data_buffer[52] = brokentime.tm_wday;
				tst->tx_data_buffer[53] = brokentime.tm_mday;
				tst->tx_data_buffer[54] = brokentime.tm_mon;
				tst->tx_data_buffer[55] = brokentime.tm_year;// from 1900
				tst->tx_data_buffer[56] = connected_users;
				tst->tx_data_buffer[57] = sharedConfigData->general_details.primary_stream_resolution;
				tst->tx_data_buffer[58] = sharedConfigData->general_details.primary_stream_fps[0];
				tst->tx_data_buffer[59] = (unsigned short)SecondsSinceLastRestart;
				tst->tx_data_buffer[60] = (unsigned short)(SecondsSinceLastRestart >> 16);
				tst->tx_data_buffer[61] = (unsigned short)shared_data->SensorErrorCnt;
				tst->tx_data_buffer[62] = fsfreesize;
				tst->tx_data_buffer[63] = NetworkSpeed;
				copy_status_flags_n_send(64, tst);
				tst->client_keep_alive_tx_status = 0;
//				DEBUG_PRINT("PcStream %d Keep alive command rxed---%d %x %x-------\n", tst->sock_number, SecondsSinceLastRestart, tst->tx_data_buffer[59], tst->tx_data_buffer[60]);
				break;
		
			case 0x5D:// Camera Config..
				if((tst->rx_data_buffer[18] == 19) || (tst->rx_data_buffer[18] == 0x1300))
				{
//					DEBUG_PRINT("PcStream time set command = %d:%d:%d %d-%d-%d\n", tst->rx_data_buffer[21], tst->rx_data_buffer[20], tst->rx_data_buffer[19], tst->rx_data_buffer[22], tst->rx_data_buffer[23], tst->rx_data_buffer[24]);
					shared_data->setrtctimecode[0] = (unsigned char)tst->rx_data_buffer[19];
					shared_data->setrtctimecode[1] = (unsigned char)tst->rx_data_buffer[20];
					shared_data->setrtctimecode[2] = (unsigned char)tst->rx_data_buffer[21];
					shared_data->setrtctimecode[3] = (unsigned char)tst->rx_data_buffer[22];
					shared_data->setrtctimecode[4] = (unsigned char)tst->rx_data_buffer[23];
					shared_data->setrtctimecode[5] = (unsigned char)tst->rx_data_buffer[24];
					shared_data->settime_flag = 1;
				} 
				else if(tst->rx_data_buffer[18]==39) //Zoom Adjust
				{	      
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",1,(unsigned char)tst->rx_data_buffer[19],(unsigned char)tst->rx_data_buffer[20],0,0,(unsigned char)('#'+'_'+1+(unsigned char)tst->rx_data_buffer[19]+(unsigned char)tst->rx_data_buffer[20]));
//					printf("shared_data->Serialport4fd:   %d %02x %02x\n",shared_data->Serialport4fd, (unsigned char)tst->rx_data_buffer[19],(unsigned char)tst->rx_data_buffer[20]);	
					if(shared_data->Zoom_init == tst->sock_number+1)
					{
						if(tst->rx_data_buffer[19] == 8)
						{
							if(tst->rx_data_buffer[20] == 0)
								shared_data->Zoom_left = 1;
							if(tst->rx_data_buffer[20] == 1)
								shared_data->Zoom_right = 1;
						}
					}
				}
				else if(tst->rx_data_buffer[18]==40) //Focus Adjust
				{	      
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",2,(unsigned char)tst->rx_data_buffer[19],(unsigned char)tst->rx_data_buffer[20],0,0,(unsigned char)('#'+'_'+2+(unsigned char)tst->rx_data_buffer[19]+(unsigned char)tst->rx_data_buffer[20]));
//					printf("shared_data->Serialport4fd:   %d %02x %02x\n",shared_data->Serialport4fd, (unsigned char)tst->rx_data_buffer[19],(unsigned char)tst->rx_data_buffer[20]);					
					if(shared_data->Zoom_init == tst->sock_number+1)
					{
						if(tst->rx_data_buffer[19] == 8)
						{
							if(tst->rx_data_buffer[20] == 0)
								shared_data->Zoom_up = 1;
							if(tst->rx_data_buffer[20] == 1)
								shared_data->Zoom_down = 1;
						}
					}
				}
				else if(tst->rx_data_buffer[18]==41) //Save Zoom
				{	      
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",6,0,0,0,0,(unsigned char)('#'+'_'+6));
//					printf("shared_data->Serialport4fd:   %d\n",shared_data->Serialport4fd);					
					if(shared_data->Zoom_init == tst->sock_number+1)
						shared_data->Zoom_init = 0;
				}
				else if(tst->rx_data_buffer[18]==42) //Save Focus 
				{	      
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",7,0,0,0,0,(unsigned char)('#'+'_'+7));
//					printf("shared_data->Serialport4fd:   %d\n",shared_data->Serialport4fd);					
				}
				else if(tst->rx_data_buffer[18]==43) //Focus Day Adjust
				{
					new_camera_parameters[0].FocusAdjustDay=(unsigned char)tst->rx_data_buffer[19];
					shared_data->focusDayAdj = new_camera_parameters[0].FocusAdjustDay;
//					printf("Focus Day adjust %d\n",(unsigned char)tst->rx_data_buffer[19]);
					//save_camera_parameters();
					//verify_camera_parameters ();
				}
				else if(tst->rx_data_buffer[18]==44) //Focus Night Adjust
				{
					new_camera_parameters[0].FocusAdjustNight=(unsigned char)tst->rx_data_buffer[19];
					shared_data->focusNightAdj = new_camera_parameters[0].FocusAdjustNight;
//					printf("Focus Night adjust %d\n",(unsigned char)tst->rx_data_buffer[19]);
					//save_camera_parameters();
					//verify_camera_parameters ();
				}
				else if(tst->rx_data_buffer[18]==45) //Focus Init
				{
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",4,0,0,0,0,(unsigned char)('#'+'_'+4));
//					printf(" shared_data->Serialport4fd:   %d\n",shared_data->Serialport4fd);					
				}
				else if(tst->rx_data_buffer[18]==46) //Zoom Init
				{
					shared_data->Zoom_init = tst->sock_number+1;// 0= no zoom, 1 zoom on socket0
					shared_data->Zoom_left = 0;// 0 = no action, 1=go left
					shared_data->Zoom_right = 0;
					shared_data->Zoom_up = 0;
					shared_data->Zoom_down = 0;
					//s printf(serialData, "M$#_%02x%02x%02x%02x%02x%02x\n",5,0,0,0,0,(unsigned char)('#'+'_'+5));
//					printf(" shared_data->Serialport4fd:   %d\n",shared_data->Serialport4fd);					
					//shared_data->SystemReset = 1;
				}
				else if(tst->rx_data_buffer[18]==47)//Night test
				{	  
					shared_data->IcrControl = SWITCHON;
//					shared_data->changeSensorParms = 1;
//					printf("Switch to Night \n");
				}
				else if(tst->rx_data_buffer[18]==48)//DayTest
				{	      
					shared_data->IcrControl = SWITCHOFF;
//					shared_data->changeSensorParms = 1;
//					printf("Switch to Day \n");
				}	    
				else if(tst->rx_data_buffer[18]==21)
				{
					if(systemPGMMode == tst->sock_number)
					{
						new_general_details.gamma_value=tst->rx_data_buffer[19];
						sharedConfigData->general_details.gamma_value = tst->rx_data_buffer[19];// it will auto clear on user logout.
//						DEBUG_PRINT("Gamma params received\n");
				}
				}
				else if(tst->rx_data_buffer[18]==6)
				{
					if(systemPGMMode == tst->sock_number)
					{
						sharedConfigData->wb_details.wb_red_gain 	= tst->rx_data_buffer[19];
						sharedConfigData->wb_details.wb_green1_gain 	= tst->rx_data_buffer[20];
						sharedConfigData->wb_details.wb_green2_gain 	= tst->rx_data_buffer[21];
						sharedConfigData->wb_details.wb_blue_gain 	= tst->rx_data_buffer[22];
						sharedConfigData->wb_details.wb_red_offset	= tst->rx_data_buffer[23];
						sharedConfigData->wb_details.wb_green1_offset	= tst->rx_data_buffer[24];
						sharedConfigData->wb_details.wb_green2_offset	= tst->rx_data_buffer[25];
						sharedConfigData->wb_details.wb_blue_offset	= tst->rx_data_buffer[26];
//						DEBUG_PRINT("-------------Wb params received Gains=%d %d %d %d, offsets=%d %d %d %d\n", tst->rx_data_buffer[19], tst->rx_data_buffer[20], tst->rx_data_buffer[21], tst->rx_data_buffer[22], tst->rx_data_buffer[23], tst->rx_data_buffer[24], tst->rx_data_buffer[25], tst->rx_data_buffer[26]);
					}
				}
				else if(tst->rx_data_buffer[18]==11)
				{
					if(systemPGMMode == tst->sock_number)
					{
						sharedConfigData->wb_details.wb_red_gain 	= wb_details.wb_red_gain;
						sharedConfigData->wb_details.wb_green1_gain 	= wb_details.wb_green1_gain;
						sharedConfigData->wb_details.wb_green2_gain 	= wb_details.wb_green2_gain;
						sharedConfigData->wb_details.wb_blue_gain 	= wb_details.wb_blue_gain;
						sharedConfigData->wb_details.wb_red_offset	= wb_details.wb_red_offset;
						sharedConfigData->wb_details.wb_green1_offset	= wb_details.wb_green1_offset;
						sharedConfigData->wb_details.wb_green2_offset	= wb_details.wb_green2_offset;
						sharedConfigData->wb_details.wb_blue_offset	= wb_details.wb_blue_offset;
//						DEBUG_PRINT("wb params Restored\n");
					}
				}
				///----------------------------------------------------	anpr
				copy_status_flags_n_send(34, tst);
				break;
		
			case 0x63:// Meta data..
				for(i = 0; i < 64; i++)
				{
					tst->tx_data_buffer[34+i] = 0;
				}
				memset((void *)meta_data_buf, 0, sizeof(meta_data_buf));
				safe_memcpy(&tst->tx_data_buffer[34+32], sizeof(meta_data_buf), meta_data_buf, sizeof(meta_data_buf));// source buffer is large.. no underflow
				//mem cpy(&tst->tx_data_buffer[34+32], meta_data_buf, 4096);
				copy_status_flags_n_send((34+32+2048), tst);
//				DEBUG_PRINT("PcStream metadata cmd rxed----------\n");
				break;

			case(0x6A):
				// configuration download to PC...
				// struct gnrl	general_details;
				// struct cam	camera_parameters[4];
				// struct ip	ip_details;
				// struct remo	remote_user[16];
				// struct nocs	no_checksum_data;
				// struct chsm	checksum;
				// struct wrpr	write_protected_data;
				// struct sch	schedule[9][8];
				// unsigned char	dydns_data[256];
				// struct alrm	alarm_data;
					
				if(systemPGMMode != 99)   
				{
					// already in pgm mode.. reply fail..
					tst->tx_data_buffer[33] =  0x01;
					tst->tx_data_buffer[34] =  systemPGMMode;// current user..
					copy_status_flags_n_send(35, tst);
				}
				else if(tst->level != ADMIN)
				{
					// already in pgm mode.. reply fail..
					tst->tx_data_buffer[33] =  0x01;
					tst->tx_data_buffer[34] =  0xfe;// current user..
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// continue ok..
					loganevent("pcstream thrd", "configuration entered ...!");
					systemPGMMode = tst->sock_number;
					//general_details_avs.sensor_type=general_details.sensor_type;
					general_details_avs.primary_stream_resolution=general_details.primary_stream_resolution;
					for(j=0;j<1;j++)
					{
						general_details_avs.primary_stream_quality[j]=general_details.primary_stream_quality[j];
						general_details_avs.primary_stream_encode[j]=general_details.primary_stream_enabled[j];
						general_details_avs.primary_stream_tl_mode[j]=general_details.primary_stream_fps[j];
						general_details_avs.second_stream_quality[j]=general_details.second_stream_quality[j];
						general_details_avs.second_stream_tl_mode[j]=general_details.second_stream_fps[j];		    
					}
					general_details_avs.dome_type=general_details.dome_type;
					general_details_avs.dome_baud=general_details.dome_baud;

					general_details_avs.second_stream_enabled=general_details.second_stream_enabled;
					general_details_avs.second_stream_resolution=general_details.second_stream_resolution;
					general_details_avs.relay_out_polarity=general_details.relay_out_polarity;
					general_details_avs.device_type=general_details.device_type;
					if(safe_strncpy(general_details_avs.name,general_details.name,9) == NULL)
					  	printf("avs name not copied default/last remains\n");
					general_details_avs.primary_stream_type=general_details.primary_stream_type;
					general_details_avs.second_stream_type=general_details.second_stream_type;

					eepromdataptr = (unsigned short *)&general_details_avs;
					temp2 = 0;		  
					for(temp1 = 0; temp1 < ((sizeof(struct gnrl_avs)+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1]= *eepromdataptr++;
					}   

					temp2 = temp1;
					for(j=0;j<1;j++)
					{
						if(safe_strncpy(camera_parameters_avs[j].name,camera_parameters[j].name,9) == NULL)
							printf("camera name not copied default/last remains\n");
						camera_parameters_avs[j].brightness=camera_parameters[j].brightness;
						camera_parameters_avs[j].contrast=camera_parameters[j].contrast;
						camera_parameters_avs[j].saturation=camera_parameters[j].saturation;
						//mem cpy(camera_parameters_avs[j].activity_data,camera_parameters[j].activity_data,64); no activity data
						camera_parameters_avs[j].type=camera_parameters[j].type;
						camera_parameters_avs[j].dome_address=camera_parameters[j].dome_address;		
					}
					eepromdataptr = (unsigned short *)&camera_parameters_avs[0];
					for(; temp1 < temp2 + (((sizeof(struct cam_avs))*4+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&ip_details;
					for(; temp1 < temp2 + ((sizeof(struct ip)+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					for(j=0;j<4;j++)
					{
						safe_memcpy(remote_user_avs[j].name, 9, remote_user[j].name,9);
						//mem cpy(remote_user_avs[j].name,remote_user[j].name,9);
						safe_memcpy(remote_user_avs[j].password, 6, remote_user[j].password,6);
						//mem cpy(remote_user_avs[j].password,remote_user[j].password,6);
						remote_user_avs[j].level=remote_user[j].level;		    
					}
					remote_user_avs[0].covert_list[0]=camera_parameters[0].Lane_or_Evidence;
					remote_user_avs[0].covert_list[1]=camera_parameters[0].DayNight_ColourMode;
					remote_user_avs[0].covert_list[2]=camera_parameters[0].FocusAdjustDay;
					remote_user_avs[0].covert_list[3]=camera_parameters[0].FocusAdjustNight;
					remote_user_avs[0].nick_name[0]=camera_parameters[0].FramesPerTrigger;
					remote_user_avs[0].nick_name[1]=general_details.gamma_value;
					remote_user_avs[0].wb_data_1=wb_details.wb_red_gain;
					remote_user_avs[0].wb_data_2=wb_details.wb_green1_gain;
					remote_user_avs[1].wb_data_1=wb_details.wb_green2_gain;
					remote_user_avs[1].wb_data_2=wb_details.wb_blue_gain;
					remote_user_avs[2].wb_data_1=wb_details.wb_red_offset;
					remote_user_avs[2].wb_data_2=wb_details.wb_green1_offset;
					remote_user_avs[3].wb_data_1=wb_details.wb_green2_offset;
					remote_user_avs[3].wb_data_2=wb_details.wb_blue_offset;		  
					eepromdataptr = (unsigned short *)&remote_user_avs[0];
					for(; temp1 < temp2 + (((sizeof(struct remo_avs))*16+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					no_checksum_data_avs.second_framerate=no_checksum_data.second_framerate;
					no_checksum_data_avs.second_bitrate=no_checksum_data.second_bitrate;
					no_checksum_data_avs.second_index=no_checksum_data.second_index;
					no_checksum_data_avs.configured=no_checksum_data.configured;
					no_checksum_data_avs.relay_out_onoff=no_checksum_data.relay_out_onoff;
					no_checksum_data_avs.general_details_default_loaded=no_checksum_data.general_details_default_loaded;
					no_checksum_data_avs.camera_parameters_default_loaded=no_checksum_data.camera_parameters_default_loaded;
					no_checksum_data_avs.remote_user_default_loaded=no_checksum_data.remote_user_default_loaded;
					no_checksum_data_avs.ip_details_default_loaded=no_checksum_data.ip_details_default_loaded;
					no_checksum_data_avs.schedule_default_loaded=no_checksum_data.schedule_default_loaded;
					no_checksum_data_avs.dydns_data_default_loaded=no_checksum_data.dydns_data_default_loaded;
					no_checksum_data_avs.alarm_data_default_loaded=no_checksum_data.alarm_data_default_loaded;	
					eepromdataptr = (unsigned short *)&no_checksum_data_avs;		  
					for(; temp1 < temp2 + ((sizeof(struct nocs_avs)+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&schedule_avs[0][0];
					for(; temp1 < temp2 + (((sizeof(struct sch_avs))*8+1)/2); temp1++) //only one schedule send (*9 removed)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					safe_memcpy(&dydns_data_avs,(sizeof (struct dydns)),&dydns_data,(sizeof (struct dydns)));
					//mem cpy(&dydns_data_avs,&dydns_data,(sizeof (struct dydns)));
					eepromdataptr = (unsigned short *)&dydns_data_avs;
					for(; temp1 < temp2 + 64; temp1++)		//host_ver:006 128 word changed to 64 words
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					temp2 = temp1;
					alarm_data_avs.input_polarity=alarm_data.input_polarity;
					alarm_data_avs.input_type=alarm_data.input_type;
					alarm_data_avs.duration=alarm_data.duration;
					alarm_data_avs.enabled=alarm_data.enabled;
					/*	      eepromdataptr = (unsigned short *)&alarm_data_avs;
					for(; temp1 < temp2 + ((sizeof(struct alrm_avs)+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}*/
					eepromdataptr1=(unsigned short *)malloc(32);
					if(eepromdataptr1 != NULL)
					{
						safe_memcpy(eepromdataptr1, 32, &alarm_data_avs, 32);
						//mem cpy(eepromdataptr1,&alarm_data_avs,32);
						//eepromdataptr = (unsigned short *)&alarm_data_avs;
						for(mcnt=0; temp1 < temp2 + ((sizeof(struct alrm_avs)+1)/2); temp1++)
						{
							tst->tx_data_buffer[36 + temp1] = eepromdataptr1[mcnt];//++;	
							mcnt++;
						}
						free(eepromdataptr1);
					}

					temp2 = temp1;
					safe_memcpy(write_protected_data_avs.model_num, 9, write_protected_data.model_num, 9);
					//mem cpy(write_protected_data_avs.model_num, write_protected_data.model_num, 9);
					safe_memcpy(write_protected_data_avs.product_ver, 9, write_protected_data.product_ver, 9);  
					//mem cpy(write_protected_data_avs.product_ver, write_protected_data.product_ver, 9);  
					write_protected_data_avs.device_type=write_protected_data.device_type;
					write_protected_data_avs.serial_num=write_protected_data.serial_num;
					write_protected_data_avs.date_of_mfg=write_protected_data.date_of_mfg;
					write_protected_data_avs.month_of_mfg=write_protected_data.month_of_mfg;
					write_protected_data_avs.year_of_mfg=  write_protected_data.year_of_mfg;
					safe_memcpy(write_protected_data_avs.mac_address, 6, write_protected_data.mac_address, 6);  		  
					//mem cpy(write_protected_data_avs.mac_address,write_protected_data.mac_address, 6);  		  
					eepromdataptr = (unsigned short *)&write_protected_data_avs;
					for(; temp1 < temp2 + ((sizeof(struct wrpr_avs)+1)/2); temp1++)
					{
						tst->tx_data_buffer[36 + temp1] = *eepromdataptr++;
					}

					copy_status_flags_n_send(36 + temp1, tst);
//					DEBUG_PRINT ("config data download to pc completed\n");
				}
				break;
	      

			case 0x6B:
			case 0x6C:
//				DEBUG_PRINT("PcStream PC COnfig data cmd 0x%X rxed----------\n", tst->rx_data_buffer[17]);
				if(systemPGMMode == tst->sock_number)
				{// user is in config.....
					eepromdataptr = (unsigned short *)&general_details_avs;
					temp2 = 0;
					for(temp1 = 0; temp1 < ((sizeof(struct gnrl_avs)+1)/2); temp1++)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}
					//new_general_details.sensor_type=general_details_avs.sensor_type;
					new_general_details.primary_stream_resolution=general_details_avs.primary_stream_resolution;
					for(j=0;j<1;j++)
					{
						new_general_details.primary_stream_quality[j]=general_details_avs.primary_stream_quality[j];		
						new_general_details.primary_stream_fps[j]=general_details_avs.primary_stream_tl_mode[j];
						new_general_details.second_stream_quality[j]=general_details_avs.second_stream_quality[j];
						new_general_details.second_stream_fps[j]=general_details_avs.second_stream_tl_mode[j];
					}

					new_general_details.dome_type=general_details_avs.dome_type;
					new_general_details.dome_baud=general_details_avs.dome_baud;

					new_general_details.second_stream_enabled=general_details_avs.second_stream_enabled;
					new_general_details.second_stream_resolution=general_details_avs.second_stream_resolution;
					new_general_details.relay_out_polarity=general_details_avs.relay_out_polarity;
					new_general_details.device_type=general_details_avs.device_type;
					if(safe_strncpy(new_general_details.name,general_details.name,9) == NULL)
						  	printf("avs name not copied default/last remains\n");
					new_general_details.primary_stream_type=general_details_avs.primary_stream_type;
					new_general_details.second_stream_type=general_details_avs.second_stream_type;

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&camera_parameters_avs[0];
					for(; temp1 < temp2 + (((sizeof(struct cam_avs))*4+1)/2); temp1++)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}
					for(j=0;j<1;j++)
					{
						if(safe_strncpy(new_camera_parameters[j].name,camera_parameters[j].name,9) == NULL)
							printf("camera name not copied default/last remains\n");
						new_camera_parameters[j].brightness=camera_parameters_avs[j].brightness;
						new_camera_parameters[j].contrast=camera_parameters_avs[j].contrast;
						new_camera_parameters[j].saturation=camera_parameters_avs[j].saturation;
						//mem cpy(new_camera_parameters[j].activity_data,camera_parameters_avs[j].activity_data,64);
						new_camera_parameters[j].type=camera_parameters_avs[j].type;
						new_camera_parameters[j].dome_address=camera_parameters_avs[j].dome_address;	

					}

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&new_ip_details;
					for(; temp1 < temp2 + ((sizeof(struct ip)+1)/2); temp1++)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&remote_user_avs[0];
					for(; temp1 < temp2 + (((sizeof(struct remo_avs))*16+1)/2); temp1++)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}
					for(j=0;j<4;j++)
					{
						safe_memcpy(new_remote_user[j].name, 9, remote_user_avs[j].name,9);
						//mem cpy(new_remote_user[j].name,remote_user_avs[j].name,9);
						safe_memcpy(new_remote_user[j].password, 6, remote_user_avs[j].password,6);
						//mem cpy(new_remote_user[j].password,remote_user_avs[j].password,6);
						new_remote_user[j].level=remote_user_avs[j].level;		    
					}
					new_camera_parameters[0].Lane_or_Evidence= remote_user_avs[0].covert_list[0];
					new_camera_parameters[0].DayNight_ColourMode=remote_user_avs[0].covert_list[1];
					new_camera_parameters[0].FocusAdjustDay=remote_user_avs[0].covert_list[2];
					new_camera_parameters[0].FocusAdjustNight=remote_user_avs[0].covert_list[3];
					new_camera_parameters[0].FramesPerTrigger=remote_user_avs[0].nick_name[0];	 
					new_general_details.gamma_value=remote_user_avs[0].nick_name[1];
					new_wb_details.wb_red_gain=remote_user_avs[0].wb_data_1;
					new_wb_details.wb_green1_gain=remote_user_avs[0].wb_data_2;
					new_wb_details.wb_green2_gain=remote_user_avs[1].wb_data_1;
					new_wb_details.wb_blue_gain=remote_user_avs[1].wb_data_2;
					new_wb_details.wb_red_offset=remote_user_avs[2].wb_data_1;
					new_wb_details.wb_green1_offset=remote_user_avs[2].wb_data_2;
					new_wb_details.wb_green2_offset=remote_user_avs[3].wb_data_1;
					new_wb_details.wb_blue_offset=remote_user_avs[3].wb_data_2;

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&no_checksum_data_avs;
					for(; temp1 < temp2 + ((sizeof(struct nocs_avs)+1)/2); temp1++)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}
					new_no_checksum_data.second_framerate=no_checksum_data_avs.second_framerate;
					new_no_checksum_data.second_bitrate=no_checksum_data_avs.second_bitrate;
					new_no_checksum_data.second_index=no_checksum_data_avs.second_index;
					new_no_checksum_data.configured=no_checksum_data_avs.configured;
					new_no_checksum_data.relay_out_onoff=no_checksum_data_avs.relay_out_onoff;
					new_no_checksum_data.general_details_default_loaded=no_checksum_data_avs.general_details_default_loaded;
					new_no_checksum_data.camera_parameters_default_loaded=no_checksum_data_avs.camera_parameters_default_loaded;
					new_no_checksum_data.remote_user_default_loaded=no_checksum_data_avs.remote_user_default_loaded;
					new_no_checksum_data.ip_details_default_loaded=no_checksum_data_avs.ip_details_default_loaded;
					new_no_checksum_data.schedule_default_loaded=no_checksum_data_avs.schedule_default_loaded;
					new_no_checksum_data.dydns_data_default_loaded=no_checksum_data_avs.dydns_data_default_loaded;
					new_no_checksum_data.alarm_data_default_loaded=no_checksum_data_avs.alarm_data_default_loaded;	

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&schedule_avs[0][0];
					for(; temp1 < temp2 + (((sizeof(struct sch_avs))*8+1)/2); temp1++) //host_ver:006 only one schedule used (*9 removed)
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}

					temp2 = temp1;
					eepromdataptr = (unsigned short *)&dydns_data_avs;
					for(; temp1 < temp2 + 64; temp1++)	//host_ver:006 128 word changed to 64 words
					{
						*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}
					safe_memcpy(&new_dydns_data,(sizeof (struct dydns)),&dydns_data_avs,(sizeof (struct dydns)));
					//mem cpy(&new_dydns_data,&dydns_data_avs,(sizeof (struct dydns)));

					temp2 = temp1;
					/*	    eepromdataptr = (unsigned short *)&alarm_data_avs;
					for(; temp1 < temp2 + ((sizeof(struct alrm_avs)+1)/2); temp1++)
					{
					*eepromdataptr++ = tst->rx_data_buffer[18 + temp1];
					}*/
					eepromdataptr1=(unsigned short *)malloc(32);
					if(eepromdataptr1 != NULL)
					{
					//eepromdataptr = (unsigned short *)&alarm_data_avs;
						for(mcnt=0; temp1 < temp2 + ((sizeof(struct alrm_avs)+1)/2); temp1++)
						{
						//*eepromdataptr1++ = rx_command[18 + temp1];
							eepromdataptr1[mcnt] = tst->rx_data_buffer[18 + temp1]; 
							mcnt++;
						}

						safe_memcpy(&alarm_data_avs, 32, eepromdataptr1,32);	    
						//mem cpy(&alarm_data_avs,eepromdataptr1,32);	    
						free(eepromdataptr1);	    
					}
					
					new_alarm_data.input_polarity=alarm_data_avs.input_polarity;
					new_alarm_data.input_type=alarm_data_avs.input_type;
					new_alarm_data.duration=alarm_data_avs.duration;
					new_alarm_data.enabled=alarm_data_avs.enabled;

					copy_status_flags_n_send(34, tst);
					// write protected data given by ashu,comes here, but not read
					if(tst->rx_data_buffer[17] == 0x6C)
					{
						//data_from_pc = NULL_COMMAND;
					}
					else
					{
						//data_from_pc = SAVE_AND_EXIT;
						loganevent("pcstream thrd", "configuration saving ...!");
						shared_data->ImgCapRequired = 0;
	//					DEBUG_PRINT("PcStream PC COnfig data saved----------\n");
						ipconfigdata.ip[0] = new_ip_details.ip_address[0];
						ipconfigdata.ip[1] = new_ip_details.ip_address[1];
						ipconfigdata.ip[2] = new_ip_details.ip_address[2];
						ipconfigdata.ip[3] = new_ip_details.ip_address[3];

						ipconfigdata.sm[0] = new_ip_details.subnet_mask[0];
						ipconfigdata.sm[1] = new_ip_details.subnet_mask[1];
						ipconfigdata.sm[2] = new_ip_details.subnet_mask[2];
						ipconfigdata.sm[3] = new_ip_details.subnet_mask[3];

						ipconfigdata.gw[0] = new_ip_details.gateway[0];
						ipconfigdata.gw[1] = new_ip_details.gateway[1];
						ipconfigdata.gw[2] = new_ip_details.gateway[2];
						ipconfigdata.gw[3] = new_ip_details.gateway[3];

						ipconfigdata.ds[0] = 8;
						ipconfigdata.ds[1] = 8;
						ipconfigdata.ds[2] = 8;
						ipconfigdata.ds[3] = 8;

						// verify ethernet ip settings..// return 0 on sucess, 1 on error, 2 on change(req restart)
						i = verifyIPSettings(&ipconfigdata);// return 0 on sucess, 1 on error, 2 on change(req restart)
						sync();
						saveEepromAll = 1;
						//NanoReset(pipefd);
						//DEBUG_PRINT("NanoReset from Save&Exit\n");
						//sleep(3);
						//((SHARED_RESOURCES*)tst->shmem)->app_exit = 1;
					}	
				}
				break;

			case(0x50):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
				// log download to pc 
//  int			sentlogtxt;	//0=no log.. 0x10=syslog, 0x20=kernlog, 0x30=authlog, 0x40=ufwlog, 0x50=stunnellog,.. 
//  					//0x11=syslogbak, 0x21=kernlogbak, 0x31=authlogbak,.. etc  
//  					//last 900k of file will only be sent..
					tst->sentlogtxt = tst->rx_data_buffer[18];
				}
				break;
			case(0x51):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// json Generate and download to pc 
					fptr = popen("/run/trois/bin/flash/cert_to_json.sh /etc/stunnel/certs /run/trois/bin/flash/certs_info.json", "r");
					usleep(1000);
					pclose(fptr);// close will wait for the process to terminate and return..
					sync();
					
					fptr = fopen("/run/trois/bin/flash/certs_info.json", "rb");
					if(fptr != NULL)
					{
						fseek(fptr, 0, SEEK_END);
						j = (int)ftell(fptr);
						fseek(fptr, 0, SEEK_SET);
						tst->tx_data_buffer[34] = (unsigned short)j;// size of file..
						fread(&tst->tx_data_buffer[36], 1, j, fptr);
						fclose(fptr);
						j = (j+1) & 0xfffe;
						fptr = fopen("/run/trois/bin/flash/certs_info.json.sha256", "rb");
						if(fptr != NULL)
						{
							fseek(fptr, 0, SEEK_END);
							i = (int)ftell(fptr);
							fseek(fptr, 0, SEEK_SET);
							tst->tx_data_buffer[36 + (j >> 1) + 1] = (unsigned short)i;// size of csum..
							i = fread(&tst->tx_data_buffer[36 + (j >> 1) + 2], 1, i, fptr);
							fclose(fptr);
							i = (i + 1) & 0x0FFe;
							copy_status_flags_n_send((36+ 2 +((j+i) >> 1)), tst);
						}
						else
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x51;// csum not ready
							copy_status_flags_n_send(35, tst);
						}
					}
					else
					{
						tst->tx_data_buffer[33] =  1;// else fail..
						tst->tx_data_buffer[34] =  0x50;// file not ready
						copy_status_flags_n_send(35, tst);
					}
				}
				break;
				
			case(0x52):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// CSR Generate and download to pc 
					loganevent("pcstream thrd", "csr_bundle_update start...!");
					if(tst->rx_data_buffer[19] > 254)
					{
						tst->tx_data_buffer[33] =  1;// else fail..
						copy_status_flags_n_send(34, tst);
					}
					else
					{
						//printf(" recieved sha size=%d, %x %x\n", tst->rx_data_buffer[19],  tst->rx_data_buffer[21],  tst->rx_data_buffer[22]);
						memset(version_1, 0, 256);
						mempcpy(version_1, &tst->rx_data_buffer[21],tst->rx_data_buffer[19]);// size validated.. just before..
						for(i = 0; i < (tst->rx_data_buffer[19]/2); i++)
						{
							temp1 = version_1[i*2];
							version_1[i*2] = version_1[i*2 + 1];
							version_1[i*2 + 1] = (unsigned char)temp1;
						}
						version_1[tst->rx_data_buffer[19]] = 0;// terminate string...
						//printf("data = %s\n", version_1);
						snprintf((char *)tempcharbuffer0,400,"%s /run/trois/bin/flash/generate_jetson_csr.sh /run/trois/bin/flash data.zip\n", version_1);
						//printf("cmd = %s\n", tempcharbuffer0);
						//safe _snprintf((char *)tempcharbuffer0,400,"%s /run/mtx/bin/flash/generate_jetson_csr.sh /run/mtx/bin/flash data.zip\n", version_1);
					//fptr = fopen("testcmd.bin","wb");
					//if(fptr != NULL)
					//{
					//	fwrite((char *)tempcharbuffer0, 1, 400, fptr);
					//	fclose(fptr);
					//}
						sync();
						fptr = popen((char *)tempcharbuffer0, "r");
						usleep(10000);
						sync();
						fread(tempcharbuffer0, 1, 1000, fptr);
						pclose(fptr);// close will wait for the process to terminate and return..
						sync();
					//fptr = fopen("testcmdout.bin","wb");
					//if(fptr != NULL)
					//{
					//	fwrite((char *)tempcharbuffer0, 1, 1000, fptr);
					//	fclose(fptr);
					//}
						sync();
						fptr = fopen("/run/trois/bin/flash/data.zip", "rb");
						if(fptr != NULL)
						{
							fseek(fptr, 0, SEEK_END);
							j = (int)ftell(fptr);
							fseek(fptr, 0, SEEK_SET);
							tst->tx_data_buffer[34] = (unsigned short)j;// size of file..
							fread(&tst->tx_data_buffer[36], 1, j, fptr);
							fclose(fptr);
							j = (j+1) & 0xfffe;
							fptr = fopen("/run/trois/bin/flash/data.zip.sha256", "rb");
							if(fptr != NULL)
							{
								fseek(fptr, 0, SEEK_END);
								i = (int)ftell(fptr);
								fseek(fptr, 0, SEEK_SET);
								tst->tx_data_buffer[36 + (j >> 1) + 1] = (unsigned short)i;// size of csum..
								i = fread(&tst->tx_data_buffer[36 + (j >> 1) + 2], 1, i, fptr);
								fclose(fptr);
								i = (i + 1) & 0x0FFe;
								copy_status_flags_n_send((36+ 2 +((j+i) >> 1)), tst);
							}
							else
							{
								tst->tx_data_buffer[33] =  1;// else fail..
								tst->tx_data_buffer[34] =  0x51;// csum not ready
								copy_status_flags_n_send(35, tst);
							}
						}
						else
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x50;// file not ready
							copy_status_flags_n_send(35, tst);
						}
					}
				}
				break;

			case(0x53):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// CSRbundle rx from pc 
					if(tst->rx_data_buffer[19] > 254)
					{
						tst->tx_data_buffer[33] =  1;// else fail..
						copy_status_flags_n_send(34, tst);
					}
					else
					{
						memset(version_1, 0, 256);
						safe_memcpy(version_1, 256, &tst->rx_data_buffer[21],tst->rx_data_buffer[19]);
						for(i = 0; i < (tst->rx_data_buffer[19]/2); i++)
						{
							temp1 = version_1[i*2];
							version_1[i*2] = version_1[i*2 + 1];
							version_1[i*2 + 1] = (unsigned char)temp1;
						}
						version_1[tst->rx_data_buffer[19]] = 0;// terminate string...
						i = (tst->rx_data_buffer[19] + 1) >> 1;
						j = tst->rx_data_buffer[21 + i + 1];
						if(safe_memcpy(tempcharbuffer1, 24*1024, &tst->rx_data_buffer[21 + i + 3],j) <= 0)
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							copy_status_flags_n_send(34, tst);
						}
						else
						{
							for(i = 0; i < (j/2); i++)
							{
								temp1 = tempcharbuffer1[i*2];
								tempcharbuffer1[i*2] = tempcharbuffer1[i*2 + 1];
								tempcharbuffer1[i*2 + 1] = (unsigned char)temp1;
							}
							fptr = fopen("tmp/csrbundle.zip","wb");
							if(fptr != NULL)
							{
								fwrite((char *)tempcharbuffer1, 1, j, fptr);
								fclose(fptr);
							}
							sync();
							
							snprintf((char *)tempcharbuffer0,400,"%s /run/trois/bin/flash/csr_bundle_update.sh /run/trois/bin/flash/tmp csrbundle.zip verify\n", version_1);// deploy
							fptr = fopen("testcmd53.bin","wb");
							if(fptr != NULL)
							{
								fwrite((char *)tempcharbuffer0, 1, 400, fptr);
								fclose(fptr);
							}
							sync();
							fptr = popen((char *)tempcharbuffer0, "r");
							usleep(10000);
							sync();
							fread(tempcharbuffer0, 1, 1000, fptr);
							pclose(fptr);// close will wait for the process to terminate and return..
							sync();
							fptr = fopen("testcmdout53.bin","wb");
							if(fptr != NULL)
							{
								fwrite((char *)tempcharbuffer0, 1, 1000, fptr);
								fclose(fptr);
							}
							sync();
							fptr = fopen("/run/trois/bin/flash/tmp/certs_verify.success", "rb");
							if(fptr != NULL)
							{
								loganevent("pcstream thrd", "csr_bundle_update verify...!");
								copy_status_flags_n_send(34, tst);
							}
							else
							{
								tst->tx_data_buffer[33] =  1;// else fail..
								tst->tx_data_buffer[34] =  0x50;// file not ready
								copy_status_flags_n_send(35, tst);
							}
						}
					}
				}
				break;

			case(0x54):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// CSR Generate and download to pc 
					if(tst->rx_data_buffer[19] > 254)
					{
						tst->tx_data_buffer[33] =  1;// else fail..
						copy_status_flags_n_send(34, tst);
					}
					else
					{
						memset(version_1, 0, 256);
						safe_memcpy(version_1, 256, &tst->rx_data_buffer[21],tst->rx_data_buffer[19]);// size validated.. just before..
						//mem cpy(version_1, &tst->rx_data_buffer[21],tst->rx_data_buffer[19]);
						for(i = 0; i < (tst->rx_data_buffer[19]/2); i++)
						{
							temp1 = version_1[i*2];
							version_1[i*2] = version_1[i*2 + 1];
							version_1[i*2 + 1] = (unsigned char)temp1;
						}
						version_1[tst->rx_data_buffer[19]] = 0;// terminate string...
						snprintf((char *)tempcharbuffer0,400,"%s /run/trois/bin/flash/csr_bundle_update.sh /run/trois/bin/flash/tmp csrbundle.zip deploy\n", version_1);
//					fptr = fopen("testcmd54.bin","wb");
//					if(fptr != NULL)
//					{
//						fwrite((char *)tempcharbuffer0, 1, 400, fptr);
//						fclose(fptr);
//					}
						sync();
						fptr = popen((char *)tempcharbuffer0, "r");
						usleep(10000);
						sync();
						fread(tempcharbuffer0, 1, 1000, fptr);
						pclose(fptr);// close will wait for the process to terminate and return..
						sync();
//					fptr = fopen("testcmdout54.bin","wb");
//					if(fptr != NULL)
//					{
//						fwrite((char *)tempcharbuffer0, 1, 1000, fptr);
//						fclose(fptr);
//					}
						sync();
						fptr = fopen("/run/trois/bin/flash/tmp/certs_deploy.success", "rb");
						if(fptr != NULL)
						{
							loganevent("pcstream thrd", "csr_bundle_update_sucess ...!");
//						fptr = popen("/run/mtx/bin/flash/csr_bundle_update_sucess.sh", "r");
//						usleep(10000);
//						sync();
//						fread(tempcharbuffer0, 1, 1000, fptr);
//						pclose(fptr);// close will wait for the process to terminate and return..
//						sync();
//						fptr = fopen("testaftercmdout54.bin","wb");
//						if(fptr != NULL)
//						{
//							fwrite((char *)tempcharbuffer0, 1, 1000, fptr);
//							fclose(fptr);
//						}
//						sync();
							copy_status_flags_n_send(34, tst);
						}
						else
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x50;// file not ready
							copy_status_flags_n_send(35, tst);
						}
					}
				}
				break;

			case(0x55):
				if(tst->level == GUEST)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
					copy_status_flags_n_send(35, tst);
				}
				else
				{
					// data for serial authentication
					safe_memcpy((unsigned char *)shared_data->capturecmdauthdata2, 1024, &tst->rx_data_buffer[18],1024);// size already valiated..
					//display_buffer((unsigned char *)shared_data->capturecmdauthdata2, 128);
					shared_data->newcapturecmdauthdata = 1;
					copy_status_flags_n_send(34, tst);
				}
				break;


			case(0x6D):
				if(systemPGMMode == tst->sock_number)
				{
					// remote setup - EXIT WITHOUT SAVING FROM PC
					systemPGMMode = 99;// no user..
					verifyEepromAll = 1;
	//				printf("case(0x6D):\n");
	//				DEBUG_PRINT("case(0x6D): \n");
					//NanoReset(pipefd);
	//				DEBUG_PRINT("NanoReset from Exit without Saving\n");
					shared_data->SystemReset = 1;
				}
				break;
				
// isp commands 0x9A file data with offset
//							0x9B checksum of current file reply fail and delete file if error
//							0x9C copy to flash and reset..


			case 0x74:// Time set from VPU/PC..
				if((tst->rx_data_buffer[25] == 0x5555) && (tst->rx_data_buffer[26] == 0xAAAA) && (tst->rx_data_buffer[27] == 0x22AA))
				{
//					DEBUG_PRINT("VPU time set command = %d:%d:%d %d-%d-%d\n", tst->rx_data_buffer[20], tst->rx_data_buffer[19], tst->rx_data_buffer[18], tst->rx_data_buffer[21], tst->rx_data_buffer[22], tst->rx_data_buffer[23]);
					shared_data->setrtctimecode[0] = (unsigned char)tst->rx_data_buffer[18];
					shared_data->setrtctimecode[1] = (unsigned char)tst->rx_data_buffer[19];
					shared_data->setrtctimecode[2] = (unsigned char)tst->rx_data_buffer[20];
					shared_data->setrtctimecode[3] = (unsigned char)tst->rx_data_buffer[21];
					shared_data->setrtctimecode[4] = (unsigned char)tst->rx_data_buffer[22];
					shared_data->setrtctimecode[5] = (unsigned char)tst->rx_data_buffer[23];
					tmp_time_st.tm_sec = shared_data->setrtctimecode[0];
					tmp_time_st.tm_min = shared_data->setrtctimecode[1];
					tmp_time_st.tm_hour = shared_data->setrtctimecode[2]; 
					tmp_time_st.tm_mday = shared_data->setrtctimecode[3];
					tmp_time_st.tm_mon = shared_data->setrtctimecode[4];
					tmp_time_st.tm_year = shared_data->setrtctimecode[5];
					tmp_time_st.tm_isdst = 0;
					tmp_ltime0 = mktime(&tmp_time_st);
					
					gettimeofday(&tval,NULL);// for timing...
					localtime_r(&tval.tv_sec, &brokentime);
					drift = (int)tval.tv_sec - (int)tmp_ltime0;
					if(abs(drift) > 1)
						shared_data->settime_flag = 1;
//					printf("CAMERA time = %d:%d:%d %d-%d-%d drift = %d\n", brokentime.tm_hour, brokentime.tm_min, brokentime.tm_sec, brokentime.tm_mday, brokentime.tm_mon, brokentime.tm_year, drift);
					tst->tx_data_buffer[34] = 0x5555;
					tst->tx_data_buffer[35] = 0xAAAA;
					*((unsigned int*)(&tst->tx_data_buffer[36])) = drift;
					tst->tx_data_buffer[38] = 0xAA22;
					tst->tx_data_buffer[39] = battery_low;
					tst->tx_data_buffer[40] = power_loss;
					tst->tx_data_buffer[41] = brokentime.tm_sec;
					tst->tx_data_buffer[42] = brokentime.tm_min;
					tst->tx_data_buffer[43] = brokentime.tm_hour;
					tst->tx_data_buffer[44] = brokentime.tm_wday;
					tst->tx_data_buffer[45] = brokentime.tm_mday;
					tst->tx_data_buffer[46] = brokentime.tm_mon;
					tst->tx_data_buffer[47] = brokentime.tm_year;// from 1900
					battery_low = 0;
					power_loss = 0;
					copy_status_flags_n_send(48, tst);
				}
				else
				{
//					shared_data->settime_flag = 1;
					tst->tx_data_buffer[33] =  1;// else fail..
					copy_status_flags_n_send(34, tst);
				}
				break;

			case 0x9A:// ISP file write with offset..
				if(tst->level != ADMIN)
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7F;// not admin.
				}
				else
				{
					tst->streaming_flag = 0;
	//				DEBUG_PRINT("PcStream ISP file write cmd size 9A with --------\n");
	//				printf("PcStream ISP file write cmd size 9A with --------\n");
								
					fdataptr = tst->rx_data_buffer[19];
					fdataptr |= (tst->rx_data_buffer[18] << 16);
					i = tst->rx_data_buffer[20];
	//				DEBUG_PRINT("PcStream ISP file write size %d with offset %d----\n", i, fdataptr);
	//				printf("PcStream ISP file write size %d with offset %d----\n", i, fdataptr);
					if(shared_data->FwUpgrade == 0)
					{
						loganevent("pcstream thrd", "firmware Upgrade entered ...!");
						shared_data->FwUpgrade = tst->sock_number+1;
						if((i > 0) && (fdataptr < (32*1024*1024)))
						{
			   				ispfileptr = fopen("/dev/shm/ispfile","wb");
			   				if(ispfileptr != NULL)
			   				{
			   					fseek(ispfileptr, fdataptr, SEEK_SET);
			   					fwrite(&tst->rx_data_buffer[21], i, 1, ispfileptr);
			   					fclose(ispfileptr);
			   				}
						}
						else
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x8E;// start error .
						}
					}
					else if(shared_data->FwUpgrade == tst->sock_number+1)
					{
						if((i > 0) && (fdataptr < (32*1024*1024)))
						{
			   				ispfileptr = fopen("/dev/shm/ispfile","ab+");
			   				if(ispfileptr != NULL)
			   				{
			   					fseek(ispfileptr, fdataptr, SEEK_SET);
			   					fwrite(&tst->rx_data_buffer[21], i, 1, ispfileptr);
			   					fclose(ispfileptr);
			   				}
						}
						else
						{
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x8F;// size error .
						}
					}
					else
					{
						tst->tx_data_buffer[33] =  1;// else fail..
						tst->tx_data_buffer[34] =  0x7E;// not user already in...
					}
				}
				copy_status_flags_n_send(35, tst);
				break;

			case 0x9B:// ISP verify checksum of file..
//				DEBUG_PRINT("PcStream ISP file write cmd size 9B with --------\n");
//				printf("PcStream ISP file write cmd size 9B with --------\n");
				if(shared_data->FwUpgrade == tst->sock_number+1)
				{
					csum = 0;
					ispfileptr = fopen("/dev/shm/ispfile","rb");
					if(ispfileptr != NULL)
					{
//   					fseek(ispfileptr, -8, SEEK_END);
//						fread(tempid,8,1,ispfileptr);
//						DEBUG_PRINT("file read firmware = %s\n", (char*)tempid);
//  					fseek(ispfileptr, 0, SEEK_SET);
						while(1)
						{
		 					readcount = fread(meta_data_buf, 1, 4096, ispfileptr);
			 				if((readcount <= 0) || (readcount > 4096))
			 					break;
			 				else
			 				{
				 				for(i = 0; i < readcount; i++)
			 						csum += meta_data_buf[i];
			 				}
			 			}
//					DEBUG_PRINT("PcStream ISP rx csum = %x, file csum = %x\n", tst->rx_data_buffer[18], csum);
//					printf("PcStream ISP rx csum = %x, file csum = %x\n", tst->rx_data_buffer[18], csum);				
						if(csum == tst->rx_data_buffer[18])
						{
/*
Version2 packing...
filename:- FWCV-240.01-01.01.1.bin
            |    |   |  |  | |  |-------New extension used
            |    |   |  |  | |----------Patch / Fix number max 9, (0-9)
            |    |   |  |  |------------Minor Revisions (0-99) 
            |    |   |  |---------------Major Revisions (0-99)
            |    |   |------------------Type / Varients(5M / 8M / ANPR / RLVD / AI only/ Orin Nano4G / Orin Nano8G/ Orin Nx)
            |    |----------------------Model firmware ID (for Jetson Orin Based Camera)
            |---------------------------Comapny Id as Prefix.
*/
/*
|0--------------------|                          |0--------------------|                            
| firmware.gz         |                          | Encrypted data      |                            
|                     |                          |                     |                            
|                     |	-> Sign using        --> |                     |     
|---------------------|    PrivateKey            |                     |                            
|8Byte IDs 32Byte name|    add signature         |---------------------|                            
|---------------------|    as 256 byte footer    |   256 byte Footer   |                           
 hash also embedded                              |---------------------|
                                                                                   
256 byte footer will have data filled with junk, 
starts with header (8byte ID0 followed by signature offset(32bit) and length(8bit), Signature 
then filename statrs @ offset 192
*/
							id[0] = 0x73519483;
							id[1] = 0x63213748;
//						now check for ids.
							fseek(ispfileptr, -256, SEEK_END);
							fread(&tempid[0],1,4,ispfileptr);
							fread(&tempid[1],1,4,ispfileptr);
							if((id[0] != tempid[0]) || (id[1] != tempid[1]))
							{
								loganevent("pcstream thrd", "FWU id check fail ...!");
								printf(" id fail.     \n");
								tst->tx_data_buffer[33] =  1;// no matching ids, fail..
								tst->tx_data_buffer[34] =  0x80;// no matching ids, fail..
								shared_data->SystemReset = 20;
								fclose(ispfileptr);
								shared_data->FwUpgrade = 0;
							}
							else
							{
/*====================================================================================================================================*/					
// firmware update new process...					
								fread(&version_1[8], 1, 248, ispfileptr);
								indatasize = (*(unsigned int *)&version_1[8]) - 16;// signature offset and file size to save
								sig_len = version_1[15];
								// read versions...
								//mem cpy(ver, &version_1[192], 32);// saving filename...
								//
								data = (unsigned char *)malloc(indatasize+1024);
								if(data != NULL)
								{
									memset(data, 0, indatasize+256);
									fseek(ispfileptr, 0, SEEK_SET);
									i = fread(data, 1, indatasize, ispfileptr);
									fclose(ispfileptr);
								}
								// get public key
								decrypt_aes_gcm_embtifile((char *)keyfilenameenc, (char *)keyfilename, Aeskey);
								// verify signature
								pubkey_file = fopen((char *)keyfilename, "r");
								if (!pubkey_file) 
								{
									loganevent("pcstream thrd", "FWU no/ key rd error. ...!");
									printf(" no/ key rd error.     \n");
									tst->tx_data_buffer[33] =  1;// else fail..
									tst->tx_data_buffer[34] =  0x82;// key rd error..
									shared_data->SystemReset = 20;
									//printf("PcStream ISP csum fail\n");	
									free(data);					
									shared_data->FwUpgrade = 0;
								}
								else
								{
									result = 0;
									printf(" checking Signature      \n");
									if(sig_len < 128)
									{
										signature = (unsigned char *)malloc(sig_len);
										if(signature != NULL)
										{
											safe_memcpy(signature, sig_len, &version_1[16], sig_len);// size already verified..
											public_key = PEM_read_PUBKEY(pubkey_file, NULL, NULL, NULL);
											fclose(pubkey_file);
											result = verify_signature(data, indatasize, signature, sig_len, public_key);
											EVP_PKEY_free(public_key);
											pubkey_file = fopen((char *)keyfilename, "w");
											if(pubkey_file != NULL) 
											{
												fwrite(signature, 1, sig_len, pubkey_file);
												fclose(pubkey_file);
											}
											free(signature);
										}
									}
									if (result == 1) 
									{
										printf(" - Signature is valid.     \n");
										// do decryption and check id onecmore...
										fptr = fopen(FILENAME, "rb");
										if(fptr != NULL)
										{
											fread(tempcharbuffer0, 1, 4096, fptr);
											fclose(fptr);
										}
										j = 0;
										i = *(unsigned int*)&tempcharbuffer0[0x864];// second slot..
										//printf("i = %08x\n", i);
										if(i)
										{
											j = (i >> 1) % 100000000;
										}
										snprintf((char *)keystr,10, "%08u", j);
										//if(safe_ <= 0)
										//	printf("stored value not set\n");
										//snp rintf((char *)tempcharbuffer0,220,"pkcs11-tool --module /usr/lib/libckteec.so --login --pin %s --slot %d --id %04d --mechanism SHA512-RSA-PKCS --sign --input-file /run/mtx/bin/flash/firmware1.gz --output-file /run/mtx/bin/flash/firmware1.sig\n", keystr ,1, idvar);
										//set env("MSFS10", (char *)tempcharbuffer0, 1);
										fptr = fopen(APP_NAME_ENC,"wb");// save for Decryption..
										if(fptr != NULL)
										{
											i = fwrite(data, 1, indatasize, fptr);
											fclose(fptr);
										}
										decrypt_aes_gcm_embtifile((char *)APP_NAME_ENC, (char *)APP_NAME, Aeskey);
										// checking with prev version and do anti roll back.......
										i = indatasize;// size read will be less
										fptr = fopen(APP_NAME,"rb");// save for Decryption..
										if(fptr != NULL)
										{
											indatasize = fread(data, 1, i, fptr);
											fclose(fptr);
										}
										if(safe_memcpy(ver, 64, &data[indatasize - (80 - 8)], 32) <= 0)
											printf(" name not copied.. will be verified below\n");
										// 
										printf(" 0 old=%s  new=%s, firmware %s\n", fver, ver, FirmwareV2);
										if(safe_memcpy(fver, 64, FirmwareV2, 32) <= 0)
											printf(" name not copied.. will be verified below\n");
										memset(&oldv, 0, sizeof(oldv));
										printf(" 1 old=%s  new=%s, firmware %s\n", fver, ver, FirmwareV2);
										if(mtx_sc_anf(fver, "%4s-%3d.%2d-%2d.%2d.%1d.%3s", oldv.Prefix, &oldv.Model, &oldv.Type, &oldv.MVer, &oldv.mVer, &oldv.Patch, oldv.extn) <7)
											printf(" filename read encountered error\n");// this error will be managed in validation below..
										memset(&newv, 0, sizeof(newv));
										if(mtx_sc_anf(ver, "%4s-%3d.%2d-%2d.%2d.%1d.%3s", newv.Prefix, &newv.Model, &newv.Type, &newv.MVer, &newv.mVer, &newv.Patch, newv.extn) < 7)
										{
											printf("new filename read encountered error\n");// this error will be managed in validation below..
										}
										i = validate_ver(oldv,newv);// returns 0 if ok else error no. 1=No Prefix, 2=Mode/Type err, 3=No ext, 4=old ver
										if(i==0)
										{
											//printf(" signing file..  \n");
											safe_signwith_cskey(keystr);
											//fptr = pop en("$MSFS10", "r");// will execute the commands in string..
											//snp rintf((char *)tempcharbuffer0,220,"-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------");
											//set env("MSFS10", (char *)tempcharbuffer0, 1);
											//pcl ose(fptr);// close will wait for the process to terminate and return..
											loganevent("pcstream thrd: FWU ok. signed :", (const char *)ver);
											//printf("PcStream: PC firmware signing Done!----------\n");		
											free(data);		
											// install file as firmware1.gz and sign with signature as firmware1.sig
											// run script to install..
											//printf(" store file....  \n");
										}
										else
										{
											// older version roll back Error...
											loganevent("pcstream thrd: FWU version.", (const char *)ver);
											loganevent("pcstream thrd", "FWU Roll back error....!");
											printf(" Roll back error.. old=%s  new=%s, firmware %s\n", fver, ver, FirmwareV2);
											//display_buffer((unsigned char *)FirmwareV2, 32);
											tst->tx_data_buffer[33] =  1;// else fail..
											tst->tx_data_buffer[34] =  0x86;// roll back Error...
											shared_data->SystemReset = 20;
										}
											
									}
									else
									{
										loganevent("pcstream thrd", "FWU Invalid signature....!");
										printf(" Invalid signature..		 \n");
										free(data);					
										tst->tx_data_buffer[33] =  1;// else fail..
										tst->tx_data_buffer[34] =  0x83;// Invalid signature..	
										shared_data->SystemReset = 20;
										shared_data->FwUpgrade = 0;
									}
								}
								
/*====================================================================================================================================*/					
							}
						}
						else
						{
							loganevent("pcstream thrd", "FWU file corrupt....!");
							printf(" PC csum fail..		 \n");
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x87;//PC csum fail
							//shared_data->SystemReset = 20;
//						printf("PcStream ISP csum fail\n");						
							fclose(ispfileptr);
							shared_data->FwUpgrade = 0;
						}
					}
					else
					{
						loganevent("pcstream thrd", "FWU file error.Restart.!");
						printf(" uploaded file error..		 \n");
						tst->tx_data_buffer[33] =  1;// else fail..
						tst->tx_data_buffer[34] =  0x88;//uploaded file error
						//remove("/dev/shm/ispfile");
						shared_data->SystemReset = 20;
	//					printf("PcStream ISP csum fail\n");						
					}
				}
				else
				{
					loganevent("pcstream thrd", "FWU user Not admin...!");
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7E;// not user already in...
				}
				copy_status_flags_n_send(35, tst);
				break;
/*====================================================================================================================================*/					
				
			case 0x9C:// ISP file copy and reset..
//				DEBUG_PRINT("PcStream ISP file write cmd size 9C with --------\n");
				if(shared_data->FwUpgrade == tst->sock_number+1)
				{
					//printf("PcStream ISP file write cmd size 9C with --------\n");				
					ispfileptr = fopen(APP_NAME,"rb");
					if(ispfileptr != NULL)// if valid copy else reply fail
					{
						id[0] = 0x73519483;
						id[1] = 0x63213748;
						fseek(ispfileptr, -80, SEEK_END);
						fread(&tempid[0],1,4,ispfileptr);
						fread(&tempid[1],1,4,ispfileptr);
						fclose(ispfileptr);
						if((id[0] != tempid[0]) || (id[1] != tempid[1]))
						{
							loganevent("pcstream thrd", "FWU file corrupted...!");
							tst->tx_data_buffer[33] =  1;// else fail..
							tst->tx_data_buffer[34] =  0x89;//save file error
							copy_status_flags_n_send(35, tst);
		//					DEBUG_PRINT("PcStream PC firmware update fail----------\n");
							printf("PcStream PC firmware update fail----------\n");		
							shared_data->SystemReset = 20;					
						}
						else
						{
							shared_data->ImgCapRequired = 0;
							//sn printf((char *)tempcharbuffer0,200,"mv /usr/Camera/firmware.gz /usr/Camera/firmwareBKUP.gz ;sync;cp -af /run/mtx/bin/flash/firmware1.gz /usr/Camera/firmware.gz ;cp -af /run/mtx/bin/flash/firmware1.sig /usr/Camera/firmware.sig ;sync\n");
							safe_bkupNcopy_fw();
		 					//rename("/usr/Camera/firmware1.gz", "usr/Camera/firmware.gz");
							printf("PcStream: PC firmware update Done!----------\n");		
		 					sync();

							loganevent("pcstream thrd", "FWU completed...!");
								
							copy_status_flags_n_send(34, tst);
							usleep(100000);
							sync();
							DeadDelayMS(990);
							//((SHARED_RESOURCES*)tst->shmem)->app_exit = 1;
		//					DEBUG_PRINT("PcStream PC firmware update done----------\n");
		//					printf("PcStream PC firmware update done----------\n");	
							shared_data->SystemReset = 20;	
						}			
					}
					else
					{
						loganevent("pcstream thrd", "FWU file not found...!");
						tst->tx_data_buffer[33] =  1;// else fail..
						tst->tx_data_buffer[34] =  0x89;//save file error
						copy_status_flags_n_send(35, tst);
	//					DEBUG_PRINT("PcStream PC firmware update fail----------\n");
						printf("PcStream PC firmware update fail----1------\n");		
						shared_data->SystemReset = 20;	
						shared_data->FwUpgrade = 0;				
					}
				}
				else
				{
					tst->tx_data_buffer[33] =  1;// else fail..
					tst->tx_data_buffer[34] =  0x7E;// not user already in...
				}
				break;
	
			default:
//				DEBUG_PRINT("PcStream rxed full command size %d %x %x %x %x\n", expected_len[tst->sock_number], tst->rx_data_buffer[0], tst->rx_data_buffer[17], tst->rx_data_buffer[18], tst->rx_data_buffer[19]);
				break;
		
			}
		}
	}
}

void process_tx(struct tst *tst)
{
	static const unsigned short command[4] = {0x2C,0x55, 0, 0};
	static const unsigned char 	meta_data_buf[4096] = {0};
	static const unsigned char 	zero_buff[MAX_SOCKET_DATA_TR] = {0};
	SHARED_RESOURCES 		*shared_data;
	COMPRESS_PC_SHARED_RESOURCES	*compressPcShr;
	unsigned int 			imgsize, tx_size, zero_size, temp1, i, j, k;
	unsigned long 			tx_short_size;
//	static unsigned short 		prev_img_no = 0;
	struct disk_fat 		image_fat;
//	unsigned char 			outputjpegimg[256*1024];
	char 				DebugStr[1024];
	int 				DebugStrSize;
	unsigned short			socket_status_template[32];
	unsigned char			encbuffer0[1024+256], encbuffer1[1024+256];
	FILE				*fptr;

	shared_data = (SHARED_RESOURCES *)tst->shmem;
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES *)tst->shmemcopc;

	if(tst->sentlogtxt)
	{
		tx_size = 0;
		fptr = NULL;
//  int			sentlogtxt;	//0=no log.. 0x10=syslog, 0x20=kernlog, 0x30=authlog, 0x40=ufwlog, 0x50=stunnellog,.. 
//  					//0x11=syslogbak, 0x21=kernlogbak, 0x31=authlogbak,.. etc  
//  					//last 900k of file will only be sent..
		switch(tst->sentlogtxt)
		{
			case 0x10:// send syslog
				//
				fptr = fopen("/var/log/syslog","r");
				break;

			case 0x11:// send syslog
				//
				fptr = fopen("/var/log/syslog.bak","r");
				break;

			case 0x20:// send syslog
				//
				fptr = fopen("/var/log/kern.log","r");
				//
				break;
			case 0x21:// send syslog
				//
				fptr = fopen("/var/log/kern.log.bak","r");
				//
				break;
				
			case 0x30:// send syslog
				//
				fptr = fopen("/var/log/auth.log","r");
				//
				break;
			case 0x31:// send syslog
				//
				fptr = fopen("/var/log/auth.log.bak","r");
				//
				break;
				
			case 0x40:// send syslog
				//
				fptr = fopen("/var/log/stunnel.log","r");
				//
				break;
			case 0x41:// send syslog
				//
				fptr = fopen("/var/log/stunnel.log.bak","r");
				//
				break;
				
			case 0x50:// send syslog
				//
				fptr = fopen("/var/log/ufw.log","r");
				//
				break;
			case 0x51:// send syslog
				//
				fptr = fopen("/var/log/ufw.log.bak","r");
				//
				break;
		}
		imgsize = 0;
		if(fptr != NULL)
		{
			fseek(fptr, 0, SEEK_END);
			tx_size = ftell(fptr);
			if(tx_size > 900*1024)
				imgsize = 900*1024;
			else
				imgsize = tx_size;
			DebugStrSize = tx_size - imgsize;
			fseek(fptr, DebugStrSize, SEEK_SET);
			tx_size = 64 + 4 + imgsize + 6;
		}
		else
		{
			// no file ...
			tx_size = 64 + 4 + 6;
		}
		tx_short_size = tx_size;
		tx_size += (MAX_SOCKET_DATA_TR -1);
		tx_size &= MAX_TX_DATA_SIZE_MASK;
		zero_size = tx_size - tx_short_size;
		if((MAXREMOTE_TX_DATA_FIFO_LENGTH - tst->remote_tx_fifo->filled_length) >= tx_size)
		{
//			printf("-----------------------LOG READ-------------------------------------------------\n");
			update_pc_status(tst->tx_data_buffer);
			tst->tx_data_buffer[7] = 0;//2;// Enc type.. 
			tst->tx_data_buffer[SEQUENCE_NO] = sequence_no[tst->sock_number]++;
			tst->tx_data_buffer[PACKET_LENGTH] = tx_short_size;
			tst->tx_data_buffer[PACKET_LENGTH + 1] = tx_short_size >> 16;
			tst->tx_data_buffer[20] = tst->sock_number;
			tst->tx_data_buffer[29] = (((unsigned char)(shared_data->lux)) << 8);
			tst->tx_data_buffer[32] =  0x50;// 
			tst->tx_data_buffer[33] =  0x55;
			tst->tx_data_buffer[34] =  tst->sentlogtxt;
			tst->tx_data_buffer[35] = (unsigned short)imgsize;
			tst->tx_data_buffer[36] = (unsigned short)(imgsize >> 16);
			if(imgsize)
			{
				fread(&tst->tx_data_buffer[37], 1, imgsize, fptr);
				fclose(fptr);
			}
			fifo_write(tst->remote_tx_fifo, (unsigned char*)&(tst->tx_data_buffer[0]), tx_short_size*2);
			if(zero_size > 0)
				fifo_write(tst->remote_tx_fifo, (unsigned char *)zero_buff, zero_size);
			tst->sentlogtxt = 0;
//			printf("-----------------------LOG FINISH-------------------------------------------------\n");
		}
		//else wait for space..
	}
	else if(tst->streaming_flag)
	{
		if(tst->streamSelect >= 4)
			tst->streamSelect = 0;		
		j = tst->streamSelect;
		for(i = 0; i <= 3; i++)
		{
			if(compressPcShr->PcImageStreamReq[j][tst->sock_number])
				break;
		}
		tst->streamSelect++;
		if(tst->streamSelect >= 4)
			tst->streamSelect = 0;		
		if(compressPcShr->PcImageInit[j][tst->sock_number] == 1) 
			compressPcShr->PcImageInit[j][tst->sock_number] = 2;
		if(compressPcShr->PcImageInit[j][tst->sock_number] == 3)// if pipeline ready...
		{
			i = compressPcShr->PcImageRdNo[j][tst->sock_number];
			if(compressPcShr->PcImageStatus[j][i][tst->sock_number] == IMGRDY)
			{
				imgsize = compressPcShr->PcImageSize[j][i];
//				printf("img ready stream %d size = %d\n", j, imgsize);
				safe_memcpy(&image_fat, sizeof(struct disk_fat ), compressPcShr->CameraImageHeaderData[j][i], sizeof(struct disk_fat ));// size already ok.
//				if(j)
//					display_buffer(compressPcShr->PcImageOutLMemory[i], 32);
//				else
//					display_buffer(compressPcShr->PcImageOutHMemory[i], 32);
				tx_size = imgsize + sizeof(struct disk_fat) + sizeof(socket_status_template) + sizeof(command) + sizeof(meta_data_buf);
				tx_short_size = tx_size;
				tx_size += (MAX_SOCKET_DATA_TR -1);
				tx_size &= MAX_TX_DATA_SIZE_MASK;
				zero_size = tx_size - tx_short_size;
				tx_short_size = tx_size/2;
				if((MAXREMOTE_TX_DATA_FIFO_LENGTH - tst->remote_tx_fifo->filled_length) >= tx_size)
//				if(((MAXREMOTE_TX_DATA_FIFO_LENGTH - tst->remote_tx_fifo->filled_length) >= tx_size) && (tst->remote_tx_fifo->filled_length < (MAXREMOTE_TX_DATA_FIFO_LENGTH/4)))
				{
//					printf("----------image---------------------------------\n");
//					if(j == 1)
//					{
//						DEBUG_PRINT("%s\n", compressPcShr->PcImageWrLog[j][i]);
//					}
					//printf("send size final= %d\n", tx_short_size);
					//DEBUG_PRINT("PcStream %d send img to pc size = %d from buf %d data size to tx %ld fill %d\n", tst->sock_number, imgsize, shared_data->PcImageRdNo[tst->sock_number], tx_short_size, zero_size);
					tst->imgstxed++;
					update_pc_status(socket_status_template);
					socket_status_template[SEQUENCE_NO] = sequence_no[tst->sock_number]++;
					socket_status_template[PACKET_LENGTH] = tx_short_size;
					socket_status_template[PACKET_LENGTH + 1] = tx_short_size >> 16;
					socket_status_template[20] = tst->sock_number;
					socket_status_template[29] = (((unsigned char)(shared_data->lux)) << 8);
					socket_status_template[7] = 0;//3;// Enc type.. 

//					safe_mem cpy(encbuffer0, 2048, (unsigned char *)&(socket_status_template[9]), 46);// size will not fail..
//					safe_mem cpy(&(encbuffer0[46]), (2048 - 46), (unsigned char *)command, 8);// size will not fail..
//					safe_mem cpy(&(encbuffer0[46+8]), (2048 - 46 -8), (unsigned char *)(&image_fat), sizeof(struct disk_fat));// size will not fail..
//					if(j == 0)
//						safe_mem cpy(&encbuffer0[46+8+sizeof(struct disk_fat)], (2048 - 46 -8 -128), (unsigned char *)compressPcShr->PcImageOutHMemory[i], (1024 - (46+8+sizeof(struct disk_fat))));// size will not fail..
//					else if(j == 1)
//						safe_mem cpy(&encbuffer0[46+8+sizeof(struct disk_fat)], (2048 - 46 -8 -128), (unsigned char *)compressPcShr->PcImageOutLMemory[i], (1024 - (46+8+sizeof(struct disk_fat))));// size will not fail..
//					else if(j == 2)
//						safe_mem cpy(&encbuffer0[46+8+sizeof(struct disk_fat)], (2048 - 46 -8 -128), (unsigned char *)compressPcShr->PcImageOutELMemory[i], (1024 - (46+8+sizeof(struct disk_fat))));// size will not fail..
//					else
//						safe_mem cpy(&encbuffer0[46+8+sizeof(struct disk_fat)], (2048 - 46 -8 -128), (unsigned char *)compressPcShr->PcImageOutEHMemory[i], (1024 - (46+8+sizeof(struct disk_fat))));// size will not fail..
//					fifo_write(tst->remote_tx_fifo, (unsigned char *)socket_status_template, 18);
//					fifo_write(tst->remote_tx_fifo, encbuffer0, 1024);
//					if(j == 0)
//						fifo_write(tst->remote_tx_fifo, (unsigned char *)&compressPcShr->PcImageOutHMemory[i][(1024 - (46+8+sizeof(struct disk_fat)))], imgsize - (1024 - (46+8+sizeof(struct disk_fat))));
//					else if(j == 1)
//						fifo_write(tst->remote_tx_fifo, (unsigned char *)&compressPcShr->PcImageOutLMemory[i][(1024 - (46+8+sizeof(struct disk_fat)))], imgsize - (1024 - (46+8+sizeof(struct disk_fat))));
//					else if(j == 2)
//						fifo_write(tst->remote_tx_fifo, (unsigned char *)&compressPcShr->PcImageOutELMemory[i][(1024 - (46+8+sizeof(struct disk_fat)))], imgsize - (1024 - (46+8+sizeof(struct disk_fat))));
//					else
//						fifo_write(tst->remote_tx_fifo, (unsigned char *)&compressPcShr->PcImageOutEHMemory[i][(1024 - (46+8+sizeof(struct disk_fat)))], imgsize - (1024 - (46+8+sizeof(struct disk_fat))));

					fifo_write(tst->remote_tx_fifo, (unsigned char *)socket_status_template, sizeof(socket_status_template));
					fifo_write(tst->remote_tx_fifo, (unsigned char *)command, sizeof(command));
					fifo_write(tst->remote_tx_fifo, (unsigned char *)(&image_fat), sizeof(struct disk_fat));
					if(j == 0)
						fifo_write(tst->remote_tx_fifo, (unsigned char *)compressPcShr->PcImageOutHMemory[i], imgsize);
					else if(j == 1)
						fifo_write(tst->remote_tx_fifo, (unsigned char *)compressPcShr->PcImageOutLMemory[i], imgsize);
					else if(j == 3)
						fifo_write(tst->remote_tx_fifo, (unsigned char *)compressPcShr->PcImageOutEHMemory[i], imgsize);
					else
						fifo_write(tst->remote_tx_fifo, (unsigned char *)compressPcShr->PcImageOutELMemory[i], imgsize);


					fifo_write(tst->remote_tx_fifo, (unsigned char *)meta_data_buf, sizeof(meta_data_buf));
					if(zero_size > 0)
						fifo_write(tst->remote_tx_fifo, (unsigned char *)zero_buff, zero_size);
//					else
//						DEBUG_PRINT("PcStream tx size error\n");
					//image_fat = (struct disk_fat *)shared_data->PcImageHeaderData[shared_data->PcImageRdNo[tst->sock_number]];
					//DEBUG_PRINT("Image # PC  prev img %d crnt img %d on %d  \n", prev_img_no, image_fat->img_seq_no, shared_data->PcImageRdNo[tst->sock_number]);
					//if(prev_img_no > image_fat->img_seq_no)
						//DEBUG_PRINT("PcStream ########################################## PC  prev img %d crnt img %d wrong ORDER.. ################################\n", prev_img_no, image_fat->img_seq_no);
					//prev_img_no = image_fat->img_seq_no;
					compressPcShr->PcImageStatus[j][i][tst->sock_number] = IMGFRE;
					i++;
					if(i >= MAX_RAW_IMAGE_PC_BUFFER)
						i = 0;
					compressPcShr->PcImageRdNo[j][tst->sock_number] = i;
//					printf("buffer status for sock %d = ", tst->sock_number);
//					for(k = 0; k < MAX_RAW_IMAGE_PC_BUFFER; k++)
//						printf(" %d",compressPcShr->PcImageStatus[j][k][tst->sock_number]);
//					printf("\n");
				}
				// check for slow pipeline..
				k = compressPcShr->PcImageWrNo[j];
				if(compressPcShr->PcImageRdNo[j][tst->sock_number] <= k)
				{
					i = k - compressPcShr->PcImageRdNo[j][tst->sock_number];
				}
				else
				{
					i = k + (MAX_RAW_IMAGE_PC_BUFFER - compressPcShr->PcImageRdNo[j][tst->sock_number]);
				}
				tx_size = 0;
//				if(client_dev_type[tst->sock_number] == 12)
//				{
					if(i > 9)
						tx_size = 3;// skip 3
					else if(i > 7)
						tx_size = 2;// skip 2
					else if(i > 5)
						tx_size = 1;// skip 1
/*				}
				else
				{
					if(i > 6)
						tx_size = 3;// skip 3
					else if(i > 4)
						tx_size = 2;// skip 2
					else if(i > 3)
						tx_size = 1;// skip 1
				}*/
				if(tx_size)//skip images until half remaining..
				{
					//printf("PC: skipping %d images if stream %s on socket %d type %d\n", tx_size, (j == 0)?"FULL":"VGA", tst->sock_number, client_dev_type[tst->sock_number]);
					for(temp1 = 0; temp1 < tx_size; temp1++)
					{
						i = compressPcShr->PcImageRdNo[j][tst->sock_number];
						if(compressPcShr->PcImageStatus[j][i][tst->sock_number] == IMGRDY)
						{
							compressPcShr->PcImageStatus[j][i][tst->sock_number] = IMGFRE;
							i++;
							if(i >= MAX_RAW_IMAGE_PC_BUFFER)
								i = 0;
							compressPcShr->PcImageRdNo[j][tst->sock_number] = i;
						}
						else// something wrong.. initiate pcstreaminit
						{
							compressPcShr->PcImageInit[j][tst->sock_number] = 2;
//							DEBUG_PRINT("Pcbuff error %d in rdptr %d cnt %d wrptr %d\n", j, i, tx_size, k);
							break;
						}
					}
				}
			}
		}
	}
}


int server_rx(struct tst *tst)
{
	int i, j, ret = 0;
	unsigned char so_buff[MAX_SOCKET_DATA_TR];

	if(ioctl(tst->socket_handle, SIOCINQ, &j) >= 0)
	{
		if(j > MAX_SOCKET_DATA_TR)
			j = MAX_SOCKET_DATA_TR;
		if(j > 0)
		{
			i = read(tst->socket_handle, so_buff, j);
			if(i <= 0)
			{
				tst->connection_status = SOCK_CLOSE;
//				DEBUG_PRINT("PcStream Error in recieve 0 or null data \n");
			}
			fifo_write(tst->remote_rx_fifo, so_buff, i);
			//printf("rxed %d bytes\n", i);
			ret = 1;
		}
	}
	return (ret);
}

int server_tx(struct tst *tst)
{
	int i, j, ret = 0;
	unsigned char so_buff[MAX_SOCKET_DATA_TR * 8];

	if(tst->remote_tx_fifo->filled_length)
	{
		if(ioctl(tst->socket_handle, SIOCOUTQ, &j) >= 0)
		{
			if(j > (MAX_SOCKET_DATA_TR * 4))// more data in buffer...wait for next.
			return 1;

			if(tst->remote_tx_fifo->filled_length > MAX_SOCKET_DATA_TR * 8)
				i = MAX_SOCKET_DATA_TR * 8;
			else if(tst->remote_tx_fifo->filled_length > MAX_SOCKET_DATA_TR * 4)
				i = MAX_SOCKET_DATA_TR * 4;
			else if(tst->remote_tx_fifo->filled_length > MAX_SOCKET_DATA_TR * 2)
				i = MAX_SOCKET_DATA_TR * 2;
			else if(tst->remote_tx_fifo->filled_length > MAX_SOCKET_DATA_TR)
				i = MAX_SOCKET_DATA_TR;
			else
				i = tst->remote_tx_fifo->filled_length;
			fifo_read(tst->remote_tx_fifo, so_buff, i);
			j = write(tst->socket_handle, so_buff, i);
			if(j < 0)
			{
//				printf("--------- tx error on sock %d\n", tst->sock_number);
				return -1;
			}
			if(j < i)
			{
				if((i-j) < MAX_SOCKET_DATA_TR)
				if((i-j) > 0)
//				printf("--------- tx fifo full on sock %d - %d %d\n", tst->sock_number, i, j);
				fifo_rewind(tst->remote_tx_fifo, (i-j));
			}
		}
	}
	return (ret);
}

void read_mac_address(unsigned char *mac_address)
{
  char buffer[128];
  int fd;
  //unsigned char mac_address[6] = {0};
  struct ifreq ifr;

  fd = socket(AF_INET, SOCK_DGRAM, 0);

  ifr.ifr_addr.sa_family = AF_INET;
  safe_strncpy(ifr.ifr_name, "enP8p1s0", IFNAMSIZ-1);

  ioctl(fd, SIOCGIFHWADDR, &ifr);

  close(fd);

  mac_address[0]=(unsigned char)ifr.ifr_hwaddr.sa_data[0];
  mac_address[1]=(unsigned char)ifr.ifr_hwaddr.sa_data[1];
  mac_address[2]=(unsigned char)ifr.ifr_hwaddr.sa_data[2];
  mac_address[3]=(unsigned char)ifr.ifr_hwaddr.sa_data[3];
  mac_address[4]=(unsigned char)ifr.ifr_hwaddr.sa_data[4];
  mac_address[5]=(unsigned char)ifr.ifr_hwaddr.sa_data[5];

}

int getCableStatus(void)
{
	char buffer[32] = {0};
	int ret = 0;
	FILE *fptr;
	fptr = fopen(ETH_CABLE_CONNECTION,"r");
	if(fptr != NULL)
	{
		if((safe_fgets(buffer,32, 2, fptr))>=0)// returned succes...
		{
			if(safe_atoi(buffer, &ret) < 0)
			{
				ret = 0;
			}
		}
		else
			printf("connector_status not avilable.\n");
		fclose(fptr);
	}
	return ret;
}
unsigned short getNetSpeed(void)
{
	char buffer[32] = {0};
	int ret = 0;
	FILE *fptr;
	fptr = fopen(ETH_NETWORK_SPEED,"r");
	if(fptr != NULL)
	{
		if((safe_fgets(buffer,32, 5, fptr))>=0)// returned succes...
		{
			if(safe_atoi(buffer, &ret) < 0)
			{
				ret = 0;
			}
		}
		else
			printf("speed not avilable.\n");
		fclose(fptr);
	}
	return ret;
}

/* helper function to validate PIN (numeric only, max 32 chars) */
static int validate_pin(const char *pin) {
	int len = safe_strlen(pin, 9);
	if (!pin || len == 0 || len > 32)
	return 0;
	for (const char *p = pin; *p; p++) 
	{
		if (!isdigit(*p))
		return 0;
	}
	return 1;
}

int safe_signwith_cskey(unsigned char *pin) 
{
	const char cmd1[64] 	= {"pkcs11-tool --module /usr/lib/libckteec.so --login --slot"};
	const char cmd2[16] 	= {"--pin"};
	const char cmd3[8] 	= {"--id"};
	const char cmd4[256] 	= {"--mechanism SHA512-RSA-PKCS --sign --input-file /run/trois/bin/flash/firmware1.gz --output-file /run/trois/bin/flash/firmware1.sig"};
	char command[512] = {0};
	FILE *fptr;
	
	/* CRITICAL: Validate PIN before use */
	if (!validate_pin((char *)pin)) 
	{
		printf("ERROR: Invalid PIN format\n");
		return -1;
	}

	snprintf(command, sizeof(command), "%s %01d %s %s %s %04d %s", cmd1, 1, cmd2, (char *)pin, cmd3, 132, cmd4);
	fptr = popen(command, "r");// will execute the commands in string..
	if(fptr != NULL)
		pclose(fptr);// close will wait for the process to terminate and return..
	return 1; 
}

int safe_bkupNcopy_fw(void) 
{
	const char cmda[6][80]={"mv /usr/Camera/firmware.gz /usr/Camera/firmwareBKUP.gz",
				"mv /usr/Camera/firmware.sig /usr/Camera/firmwareBKUP.sig",
				"sync",
				"cp -af /run/trois/bin/flash/firmware1.gz /usr/Camera/firmware.gz",
				"cp -af /run/trois/bin/flash/firmware1.sig /usr/Camera/firmware.sig",
				"sync"};
	const size_t cmdlen[8] = {54, 56, 4, 62, 64, 4, 0, 0};

	FILE *fptr;
	for(int i = 0; i < 6; i++)
	{
		/* CRITICAL: Validate command before use */
		if (safe_strlen(cmda[i], 80) != cmdlen[i]) 
		{
			printf("ERROR: Invalid cmd inserted\n");
			return -1;
		}

		fptr = popen(cmda[i], "r");// will execute the commands in string..
		if(fptr != NULL)
			pclose(fptr);// close will wait for the process to terminate and return..
		sync();
	}
	return 1; 
}


