#ifndef BUFFER_POOL_MANAGER_H
#define BUFFER_POOL_MANAGER_H
#include "publicparam.h"
#include <mutex>
#include <atomic>
#include "rpevents.h"


unsigned char CheckSum(unsigned char *addr, int count);
unsigned short CheckSum_old(unsigned char *addr, int count); // 旧版本

class BufferPool
{
public:
    BufferPool(void);
    ~BufferPool(void);

public:
	bool Init(unsigned int dwBuffPoolSize);
	void UnInit();

	bool DataWrite(unsigned char *pBuffer, unsigned int dwSize);
	bool DataRead(unsigned char *pBuffer, unsigned int dwSize);

	void Cancel();
	void Restart();

	unsigned int GetWriteDataCount();
	unsigned int GetReadDataCount();

	bool PacketRead(NET_PACKET *pNetPacket);
	bool IsPacketReady(unsigned int dwSize);

protected:
	unsigned int m_dwBufferPoolSize;
	unsigned char *m_pDataBufPool;	  // 数据缓冲池
	unsigned int m_dwWritePtr;		  // 缓冲池内当前数据写进指针(接收的数据->数据缓冲池)
	unsigned int m_dwReadPtr;		  // 缓冲池内当前数据写出指针(数据缓冲池数据->写出到文件)
	unsigned int m_dwReadBlockCount;  // 读指针行程
	unsigned int m_dwWriteBlockCount; // 写指针行程
	unsigned int m_dwValidDataLen;	  // 有效数据长度

    std::mutex m_mutexDataBuf;				  // 互斥信号量
    smart_event_t m_evRead;
    smart_event_t m_evWrite;
    smart_event_t m_evCancel;

private:
	unsigned char m_pPacketHead[PACKET_HEAD_SIZE]; // 包头
};

#endif
