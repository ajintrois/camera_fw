/*
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <cuda_runtime.h>

#define BLACK_LEVEL	0x60
#define BLACK_LEVEL_R	0x60
#define BLACK_LEVEL_G	0x35
#define BLACK_LEVEL_B	0x60
#define MAX_VALUE_Y	256
#define MAX_VALUE_X	512
#define MAX_VALUE_I	MAX_VALUE_X-1
#define MAX_VALUE	MAX_VALUE_Y-1

/*
RGB ---> YUV
apply white balance and gamma correct RGB.
Then extract Y for all the pixels and u and v for quad pixels.
*/
/*
// SDTV equation
__constant__ float factorRgb2Yuv[3][3] = {{ 0.299,  0.587,  0.114},
                                    {-0.172, -0.339,  0.511},// +128
                                    { 0.511, -0.428,  0.083}};//+128
// SDTV computer equation
__constant__ float factorRgb2Yuv[3][3] = {{ 0.257,  0.504,  0.098},
                                    {-0.148, -0.291,  0.439},// +128
                                    { 0.439, -0.368,  0.071}};//+128
// HDTV equation
__constant__ float factorRgb2Yuv[3][3] = {{ 0.213,  0.715,  0.072},
                                    {-0.117, -0.394,  0.511},// +128
                                    { 0.511, -0.464,  0.047}};//+128
// HDTV computer equation
--------
__constant__ float factorRgb2Yuv[3][3] = {{ 0.183,  0.614,  0.062},
                                    {-0.101, -0.338,  0.439},// +128
                                    { 0.439, -0.399,  0.040}};//+128
*/
__constant__ float factorRgb2Yuv[3][3];
__constant__ short bayerPattern[4][4];
__constant__ unsigned char rgbGammaCurve[MAX_VALUE_X];
__constant__ unsigned char rgbGammaCurveg[MAX_VALUE_X];
__constant__ unsigned char rgbGammaCurveb[MAX_VALUE_X];
__constant__ unsigned char rgbGammaCurver[MAX_VALUE_X];
__constant__ short whitePoint = 1 << 10;

static void setBayerPatternConstant()
{
	float rgbyuvfactors[3][3] = {
//				{ 0.183,  0.614,  0.062},
  //                              {-0.101, -0.338,  0.439},// +128
    //                            { 0.439, -0.399,  0.040} // +128
				    { 0.213,  0.715,  0.072},
                                    {-0.117, -0.394,  0.511},// +128
                                    { 0.511, -0.464,  0.047} // +128
								};
	short pattern[4][4] =	{
				{0,1,2,3},// CU_EGL_COLOR_FORMAT_BAYER_RGGB
				{1,0,3,2},// CU_EGL_COLOR_FORMAT_BAYER_GRBG
				{2,3,0,1},// CU_EGL_COLOR_FORMAT_BAYER_GBRG
				{3,2,1,0} // CU_EGL_COLOR_FORMAT_BAYER_BGGR
				};
				
	cudaMemcpyToSymbol(bayerPattern, pattern, sizeof(pattern));
	cudaMemcpyToSymbol(factorRgb2Yuv, rgbyuvfactors, sizeof(rgbyuvfactors));
	
}

