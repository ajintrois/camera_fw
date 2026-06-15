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

#ifndef _MIMG_VIDEO_LIVE_FRAMED_SOURCE_HH
#define _MIMG_VIDEO_LIVE_FRAMED_SOURCE_HH

#ifndef _JPEG_VIDEO_SOURCE_HH
#include "JPEGVideoSource.hh"
#endif


#ifndef MAX_FRAME_BUF_SIZE
#define MAX_FRAME_BUF_SIZE (4 * 1024 * 1024)
#endif

class MtxMIMGVideoLiveFramedSource: public JPEGVideoSource
{
	public:
		static MtxMIMGVideoLiveFramedSource* createNew(UsageEnvironment& env,char const* fileName,unsigned preferredFrameSize = 0,unsigned playTimePerFrame = 0);
		// "preferredFrameSize" == 0 means 'no preference'
		// "playTimePerFrame" is in microseconds
		virtual u_int8_t type();
		virtual u_int8_t qFactor();
		virtual u_int8_t width(); // # pixels/8 (or 0 for 2048 pixels)
		virtual u_int8_t height(); // # pixels/8 (or 0 for 2048 pixels)
		virtual u_int16_t restartInterval();
		virtual u_int8_t const* quantizationTables(u_int8_t& precision,u_int16_t& length);

	protected:
		MtxMIMGVideoLiveFramedSource(UsageEnvironment& env,char const* fileName,unsigned preferredFrameSize,unsigned playTimePerFrame);
		// called only by createNew()

		virtual ~MtxMIMGVideoLiveFramedSource();

		int ReadOneNaluFromVideoList(unsigned char *buffer);
	private:
		// redefined virtual functions:
		virtual void doGetNextFrame();

	protected:
		unsigned char framebuffer[MAX_FRAME_BUF_SIZE];
		unsigned long ImageSize;
};

#endif
