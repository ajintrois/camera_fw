//#include <fcntl.h>
//#include <spawn.h>
#include <stdio.h>
#include <string.h>
//#include <sys/wait.h>
#include <unistd.h>
#include <openssl/evp.h>
//#include <ctype.h>


int GenHash(FILE *rdfptr, int size, char *hash)
{
	int i;
	unsigned char tempbuf[1024];
	
	EVP_MD_CTX *shactx1 = EVP_MD_CTX_new();
	EVP_DigestInit_ex(shactx1, EVP_sha256(),NULL);
	i = 1024;
	while(i >= 1024)
	//while(size >= 1024)
	{
		i = 1024;
		if(size < 1024)
			i = size;
		i = fread(tempbuf, 1, i, rdfptr);
		EVP_DigestUpdate(shactx1, tempbuf, i);
		size -= i;
	}
	EVP_DigestFinal_ex(shactx1, hash, &i);
	EVP_MD_CTX_free(shactx1);
	return i;
}

void display_buffer (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j;
	char c;

	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		for(j = 0; j < 8; j++)
			printf(" %02x",(unsigned char) *(source+j+i*8));
		printf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
	}
}

int main(int argc, char *argv[]) 
{
	FILE 		*fpRead, *fpwrite, *fptr;
	unsigned char	c, buffer[1024];
	char 		firmware1[64] = {"no -f/w-"};
	char		hashgen[32], filehash[32];
	char 		cmd[1024];
	unsigned int 	id0, id1, file_size, wr_size;
	unsigned short	cs0;
	unsigned int 	tempid[2];
	int 		i = 0, j, k;
	cs0 = 0;
//===================================================================================================	
// load AVS application 
	id0 = 0x73519483;
	id1 = 0x63213748;
	wr_size = 0;
	fpwrite = fopen("firmware.tar.gz", "wb");
	if(fpwrite != NULL)
	{
		fpRead = fopen("../firmware.gz", "rb");
		if(fpRead != NULL)
		{
			if(fseek(fpRead, -80, SEEK_END) == 0)
			{
				//printf("Reading file fetching Details  Main                               +\n");
				file_size = ftell(fpRead);
				i = fread(tempid, 1, 8, fpRead);
				if((tempid[0] == id0)&&(tempid[1] == id1))
				{
					//printf("Ids Matching Found now checking Hash                              +\n");
					i = fread(firmware1, 1, 32, fpRead);
					//file_size = ftell(fpRead);
					i = fread(filehash, 1, 32, fpRead);
					i = fseek(fpRead, 0, SEEK_SET);
					// check hash...
					//display_buffer(filehash, 32);
					//printf("Ids Matching Found now checking Hash      %d                       +\n", file_size);
					i = GenHash(fpRead, file_size+40, hashgen);
					//display_buffer(hashgen, 32);
					i = 1;
					for(cs0 = 0; cs0 < 32; cs0++)
					{
						if(hashgen[cs0] != filehash[cs0])
							i = 0;
					}
					j = fseek(fpRead, 0, SEEK_SET);
					if(i == 1)
					{
						//printf("Hashes Matching file Ok Now Copying to file                       +\n");
						while(file_size > 0)
						{
							if(file_size <= 1024)
							{
								j = fread(buffer, 1, file_size, fpRead);
								k = fwrite(buffer, 1, file_size, fpwrite);
							}
							else
							{
								j = fread(buffer, 1, 1024, fpRead);
								k = fwrite(buffer, 1, j, fpwrite);
							}
							file_size -= j;
						}
						fclose(fpwrite);
					}
				}
			}
			fclose(fpRead);
		}
		fpRead = NULL;
		if(i == 0)
		{
			fseek(fpwrite, 0, SEEK_SET);
			fpRead = fopen("../firmwareBKUP.gz", "rb");
			if(fpRead != NULL)
			{
				if(fseek(fpRead, -80, SEEK_END) == 0)
				{
					//printf("Reading file fetching Details  Bkup                               +\n");
					file_size = ftell(fpRead);
					i = fread(tempid, 1, 8, fpRead);
					if((tempid[0] == id0)&&(tempid[1] == id1))
					{
						i = fread(firmware1, 1, 32, fpRead);
						i = fread(filehash, 1, 32, fpRead);
						i = fseek(fpRead, 0, SEEK_SET);
						// check hash...
						GenHash(fpRead, file_size+40, hashgen);
						i = 1;
						for(cs0 = 0; cs0 < 32; cs0++)
						{
							if(hashgen[cs0] != filehash[cs0])
								i = 0;
						}
						j = fseek(fpRead, 0, SEEK_SET);
						if(i == 1)
						{
							//printf("Hashes Matching file Ok Now Copying to file                       +\n");
							while(file_size > 0)
							{
								if(file_size <= 1024)
								{
									j = fread(buffer, 1, file_size, fpRead);
									k = fwrite(buffer, 1, file_size, fpwrite);
								}
								else
								{
									j = fread(buffer, 1, 1024, fpRead);
									k = fwrite(buffer, 1, j, fpwrite);
								}
								file_size -= j;
							}
							// copyfirmware and signature from backup to replace origianl
					 
							fptr = popen("cp ../firmwareBKUP.gz ../firmware.gz", "r");// will execute the commands in string..
							pclose(fptr);// close will wait for the process to terminate and return..
							sync();
							fptr = popen("cp ../firmwareBKUP.sig ../firmware.sig", "r");// will execute the commands in string..
							pclose(fptr);// close will wait for the process to terminate and return..
							sync();
						}
					}
				}
				fclose(fpRead);
			}
			fclose(fpwrite);
		}
	}
//===================================================================================================	
// store in file version.txt.. in home
	fpRead = fopen("version.txt", "wb");
	if(fpRead != NULL)
	{
		fprintf(fpRead,"%s \n",firmware1);
		fclose(fpRead);
	}

//===================================================================================================	
}
