#ifndef UTILS_H
#define UTILS_H

unsigned long GetTickCount();

void msleep(int ms);

long long GetCrc32(unsigned char *pData, unsigned int uiDataLen);

#endif // UTILS_H
