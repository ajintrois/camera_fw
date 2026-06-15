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
// on demand, from a MIMG video file.
// Implementation
/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

#include "MtxMIMGVideoLiveServerMediaSubsession.hh"
#include "JPEGVideoRTPSink.hh"
#include "MtxMIMGVideoLiveFramedSource.hh"
//#include "MIMGVideoStreamFramer.hh"
#include "ByteStreamFileSource.hh"


#ifndef _MIMG_VIDEO_RTP_SINK_HH
#include "JPEGVideoRTPSink.hh"
#endif

extern void mtx_strcpy(char *destination, const char *src);

MtxMIMGVideoLiveServerMediaSubsession*
MtxMIMGVideoLiveServerMediaSubsession::createNew(UsageEnvironment& env,char const* fileName,Boolean reuseFirstSource) 
{
	return new MtxMIMGVideoLiveServerMediaSubsession(env, fileName, reuseFirstSource);
}

MtxMIMGVideoLiveServerMediaSubsession::MtxMIMGVideoLiveServerMediaSubsession(UsageEnvironment& env,char const* fileName, Boolean reuseFirstSource)
: OnDemandServerMediaSubsession(env, reuseFirstSource)
{
	mtx_strcpy(fFileName, fileName);
}

MtxMIMGVideoLiveServerMediaSubsession::~MtxMIMGVideoLiveServerMediaSubsession() 
{
}

FramedSource* MtxMIMGVideoLiveServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) 
{
	estBitrate = 15000;//500; // kbps, estimate
	return MtxMIMGVideoLiveFramedSource::createNew(envir(), fFileName);

}

#if 1
RTPSink* MtxMIMGVideoLiveServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,FramedSource* /*inputSource*/) 
{
	return JPEGVideoRTPSink::createNew(envir(), rtpGroupsock);
}
#endif


