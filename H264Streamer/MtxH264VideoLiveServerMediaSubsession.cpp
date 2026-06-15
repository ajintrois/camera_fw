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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H264 video file.
// Implementation
/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

#include "MtxH264VideoLiveServerMediaSubsession.hh"
#include "H264VideoRTPSink.hh"
#include "MtxH264VideoLiveFramedSource.hh"
#include "H264VideoStreamFramer.hh"
#include "ByteStreamFileSource.hh"

//VK
extern void mtx_strcpy(char *destination, const char *src);


MtxH264VideoLiveServerMediaSubsession* MtxH264VideoLiveServerMediaSubsession::createNew(UsageEnvironment& env,char const* fileName,Boolean reuseFirstSource) 
{
	return new MtxH264VideoLiveServerMediaSubsession(env, fileName, reuseFirstSource);
}

MtxH264VideoLiveServerMediaSubsession::MtxH264VideoLiveServerMediaSubsession(UsageEnvironment& env,char const* fileName, Boolean reuseFirstSource) 
: H264VideoFileServerMediaSubsession(env, fileName, reuseFirstSource)
{
	mtx_strcpy(fFileName, fileName);
}

MtxH264VideoLiveServerMediaSubsession::~MtxH264VideoLiveServerMediaSubsession() 
{
}

FramedSource* MtxH264VideoLiveServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) 
{
	estBitrate = 2000;//500; // kbps, estimate

	// Create the video source:
	MtxH264VideoLiveFramedSource* liveSource = MtxH264VideoLiveFramedSource::createNew(envir(), fFileName);

	if (liveSource == NULL) return NULL;
	//printf("haaaaaaaaaaai %s\n",fFileName);

	// Create a framer for the Video Elementary Stream:
	return H264VideoStreamFramer::createNew(envir(), liveSource);
}

