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

#define	TRUE 1
#define FALSE 0

using namespace std;

/********************************************************************************************/
/*                          Global Vars                                                     */
/********************************************************************************************/

/********************************************************************************************/
/*                          Extern Vars                                                     */
/********************************************************************************************/
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/

extern void gpuConvertBayer2YUV4203buf(
				unsigned char 	*src, 
				unsigned char 	*dsty, 
				unsigned char 	*dstu, 
				unsigned char 	*dstv, 
				short 		srcwidth, 
				short 		srcheight,
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


int MTXcudaISPNVbuffer(unsigned char *in_buffer, unsigned char *y_buffer, unsigned char *u_buffer, unsigned char *v_buffer, 
			short width, short height, float gamma, float wbr, float wbgr, float wbgb, float wbb,
			short roffset, short g1offset, short g2offset, short boffset, int color)
{
	bool cuda_zero_copy  = TRUE;
	unsigned char *d_src, *d_dsty, *d_dstu, *d_dstv;
	unsigned long int imagptr;
	
	cudaHostRegister(in_buffer, (width * height * 2), 0);
	cudaHostGetDevicePointer(&d_src, in_buffer, 0);

	cudaHostRegister(y_buffer,  (width * height), 0);
	cudaHostRegister(u_buffer, ((width * height)>> 2), 0);
	cudaHostRegister(v_buffer, ((width * height)>> 2), 0);
	
	cudaHostGetDevicePointer(&d_dsty, y_buffer, 0);
	cudaHostGetDevicePointer(&d_dstu, u_buffer, 0);
	cudaHostGetDevicePointer(&d_dstv, v_buffer, 0);

	gpuConvertBayer2YUV4203buf(d_src, d_dsty, d_dstu, d_dstv, width, height, gamma, wbr, wbgr, wbgb, wbb, roffset, g1offset, g2offset, boffset, color);

	cudaHostUnregister(in_buffer);
	cudaHostUnregister(y_buffer);
	cudaHostUnregister(u_buffer);
	cudaHostUnregister(v_buffer);
	
	return 0;
}


