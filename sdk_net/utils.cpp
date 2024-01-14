#include "utils.h"
#include <time.h>
#include <unistd.h>


unsigned long GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);

    return(ts.tv_sec*1000 + ts.tv_nsec/1000000);
}

void msleep(int ms)
{
    usleep(ms * 1000);
}

long long GetCrc32(unsigned char *pData, unsigned int uiDataLen)
{
    long long Crc32Table[256];
    long long Crc;
    long long i, j;

    for (i = 0; i < 256; i++)
    {
        Crc = i;
        for (j = 0; j < 8; j++)
        {
            if (Crc & 1)
            {
                Crc = (Crc >> 1) ^ 0xEDB88320;
            }
            else
            {
                Crc >>= 1;
            }
        }
        Crc32Table[i] = Crc;
    }

    Crc = 0xffffffff;

    for (i = 0; i < uiDataLen; i++)
    {
        Crc = (Crc >> 8) ^ Crc32Table[(Crc & 0xFF) ^ pData[i]];
    }

    Crc ^= 0xFFFFFFFF;

    return Crc;
}
