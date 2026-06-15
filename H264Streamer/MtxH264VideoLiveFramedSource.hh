/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2013 Live Networks, Inc.  All rights reserved.
// A file source that is a plain byte stream (rather than frames)
// C++ header
/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

#include "FramedSource.hh"
#include <fcntl.h>      /* Needed only for _O_RDWR definition */  
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>



#ifndef MAX_FRAME_BUF_SIZE
#define MAX_FRAME_BUF_SIZE (1024 * 1024)
#endif

#ifndef RESIDUALBUFMAXSIZE
#define RESIDUALBUFMAXSIZE		MAX_FRAME_BUF_SIZE//0x20000
#endif

class MtxH264VideoLiveFramedSource: public FramedSource 
{
	public:
		static MtxH264VideoLiveFramedSource* createNew(UsageEnvironment& env,char const* fileName,unsigned preferredFrameSize = 0,unsigned playTimePerFrame = 0);
		// "preferredFrameSize" == 0 means 'no preference'
		// "playTimePerFrame" is in microseconds

	protected:
		MtxH264VideoLiveFramedSource(UsageEnvironment& env,char const* fileName,unsigned preferredFrameSize,unsigned playTimePerFrame);
		// called only by createNew()

		virtual ~MtxH264VideoLiveFramedSource();

		//int ReadOneNaluFromVideoList(encDrvParam *encparam, frameEnc *frame);
		int ReadOneNaluFromVideoList(unsigned char *buffer);

	private:
		// redefined virtual functions:
		virtual void doGetNextFrame();

	protected:
		unsigned char residual_buf[RESIDUALBUFMAXSIZE];
		int residual_size;
		unsigned char framebuffer[MAX_FRAME_BUF_SIZE];
		unsigned long ImageSize;

};

