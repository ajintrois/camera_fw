#include <stdio.h>
#include <stdarg.h>
#include <stddef.h> // for size_t
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	FILE *fptr;
	unsigned char buffer[1024];
	int i;
	fptr = fopen("configenc.key", "rb");// will execute the commands in string..
	if(fptr != NULL)
	{
		i = 256;
		snprintf(buffer, 1024, "%s %d\n", "filename", i);
		printf("%s\n", buffer);
		i = fread(buffer,1, 96, fptr);
		fclose(fptr);// close will wait for the process to terminate and return..
		buffer[5] = 0x56;
		buffer[6] = 0x78;
		fptr = fopen("configenc1.key", "wb");// will execute the commands in string..
		if(fptr != NULL)
		{
			fwrite(buffer, 1, 96, fptr);
			fclose(fptr);// close will wait for the process to terminate and return..
		}
	}
	return 1;
}
