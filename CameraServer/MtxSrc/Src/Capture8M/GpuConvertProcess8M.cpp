/********************************************************************************************/
/*		Project		:	TK1 Image Analytics				    */
/*		Filename	:	UncannyProcess.c				    */
/*		Functionality	:	Uncanny AI Image Analytics Interface Processing     */
/*		Author		:	Manoj Kumar D					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h> 

#include <cuda_runtime.h>

#include "../defines.h"
#include "../common_shm.h"

/********************************************************************************************/
/*                          Defines                                                         */
/********************************************************************************************/

#define DEBUG_PRINT			printf

#define LOG_BUF
#define OUT_BUF
#define	TRUE 1
#define FALSE 0

using namespace std;

/********************************************************************************************/
/*                          Global Vars                                                     */
/********************************************************************************************/

/********************************************************************************************/
/*                          Extern Vars                                                     */
/********************************************************************************************/
extern unsigned int		process_timer[];
extern struct cirfifo		*ProcessDebugFifo;
extern char 			DebugStr[1024];
extern int 			DebugStrSize;
extern unsigned short		keepalive_timer[MAX_WDT_COUNT], keepalive_timer_enabled[MAX_WDT_COUNT], keepalive_timer_reload[MAX_WDT_COUNT], aux_keep_alive_timer[MAX_WDT_COUNT];
extern int			wdt_rd_pipe, wdt_wr_pipe;
extern int			processIDVar, displaymsg_cnt;
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/

void gpuConvertBayer2YUV4203buf(	unsigned char 	*src, 
					unsigned char 	*dsty, 
					unsigned char 	*dstu, 
					unsigned char 	*dstv, 
					unsigned int 	width, 
					unsigned int 	height,
					float		gamma,
					float		wbgainR,
					float		wbgainG1,
					float		wbgainG2,
					float		wbgainB,
				   	short		roffset,
				   	short		g1offset,
				   	short		g2offset,
				   	short		boffset,
					int		color);


/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/

int MTXcudaISPNVbuffer(unsigned char *cuda_in_buffer, unsigned char *cuda_y_buffer, unsigned char *cuda_u_buffer, unsigned char *cuda_v_buffer, uint16_t width, uint16_t height, float loadgamma, float wbr, float wbgr, float wbgb, float wbb, short roffset, short g1offset, short g2offset, short boffset, int color)
{
	bool			cuda_zero_copy  = TRUE;
	unsigned int		size, convertImgSize;
	int			frameno;
	unsigned char 		*imgptr, *d_src, *d_dsty, *d_dstu, *d_dstv,*hostsrc, *dsrcmapped;
	unsigned long starttime, endtime;
	unsigned long starttime1, endtime1;
	unsigned long int imagptr;
	struct timeval t1, t2;
	
	// allocate memory...
	size = width*height*2;
//	wbr = 1.7;
//	wbgr = 1.1;
//	wbgb = 1.1;
//	wbb = 1.97;
//	loadgamma = gammaValue[gammaindex];//+5.0;
	
	gettimeofday(&t1, NULL);
	starttime1 = (t1.tv_sec*1000000) + t1.tv_usec;

	imagptr = (unsigned long int)cuda_in_buffer;
	imgptr = (unsigned char*)(imagptr + width*2);
	cudaHostRegister(imgptr, (width * height * 2), 0);
	cudaHostGetDevicePointer(&d_src, imgptr, 0);

	
	cudaHostRegister(cuda_y_buffer,  (width * height), 0);
	cudaHostRegister(cuda_u_buffer, ((width * height)>> 2), 0);
	cudaHostRegister(cuda_v_buffer, ((width * height)>> 2), 0);
	
	cudaHostGetDevicePointer(&d_dsty, cuda_y_buffer, 0);
	cudaHostGetDevicePointer(&d_dstu, cuda_u_buffer, 0);
	cudaHostGetDevicePointer(&d_dstv, cuda_v_buffer, 0);


	frameno = 0;

	gettimeofday(&t2, NULL);
	endtime1 = (t2.tv_sec*1000000) + t2.tv_usec;
	starttime = (t2.tv_sec*1000000) + t2.tv_usec;
	
	gpuConvertBayer2YUV4203buf (d_src, 
			d_dsty, 
			d_dstu, 
			d_dstv, 
			width, 
			height,
			loadgamma,
			wbr,
			wbgr,
			wbgb,
			wbb,
			roffset, 
			g1offset, 
			g2offset, 
			boffset, 
			color);
	gettimeofday(&t2, NULL);
	endtime = (t2.tv_sec*1000000) + t2.tv_usec;
//	printf(" %3d: GPU copy = %05ld uSec  \tgpu proc= %05ld uSec width = %4d height = %4d\t", frameno, endtime1 - starttime1, endtime - starttime, width, height);		
	cudaHostUnregister(imgptr);
	cudaHostUnregister(cuda_y_buffer);
	cudaHostUnregister(cuda_u_buffer);
	cudaHostUnregister(cuda_v_buffer);
	return 0;
}