static void setGammaCurveConstant(float gamma)
{
//	const float gamma = 1.8;//1.8;
	double d, actualgamma;
	int i;
	unsigned char GammaCurve[MAX_VALUE_X];

	actualgamma = 1/gamma;// for R
	GammaCurve[0] = 0;
	for(i = 1; i < MAX_VALUE_X; i++)
	{
		d = pow(i/(double)MAX_VALUE_X,actualgamma);
		GammaCurve[i] = (unsigned char)((unsigned short)(d*MAX_VALUE_X) >> 1);
		//x=d*MAX_VALUE_X;
		//GammaCurve[i] = (unsigned char)x;
	}
	cudaMemcpyToSymbol(rgbGammaCurve, GammaCurve, sizeof(GammaCurve));

}
static void setGammaCurveConstantColor(float rgamma, float bgamma, float ggamma)
{
//	const float gamma = 1.8;//1.8;
	double d, actualgamma;
	int i;
	unsigned char GammaCurve[MAX_VALUE_X];

	actualgamma = 1/rgamma;// for R
	GammaCurve[0] = 0;
	for(i = 1; i < MAX_VALUE_X; i++)
	{
		d = pow(i/(double)MAX_VALUE_X,actualgamma);
		GammaCurve[i] = (unsigned char)((unsigned short)(d*MAX_VALUE_X) >> 1);
		//x=d*MAX_VALUE_X;
		//GammaCurve[i] = (unsigned char)x;
	}
	cudaMemcpyToSymbol(rgbGammaCurver, GammaCurve, sizeof(GammaCurve));
	
	actualgamma = 1/bgamma;// for R
	GammaCurve[0] = 0;
	for(i = 1; i < MAX_VALUE_X; i++)
	{
		d = pow(i/(double)MAX_VALUE_X,actualgamma);
		GammaCurve[i] = (unsigned char)((unsigned short)(d*MAX_VALUE_X) >> 1);
		//x=d*MAX_VALUE_X;
		//GammaCurve[i] = (unsigned char)x;
	}
	cudaMemcpyToSymbol(rgbGammaCurveb, GammaCurve, sizeof(GammaCurve));
	
	actualgamma = 1/ggamma;// for R
	GammaCurve[0] = 0;
	for(i = 1; i < MAX_VALUE_X; i++)
	{
		d = pow(i/(double)MAX_VALUE_X,actualgamma);
		GammaCurve[i] = (unsigned char)((unsigned short)(d*MAX_VALUE_X) >> 1);
		//x=d*MAX_VALUE_X;
		//GammaCurve[i] = (unsigned char)x;
	}
	cudaMemcpyToSymbol(rgbGammaCurveg, GammaCurve, sizeof(GammaCurve));

}
/*
static void setGammaCurveConstantg(float gamma)
{
//	const float gamma = 1.8;//1.8;
	double d, actualgamma;
	int i;
	unsigned char GammaCurve[MAX_VALUE_X];

	actualgamma = 1/(gamma - 0.5);// for R
	GammaCurve[0] = 0;
	for(i = 1; i < MAX_VALUE_X; i++)
	{
		d = pow(i/(double)MAX_VALUE_X,actualgamma);
		GammaCurve[i] = (unsigned char)((unsigned short)(d*MAX_VALUE_X) >> 1);
		//x=d*MAX_VALUE_X;
		//GammaCurve[i] = (unsigned char)x;
	}
	cudaMemcpyToSymbol(rgbGammaCurveg, GammaCurve, sizeof(GammaCurve));

}
*/
//======================================================================================================
// Converts a 12-bit Bayer quad to YUV420M. The Bayer components are provided
// in the order they're stored in the buffer
__device__ void convertBayerPix0C(	short4 bayerdata, 
					uchar4 *rgbblock, 
					uchar4 *yuvblock,
					short4 offsetcolor, 
					float whiteR,
					float whiteG1,
					float whiteG2,
					float whiteB)
{
	// Signed 16-bit Bayer maps 1<<12 to white.

/*	// white balance const  default values
	const float whiteR	= 1.7;
	const float whiteG	= 1.0;
	const float whiteB	= 1.9;
*/
	// Extract the Bayer quad.

	float RR = (umax(((bayerdata.x >> 6) & 0x03ff), offsetcolor.x) - offsetcolor.x);
	float GR = (umax(((bayerdata.y >> 6) & 0x03ff), offsetcolor.y) - offsetcolor.y);
	float GB = (umax(((bayerdata.z >> 6) & 0x03ff), offsetcolor.z) - offsetcolor.z);
	float BB = (umax(((bayerdata.w >> 6) & 0x03ff), offsetcolor.w) - offsetcolor.w);

/*
	unsigned char r = (unsigned char)((rgbGammaCurve[umin((((((float) 	RR) / whitePoint) 	* MAX_VALUE_I) * whiteR ), MAX_VALUE_I)]) >> 1);
	unsigned char g = (unsigned char)((rgbGammaCurve[umin((((((float)   ((GR + GB)/2)) / whitePoint)* MAX_VALUE_I) * whiteG1), MAX_VALUE_I)]) >> 1);
	unsigned char b = (unsigned char)((rgbGammaCurve[umin((((((float) 	BB) / whitePoint) 	* MAX_VALUE_I) * whiteB ), MAX_VALUE_I)]) >> 1);
*/
/*	unsigned char r = rgbGammaCurve[umin((((((float) RR) / whitePoint) * MAX_VALUE_I) * whiteR ), MAX_VALUE_I)] >> 1;
//	unsigned char g = rgbGammaCurve[umin((((((float)   ((GR + GB)/2)) / whitePoint) * MAX_VALUE_I) * whiteG1), MAX_VALUE_I)];
	unsigned char g1= rgbGammaCurve[umin((((((float) GR) / whitePoint) * MAX_VALUE_I) * whiteG1), MAX_VALUE_I)] >> 1;
	unsigned char g2= rgbGammaCurve[umin((((((float) GB) / whitePoint) * MAX_VALUE_I) * whiteG2), MAX_VALUE_I)] >> 1;
	unsigned char b = rgbGammaCurve[umin((((((float) BB) / whitePoint) * MAX_VALUE_I) * whiteB ), MAX_VALUE_I)] >> 1;
*/
	unsigned char r  = (unsigned char)(rgbGammaCurver[umin((((RR / whitePoint) * MAX_VALUE_I) * whiteR ), MAX_VALUE_I)]);
	unsigned char g1 = (unsigned char)(rgbGammaCurveg[umin((((GR / whitePoint) * MAX_VALUE_I) * whiteG1), MAX_VALUE_I)]);
	unsigned char g2 = (unsigned char)(rgbGammaCurveg[umin((((GB / whitePoint) * MAX_VALUE_I) * whiteG2), MAX_VALUE_I)]);
	unsigned char b  = (unsigned char)(rgbGammaCurveb[umin((((BB / whitePoint) * MAX_VALUE_I) * whiteB ), MAX_VALUE_I)]);
	unsigned char g  =  (g1 + g2)/2;

	yuvblock->x = (unsigned char) umin((factorRgb2Yuv[0][0]*r + factorRgb2Yuv[0][1]*g + factorRgb2Yuv[0][2]*b), MAX_VALUE);
	yuvblock->y = (factorRgb2Yuv[1][0]*r + factorRgb2Yuv[1][1]*g + factorRgb2Yuv[1][2]*b) + 127;
	yuvblock->z = (factorRgb2Yuv[2][0]*r + factorRgb2Yuv[2][1]*g + factorRgb2Yuv[2][2]*b) + 127;

	rgbblock->x = r;
	rgbblock->y = g1;
	rgbblock->z = g2;
//	rgbblock->y = g;
//	rgbblock->z = g;
	rgbblock->w = b;
}
//======================================================================================================
__device__ void convertBayerPix0B(	short4 bayerdata, 
					uchar4 *rgbblock, 
					unsigned char *y,
					float whiteR,
					float whiteG1,
					float whiteG2,
					float whiteB)
{

	float RR = (umax(((bayerdata.x >> 6) & 0x03ff), BLACK_LEVEL_R) - BLACK_LEVEL_R);
	float GR = (umax(((bayerdata.y >> 6) & 0x03ff), BLACK_LEVEL_G) - BLACK_LEVEL_G);
	float GB = (umax(((bayerdata.z >> 6) & 0x03ff), BLACK_LEVEL_G) - BLACK_LEVEL_G);
	float BB = (umax(((bayerdata.w >> 6) & 0x03ff), BLACK_LEVEL_B) - BLACK_LEVEL_B);


	unsigned char r = (unsigned char)(rgbGammaCurve[umin((((((float) 	RR) / whitePoint) 	* MAX_VALUE_I) * whiteR ), MAX_VALUE_I)]);
//	unsigned char g = (unsigned char)(rgbGammaCurve[umin((((((float)   ((GR + GB)/2)) / whitePoint) * MAX_VALUE_I) * whiteG1), MAX_VALUE_I)]);
	unsigned char g1= rgbGammaCurve[umin((((((float) GR) / whitePoint) * MAX_VALUE_I) * whiteG1), MAX_VALUE_I)];
	unsigned char g2= rgbGammaCurve[umin((((((float) GB) / whitePoint) * MAX_VALUE_I) * whiteG2), MAX_VALUE_I)];
	unsigned char b = (unsigned char)(rgbGammaCurve[umin((((((float) 	BB) / whitePoint) 	* MAX_VALUE_I) * whiteB ), MAX_VALUE_I)]);
	unsigned char g = (g1+g2)/2;

	*y = (unsigned char)umin((factorRgb2Yuv[0][0]*r + factorRgb2Yuv[0][1]*g + factorRgb2Yuv[0][2]*b), MAX_VALUE);

	rgbblock->x = r;
	rgbblock->y = g1;
	rgbblock->z = g2;
//	rgbblock->y = g;
//	rgbblock->z = g;
	rgbblock->w = b;
}

