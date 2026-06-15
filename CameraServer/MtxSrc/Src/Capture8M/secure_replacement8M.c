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



