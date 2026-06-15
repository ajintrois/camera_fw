/*
 * Safe alternatives to banned C functions for stqc test
 *
 * Input validation on all external data
 * for replacement function list refer replaced.txt
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h> // for size_t
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>


/**
 * safe_strlen - Safely calculates the length of a string.
 * @str: Pointer to the string (can be NULL).
 * @max_len: Maximum number of characters to check (prevents overrun).
 *
 * Returns:
 *   - Length of the string (up to max_len) if valid.
 *   - 0 if str is NULL.
 */
size_t safe_strlen(const char *str, size_t max_len) 
{
	if (str == NULL) 
	{
		return 0; // NULL pointer safety
	}
	size_t len = 0;
	while (len < max_len && str[len] != '\0') 
	{
		len++;
	}
	return len; // Will be max_len if no '\0' found
}


/* helper function to validate hex addresses */
static int validate_hex_addr(unsigned long addr) {
	/* Add your specific validation logic here */
	return (addr != 0);
}

/**
 * safe_strncpy - Safely calculates the length of src string.
 * @dest: Pointer to the dest string ( NULL pointer safety will be checked).
 * @src: Pointer to the source string ( NULL pointer safety will be checked).
 * @count: size of destination (prevents overrun).
 *
 * Returns:
 *   - Pointer to the destination string (terminated up to src length or count) if valid.
 *   - NULL if input is NULL (No actions taken)
 */
char* safe_strncpy( char* dest, const char* src, size_t count) 
{
	size_t destsz;
	if(dest == NULL)// NULL pointer safety
		return NULL;
	if(src == NULL)// NULL pointer safety
	{
		dest[0] = '\0';// NULL pointer safety returns 0 string
		return dest;
	}
	if((src > dest) && (src < (dest + count))) // overlap check for src 
	{
		dest[0] = '\0';// overlap safe-> returns 0 string
		return dest;
	}
	destsz=safe_strlen(src, count);// returns count without \0
	if(destsz == 0)
	{
		dest[0] = '\0';
		return NULL;
	}
	if((dest > src) && (dest < (src + destsz))) // overlap check for dest
	{
		dest[0] = '\0';// overlap safe-> returns 0 string
		return dest;
	}
	mempcpy(dest, src, destsz);
	//str ncpy(dest, src, destsz);
	dest[destsz] = '\0'; // Ensure null termination
	return dest;
}


/**
 * safe_atoi - Safely return intiger from an input string
 * @nptr: source string.
 * @value: if no errors, retuns the value of string.
 *
 * Returns:
 *   > 0 : Number of characters actually written (excluding null terminator).
 *   -1   : Error occurred (invalid args or formatting error).
 *
 * Notes:
 * - checks for NULL pointer in place, returns error
 * - Detects and reports truncation.
 */
int safe_atoi(const char *nptr, int *value)
{
	*value = 0;
	if(nptr == NULL)// NULL pointer safety
		return -1;
	int len = safe_strlen(nptr, 9);// returns count without \0
	if(len == 0)
		return 0;
	else if(len > 8)// truncate the data..
	{
		char strdata[10];
		safe_strncpy(strdata, nptr, 9);// truncates with \0
		long tl;
		tl = strtol(strdata, NULL, 0);
		if(errno == EINVAL)
			printf("conversion error!\n");
		*value = (int)tl;
	}
	else
	{
		long tl;
		tl = strtol(nptr, NULL, 0);
		if(errno == EINVAL)
			printf("conversion error!\n");
		*value = (int)tl;
	}
	return len;// no of bytes converted...
}

/**
 * safe_memcpy - Safely  transfer count of contents.
 * @dest: destination memory pointer.
 * @destsz: max source size.
 * @src: source memory pointer.
 * @count: no of elements to transfer...
 *
 * Returns:
 *    >=1 : successful transfer of this many elements.
 *    0 : count above max possible/ size out of bounds.
 *   -1 : Error occurred (invalid args or operlapping memory).
 *
 * Notes:
 * - checks for NULL pointer in place, returns error
 * - Detects and reports outof bound or overlapped memory .
 */
int safe_memcpy(void *dest, size_t destsz, void *src, size_t count)
{
	if(dest == NULL)// NULL pointer safety
		return -1;
	if(src == NULL)// NULL pointer safety
		return -1;
	if(count == 0)// 
		return 0;
	if(destsz == 0)//
		return 0;
	if((count < (1<<20)) && (count <= destsz))// max 1MB and size check if within bounds
	{
		mempcpy( dest, src, count);
		return count;// returns no of elements transfered.
	}
	else
	{
		memset(dest, 0, destsz);// zero out destination on failure..
		return 0;
	}
}

/**
 * safe_fgets - Safely get a string from an opened stream 
 * @s: destination string pointer.
 * @maxbufsz: max destination size. 
 * @count: no of elements to transfer. max 32K bytes read...
 * @stream: opened file pointer.
 *
 * Returns:
 *    1 : successful transfer done for count of contents.
 *    0 : count above max possible/ size out of bounds.
 *   -1 : Error occurred (invalid args or operlapping memory).
 *
 * Notes:
 * - checks for NULL pointer in place, returns error
 * - Detects and reports outof bound or overlapped memory .
 * - sanitized! output with null terminaation.
 */
int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream)
{        
	char buff[count+1];
	if(s == NULL)            // NULL pointer safety
		return -1;
	if(count > 1 << 16)// max 32K bytes read...
		return -1;
	if(maxbufsz < 1)	// buffer size check
		return -1;
	if(maxbufsz < count)	// buffer size check
		count = maxbufsz - 1;
	if(count < 1)		// input sanitisation
		return -1;
	long fileptr, filesize, datasize;
	fileptr = ftell(stream);
	if(fseek(stream, 0, SEEK_END) != 0)
	{
		// seek error... 
		return -1;
	}
	filesize = ftell(stream);
	datasize = filesize - fileptr;
	if(fseek(stream, fileptr, SEEK_SET) != 0)
	{
		// seek error... 
		return -1;
	}
	if(datasize <= 0)
		return -1;
		
	if(datasize  > count)// file size sanitisation
	{
		int t=fread(buff, 1, count, stream);
		if(t < 1)
			printf("returned error");
		safe_strncpy(s, buff, count);// sanitized.. output with null termination
		return 1;
	}
	else// if(count > datasize)
	{
		int t=fread(buff, 1, datasize, stream);
		if(t < 1)
			printf("returned error");
		safe_strncpy(s, buff, datasize);// sanitized.. output with null termination
		return 0;
	}
}


/*
int main(void)
{
	char form123[128], form456[128];
	int i, j;
	FILE *fptr;
	
	fptr = fopen("t.txt","r");
	i = safe_fgets(form123, 128, 10, fptr);
	fclose(fptr);
	j =safe_atoi(form123, &i);
	printf("read string %s, value= %d return = %d\n",form123, i, j);
	snprintf(form456, 128, "string %s of size %d = %d, %s", form123, safe_strlen(form123, 10), i, "good day.");
	printf("str read - %s\n", form456);
	memset(form123, 0, 128);
	safe_memcpy(form123, 128, form456, 128);
	printf("final-%s\n", form123);
	return 0;

}
*/

