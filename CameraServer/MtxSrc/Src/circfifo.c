/********************************************************************************************/
/*		Project		:	IP Camera					    */
/*		Filename	:	circfifo.c					    */
/*		Functionality	:	Circular buffer routine				    */
/*		Author		:	Manoj Kumar D					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "circfifo.h"
/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);

void fifo_init(struct cirfifo *fifo, unsigned long depth, unsigned char *data)
{
	// resets read and write ptr length = 0 and data ptr to home;
	fifo->filled_length = 0;
	fifo->writeptr = 0;
	fifo->readptr = 0;
	fifo->fifo_depth = depth;
	fifo->fifo = data;
}
void fifo_flush(struct cirfifo *fifo)
{
	// puts read ptr to write ptr, length = 0 and no change to data ptr;
	fifo->filled_length = 0;
	fifo->readptr = fifo->writeptr;
}

unsigned long fifo_rewind(struct cirfifo *fifo, unsigned long length)// if ok returns (rewind length) else (0)
{
	// rewinds read ptr by length and adjust the fifo filled_length;
	unsigned long ret = 0;
//	if((fifo->fifo_depth - fifo->filled_length) <= length)
//		return 0;
	ret = length;
	if(fifo->readptr > length)
	{
		fifo->filled_length += length;
		fifo->readptr -= length;
	}
	else
	{
		fifo->filled_length += length;
		length -= fifo->readptr;
		fifo->readptr = fifo->fifo_depth - length;
	}
//	printf("fifo rewind %d\n", length);
	return ret;
}
unsigned long fifo_write(struct cirfifo *fifo, unsigned char *data, unsigned long len)
{
	// returns no of bytes writteen in fifo...
	unsigned long j, templen;
	if(!len)
		return 0;
	if(len > (fifo->fifo_depth - fifo->filled_length))
		len = fifo->fifo_depth - fifo->filled_length;
	templen = 0;
	j = fifo->writeptr;
	if(len > (fifo->fifo_depth - j))
	{
		templen = fifo->fifo_depth - j;
		if(safe_memcpy(&fifo->fifo[j], fifo->fifo_depth-j, data, templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(&fifo->fifo[j],data,templen);
		j+= templen;
		data+=templen;
		fifo->filled_length += (j - fifo->writeptr);
		j = fifo->writeptr = 0;
	}
	templen = len - templen;
	if(templen)
	{
		if(safe_memcpy(&fifo->fifo[j], fifo->fifo_depth-j, data, templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(&fifo->fifo[j],data,templen);
		j+= templen;
		//data+=templen;
	}
	fifo->filled_length += (j - fifo->writeptr);
	fifo->writeptr = j;
//	printf("fifo write %d\n", len);
	return len;
}

unsigned long fifo_read(struct cirfifo *fifo, unsigned char * data, unsigned long len)
{
	// returns no of bytes read from fifo...
	unsigned long j, templen;
	if(!len)
		return 0;
	if(len > fifo->filled_length)
		len = fifo->filled_length;
	templen = 0;
	j = fifo->readptr;
	if(len > (fifo->fifo_depth - j))
	{
		templen = fifo->fifo_depth - j;
		if(safe_memcpy(data, templen, &fifo->fifo[j], templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(data,&fifo->fifo[j],templen);
		data+=templen;
		j+=templen;
		fifo->filled_length -= (j - fifo->readptr);
		j = fifo->readptr = 0;
	}
	templen = len - templen;
	if(templen)
	{
		if(safe_memcpy(data, templen, &fifo->fifo[j], templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(data,&fifo->fifo[j],templen);
		j+=templen;
	}
	fifo->filled_length -= (j - fifo->readptr);
	fifo->readptr = j;
//	printf("fifo read %d\n", len);
	return len;
}

unsigned long fifo_peek(struct cirfifo *fifo, unsigned char * data, unsigned long len)
{
  //similar to read but will not alter variables...
	// returns no of bytes read from fifo ..
	unsigned long j, templen;
	if(!len)
		return 0;
	if(len > fifo->filled_length)
		len = fifo->filled_length;
	templen = 0;
	j = fifo->readptr;
	if(len > (fifo->fifo_depth - j))
	{
		templen = fifo->fifo_depth - j;
		if(safe_memcpy(data, templen, &fifo->fifo[j], templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(data,&fifo->fifo[j],templen);
		data+=templen;
		j = 0;
	}
	templen = len - templen;
	if(templen)
	{
		if(safe_memcpy(data, templen, &fifo->fifo[j], templen) <= 0)
			printf("memory cpy fail\n");
		//mem cpy(data,&fifo->fifo[j],templen);
	}
//	printf("fifo peek %d\n", len);
	return len;
}
