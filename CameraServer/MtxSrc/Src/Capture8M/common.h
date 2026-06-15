#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "sv/sv.h"

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(array[0]))

int32_t QueryNumber(int32_t minInclusive, int32_t maxInclusive, int32_t defaultValue)
{
   return defaultValue;
}

bool CheckQueryBool(const char* value, uint32_t sizeValue, const char **checkedValue, int32_t sizeCheckedValue)
{
    return true;
}

bool QueryBool(bool defaultValue)
{
    return false;
}

void SelectPixelFormat(IControl* control)
{
    return;
}

void SelectFrameSize(IControl* frmsizControl)
{
    return;
}

ICamera* SelectCamera(const CICameraList *cameras, int32_t size)
{
    return (ICamera*)((*cameras)[0]);
}

int32_t SelectValue(const char *name, int32_t minValue, int32_t maxValue, int32_t defaultValue)
{
    return 0;//QueryNumber(minValue, maxValue, defaultValue);
}

bool SelectEnable(char *name, bool defaultValue)
{ 
    return true; //QueryBool(defaultValue);
}

int32_t SelectFromMenu(char *name, char menu[][64], uint32_t menuSize, uint32_t defaultIndex)
{
    return 0;//QueryNumber(0, menuSize - 1, defaultIndex);
}
   
void WaitForEnter()
{
    getchar();
}