//======================================================================================================

__device__ unsigned char convertBayerPix1(	unsigned char 		BB, 
						unsigned char 		GB, 
						unsigned char 		GR, 
						unsigned char 		RR)
{

	return((unsigned char)umin((factorRgb2Yuv[0][0] * RR + factorRgb2Yuv[0][1] * ((GR + GB)/2) + factorRgb2Yuv[0][2] * BB), MAX_VALUE));
}
//======================================================================================================

__device__ unsigned char convertBayerPix2(	unsigned char 		RR, 
						unsigned char 		GR, 
						unsigned char 		GB, 
						unsigned char 		BB)
{

	return((unsigned char)umin((factorRgb2Yuv[0][0] * RR + factorRgb2Yuv[0][1] * ((GR + GB)/2) + factorRgb2Yuv[0][2] * BB), MAX_VALUE));
}
//======================================================================================================

__device__ unsigned char convertBayerPix3(	unsigned char 		GR, 
						unsigned char 		RR, 
						unsigned char 		BB, 
						unsigned char 		GB)
{

	return((unsigned char)umin((factorRgb2Yuv[0][0] * RR + factorRgb2Yuv[0][1] * ((GR + GB)/2) + factorRgb2Yuv[0][2] * BB), MAX_VALUE));
}
//======================================================================================================
__global__ void gpuConvertBayer2YUV420_kernel3( unsigned char *src, 
					   	unsigned char *dsty,
					   	unsigned int width, 
					   	unsigned int height)
{
	int chromaaddr = blockIdx.x * blockDim.x + threadIdx.x;// 2x2 blockaddr..
	int lineNo = (chromaaddr / (width / 2));// line number.. Odd line number of 2048
	if(lineNo >= (height/2)-1)
		return;
	int pixelNo = ((chromaaddr % (width/2))*2)+1;
	if(pixelNo >= (width)-1)
		return;
	int bayerblock =  pixelNo + (lineNo*width*2) + width;//process start on odd pixels on odd lines...

	dsty[bayerblock] = convertBayerPix3(	src[bayerblock], 
						src[bayerblock+1], 
						src[bayerblock+width], 
						src[bayerblock+width+1]);
	return;
}
//======================================================================================================
__global__ void gpuConvertBayer2YUV420_kernel2( unsigned char *src, 
					   	unsigned char *dsty,
					   	unsigned int width, 
					   	unsigned int height)
{
	int chromaaddr = blockIdx.x * blockDim.x + threadIdx.x;// 2x2 blockaddr..
	int lineNo = (chromaaddr / (width / 2));// line number.. Odd line number of 2048
	if(lineNo >= (height/2)-1)
		return;
	int pixelNo = ((chromaaddr % (width/2))*2);
	int bayerblock =  pixelNo + (lineNo*width*2) + width ;//process start on even pixels on odd lines...

	dsty[bayerblock] = convertBayerPix2(	src[bayerblock], 
						src[bayerblock+1], 
						src[bayerblock+width], 
						src[bayerblock+width+1]);
	return;
}
//======================================================================================================
__global__ void gpuConvertBayer2YUV420_kernel1( unsigned char *src, 
						unsigned char *dsty,
					   	unsigned int width, 
					   	unsigned int height)
{
	int chromaaddr = blockIdx.x * blockDim.x + threadIdx.x;// 2x2 blockaddr..
	int lineNo = chromaaddr / (width / 2);// line number.. even line number of 2048..
	int pixelNo = ((chromaaddr % (width/2))*2+1);
	if(pixelNo >= (width-1))
		return;
	int bayerblock =  pixelNo + (lineNo*width*2);//process start on odd pixels on even lines...

	dsty[bayerblock] = convertBayerPix1(	src[bayerblock], 
						src[bayerblock+1], 
						src[bayerblock+width], 
						src[bayerblock+width+1]);
	return;
}

