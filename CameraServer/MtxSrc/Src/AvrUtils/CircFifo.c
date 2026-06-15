/********************************************************************************************/
/*		Project		:	HAT Network to Serial Interface			    */
/*		Filename	:	CircFifo.c					    */
/*		Functionality	:	Circular Fifo operations 			    */
/*		Author		:	Manoj Kumar					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include "SystemDefines.h"
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/

/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/

void FifoInit(CIRCULAR_FIFO *fifo, int depth, unsigned char *data)
{
	// resets read and write ptr length = 0 and data ptr to home;
	fifo->filled_length = 0;
	fifo->writeptr = 0;
	fifo->readptr = 0;
	fifo->fifo_depth = depth;
	fifo->fifo = data;
	//printf("Haai %d %d\n",fifo->fifo_depth,fifo->filled_length);
}
void FifoFlush(CIRCULAR_FIFO *fifo)
{
	// puts read ptr to write ptr, length = 0 and no change to data ptr;
	fifo->filled_length = 0;
	fifo->readptr = fifo->writeptr;
}

int FifoRewind(CIRCULAR_FIFO *fifo, int length)// if ok returns (rewind length) else (0)
{
	// rewinds read ptr by length and adjust the fifo filled_length;
	int ret = 0;
//	if((fifo->fifo_depth - fifo->filled_length) <= length)
//		return 0;
	ret = length;
	fifo->filled_length += length;
	if(fifo->readptr > length)
	{
		fifo->readptr -= length;
	}
	else
	{
		length -= fifo->readptr;
		fifo->readptr = fifo->fifo_depth - length;
	}
	return ret;
}

int FifoWrite(CIRCULAR_FIFO *fifo, unsigned char *data, int len)
{
	// returns no of bytes writteen in fifo...
	int /*i,*/ j, templen;
	//unsigned long *tempdata;

	//printf("Haai %d\n",len);
	if(!len)
	{
		printf("Haai %d\n",len);
		return 0;
	}
	if(len > (fifo->fifo_depth - fifo->filled_length))
	{
		printf("Haai %d %d %d\n",len,fifo->fifo_depth,fifo->filled_length);
		len = fifo->fifo_depth - fifo->filled_length;				
	}	
	//printf("Haai1 %d\n",len);
	templen = 0;
	j = fifo->writeptr;
	if(len > (fifo->fifo_depth - j))
	{
		templen = fifo->fifo_depth - j;
		mempcpy(&fifo->fifo[j],data,templen);
		j+= templen;
		data+=templen;
		fifo->filled_length += (j - fifo->writeptr);
		j = fifo->writeptr = 0;
	}
	//printf("Haai2 %d\n",len);	
	templen = len - templen;
	if(templen)
	{
		mempcpy(&fifo->fifo[j],data,templen);
		j+= templen;
		//data+=templen;
	}
	fifo->filled_length += (j - fifo->writeptr);
	fifo->writeptr = j;
	return len;
}

int FifoRead(CIRCULAR_FIFO *fifo, unsigned char * data, int len)
{
	// returns no of bytes read from fifo...
	int /*i,*/ j, templen;
	if(!len)
		return 0;
	if(len > fifo->filled_length)
		len = fifo->filled_length;
	templen = 0;
	j = fifo->readptr;
	if(len > (fifo->fifo_depth - j))
	{
		templen = fifo->fifo_depth - j;
		mempcpy(data,&fifo->fifo[j],templen);
		data+=templen;
		j+=templen;
		fifo->filled_length -= (j - fifo->readptr);
		j = fifo->readptr = 0;
	}
	templen = len - templen;
	if(templen)
	{
		mempcpy(data,&fifo->fifo[j],templen);
		j+=templen;
	}
	fifo->filled_length -= (j - fifo->readptr);
	fifo->readptr = j;
	return len;
}