//======================================================================================================
__global__ void gpuConvertBayer2YUV420_kernel0C( unsigned char  *src, 
					   	unsigned char 	*dsty,
					   	char 		*dstu,
					   	char 		*dstv,
					   	unsigned char 	*dstb,
					   	unsigned int 	width, 
					   	unsigned int 	height,
					   	short		roffset,
					   	short		g1offset,
					   	short		g2offset,
					   	short		boffset,
						float		wbgainR,
						float		wbgainG1,
						float		wbgainG2,
						float		wbgainB)
{
	short* bayerOffset  = (short*)src;
	short4 bayerdata;
	uchar4 rgbblock;
	uchar4 yuvblock;
	int chromaaddr = blockIdx.x * blockDim.x + threadIdx.x;// 2x2 blockaddr..
	int lineNo = chromaaddr / (width / 2);// line number.. even line number of 2048..
	int invlineNo = (height/2 - 1) - (chromaaddr / (width / 2));// line number.. even line number of 2048..
	int bayerblock = (invlineNo*width*2) + (chromaaddr % (width/2))*2;//process on even pixels on even lines...
	short4 offsetcolor;
	int outuvblock = (chromaaddr % (width/2))*2 + (lineNo*width*2);//process on even pixels on even lines...

	bayerdata.x = bayerOffset[bayerblock];
	bayerdata.y = bayerOffset[bayerblock+1];
	bayerdata.z = bayerOffset[bayerblock+width];
	bayerdata.w = bayerOffset[bayerblock+width+1];
	
	offsetcolor.x = roffset;
	offsetcolor.y = g1offset;
	offsetcolor.z = g2offset;
	offsetcolor.w = boffset;

	convertBayerPix0C(	bayerdata, 
				&rgbblock, 
				&yuvblock,
				offsetcolor,
				wbgainR,
				wbgainG1,
				wbgainG2,
				wbgainB);
				
	dstb[outuvblock] = rgbblock.z;
	dstb[outuvblock+1] = rgbblock.w;
	dstb[outuvblock+width] = rgbblock.x;
	dstb[outuvblock+width+1] = rgbblock.y;
	dsty[outuvblock] = yuvblock.x;
	dstu[chromaaddr] = yuvblock.y;
	dstv[chromaaddr] = yuvblock.z;
	
	return;
}
//======================================================================================================
__global__ void gpuConvertBayer2YUV420_kernel0B( unsigned char  *src, 
					   	unsigned char 	*dsty,
					   	char 		*dstu,
					   	char 		*dstv,
					   	unsigned char 	*dstb,
					   	unsigned int 	width, 
					   	unsigned int 	height,
						float		wbgainR,
						float		wbgainG1,
						float		wbgainG2,
						float		wbgainB)
{
	short* bayerOffset  = (short*)src;
	short4 bayerdata;
	uchar4 rgbblock;
	unsigned char y;
	int chromaaddr = blockIdx.x * blockDim.x + threadIdx.x;// 2x2 blockaddr..
	int lineNo = chromaaddr / (width / 2);// line number.. even line number of 2048..
	int invlineNo = (height/2 - 1) - (chromaaddr / (width / 2));// line number.. even line number of 2048..
	int bayerblock = (invlineNo*width*2) + (chromaaddr % (width/2))*2;//process on even pixels on even lines...

	int outuvblock = (chromaaddr % (width/2))*2 + (lineNo*width*2);//process on even pixels on even lines...

	bayerdata.x = bayerOffset[bayerblock];
	bayerdata.y = bayerOffset[bayerblock+1];
	bayerdata.z = bayerOffset[bayerblock+width];
	bayerdata.w = bayerOffset[bayerblock+width+1];
	
	convertBayerPix0B(	bayerdata, 
				&rgbblock, 
				&y,
				wbgainR,
				wbgainG1,
				wbgainG2,
				wbgainB);
				
	dstb[outuvblock] = rgbblock.z;
	dstb[outuvblock+1] = rgbblock.w;
	dstb[outuvblock+width] = rgbblock.x;
	dstb[outuvblock+width+1] = rgbblock.y;
	dsty[outuvblock] = y;
	dstu[chromaaddr] = 0x80;
	dstv[chromaaddr] = 0x80;
	
	return;
}
//======================================================================================================
void gpuConvertBayer2YUV4203buf(unsigned char 	*src, 
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
				int		color)				
{

	unsigned char *d_src = NULL;
	unsigned char *d_dstbayer = NULL;
	size_t planeSize = width * height * sizeof(unsigned char);

	d_src = src;
	cudaStreamAttachMemAsync(NULL, src, 0, cudaMemAttachGlobal);

	cudaStreamAttachMemAsync(NULL, dsty, 0, cudaMemAttachGlobal);
	cudaStreamAttachMemAsync(NULL, dstu, 0, cudaMemAttachGlobal);
	cudaStreamAttachMemAsync(NULL, dstv, 0, cudaMemAttachGlobal);

	cudaMalloc(&d_dstbayer, planeSize+height+width);

	//wbgainR = 1.7;
	//wbgainG1 = 1.0;
	//wbgainG2 = 1.0;
	//wbgainB = 1.9;
	setBayerPatternConstant();
	setGammaCurveConstant(gamma);
	setGammaCurveConstantColor(gamma, gamma, gamma);
	//setGammaCurveConstantg(gamma);
	unsigned int blockSize = 1024;
	unsigned int numBlocks = (((height * width) / 4) + (blockSize - 1)) / blockSize;
//	gpuConvertBayer2YUV420_kernel0<<<numBlocks, blockSize>>>(d_src, dsty, (char*)dstu, (char*)dstv, d_dstbayer, width, height, wbgainR, wbgainG1, wbgainG2, wbgainB);
	if(color)
		gpuConvertBayer2YUV420_kernel0C<<<numBlocks, blockSize>>>(d_src, dsty, (char*)dstu, (char*)dstv, d_dstbayer, width, height, roffset, g1offset, g2offset, boffset, wbgainR, wbgainG1, wbgainG2, wbgainB);
	else
		gpuConvertBayer2YUV420_kernel0B<<<numBlocks, blockSize>>>(d_src, dsty, (char*)dstu, (char*)dstv, d_dstbayer, width, height, wbgainR, wbgainG1, wbgainG2, wbgainB);
	cudaStreamSynchronize(NULL);
	gpuConvertBayer2YUV420_kernel1<<<numBlocks, blockSize>>>(d_dstbayer, dsty, width, height);
	gpuConvertBayer2YUV420_kernel2<<<numBlocks, blockSize>>>(d_dstbayer, dsty, width, height);
	gpuConvertBayer2YUV420_kernel3<<<numBlocks, blockSize>>>(d_dstbayer, dsty, width, height);
	cudaStreamSynchronize(NULL);
	cudaFree(d_dstbayer);
	cudaStreamAttachMemAsync(NULL, src, 0, cudaMemAttachHost);
	
	cudaStreamAttachMemAsync(NULL, dsty, 0, cudaMemAttachHost);
	cudaStreamAttachMemAsync(NULL, dstu, 0, cudaMemAttachHost);
	cudaStreamAttachMemAsync(NULL, dstv, 0, cudaMemAttachHost);


}
