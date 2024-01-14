#include "bufferpool.h"
#include <unistd.h>
#include <string.h>
#include "rslogging.h"
#include "utils.h"

#define CHECKSUM_V0 0
#define CHECKSUM_V1 1


unsigned char g_byCheckSumFlag = CHECKSUM_V1;

BufferPool::BufferPool(void)
{
	m_pDataBufPool = NULL;
	m_dwWritePtr = 0;
	m_dwReadPtr = 0;
	m_dwReadBlockCount = 0;
	m_dwWriteBlockCount = 0;
    m_evRead    = CreateEvent(true);
    m_evWrite   = CreateEvent(true, true);;
    m_evCancel  = CreateEvent(true);
}

BufferPool::~BufferPool(void)
{
    DestroyEvent(m_evRead);
    DestroyEvent(m_evWrite);
    DestroyEvent(m_evCancel);
}

bool BufferPool::Init(unsigned int dwBuffPoolSize)
{
	if (m_pDataBufPool)
	{
		delete[] m_pDataBufPool;
		m_pDataBufPool = NULL;
	}
	m_dwBufferPoolSize = dwBuffPoolSize;
	m_pDataBufPool = new unsigned char[dwBuffPoolSize];
	memset(m_pDataBufPool, 0, dwBuffPoolSize);
	m_dwWritePtr = 0;
	m_dwReadPtr = 0;
	m_dwValidDataLen = 0;
	m_dwWriteBlockCount = 0;
	m_dwReadBlockCount = 0;

	return true;
}

bool BufferPool::DataWrite(unsigned char *pBuffer, unsigned int dwSize)
{
    if (m_pDataBufPool == nullptr)
    {
        RSLOG_ERROR << "this data buf pool is nullptr";
        return false;
    }

    smart_event_t evs[2] = {m_evCancel, m_evRead};
    int ret = WaitForMultipleEvents(evs, 2, false, WAIT_INFINITE);
    if (ret == 0)
    {
        RSLOG_DEBUG << "get exit sig, the event is set， return";
        SetEvent(m_evWrite);
        return false;
    }
    else if (ret == 1)
    {
        std::lock_guard locker(m_mutexDataBuf);
        if (m_dwWritePtr + dwSize < m_dwBufferPoolSize)
        {
            if (m_dwWritePtr > m_dwReadPtr)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataWrite::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, m_dwWritePtr, dwSize);
                memcpy(m_pDataBufPool + m_dwWritePtr, pBuffer, dwSize);
                m_dwWritePtr += dwSize;
                m_dwValidDataLen += dwSize;
                m_dwWriteBlockCount += dwSize;
                SetEvent(m_evRead);
            }
            else if (m_dwWritePtr == m_dwReadPtr && m_dwValidDataLen == 0)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataWrite::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, m_dwWritePtr, dwSize);
                memcpy(m_pDataBufPool + m_dwWritePtr, pBuffer, dwSize);
                m_dwWritePtr += dwSize;
                m_dwValidDataLen += dwSize;
                m_dwWriteBlockCount += dwSize;
                SetEvent(m_evRead);
            }
            else if ((m_dwWritePtr + dwSize) < m_dwReadPtr)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataWrite::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, m_dwWritePtr, dwSize);
                memcpy(m_pDataBufPool + m_dwWritePtr, pBuffer, dwSize);
                m_dwWritePtr += dwSize;
                m_dwValidDataLen += dwSize;
                m_dwWriteBlockCount += dwSize;
                SetEvent(m_evRead);
            }
            else
            {
                RSLOG_DEBUG << "m_dwWritePtr + dwSize ,, m_dwBufferPoolSize";
                ResetEvent(m_evRead);
                return false;
            }
        }
        else
        {
            RSLOG_DEBUG << "m_dwWritePtr + dwSize >= m_dwBufferPoolSize";
            if (m_dwWritePtr >= m_dwReadPtr)
            {
                if (((m_dwWritePtr + dwSize) % m_dwBufferPoolSize) <= m_dwReadPtr)
                {
                    LOG_TRACE_F4("CBufferPoolManager::DataWrite::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, m_dwWritePtr, m_dwBufferPoolSize-m_dwWritePtr);
                    memcpy(m_pDataBufPool + m_dwWritePtr, pBuffer, m_dwBufferPoolSize - m_dwWritePtr);
                    LOG_TRACE_F4("CBufferPoolManager::DataWrite::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, 0, dwSize-(m_dwBufferPoolSize-m_dwWritePtr));
                    memcpy(m_pDataBufPool, pBuffer + (m_dwBufferPoolSize - m_dwWritePtr), dwSize - (m_dwBufferPoolSize - m_dwWritePtr));
                    m_dwWritePtr = (m_dwWritePtr + dwSize) % m_dwBufferPoolSize;
                    m_dwValidDataLen += dwSize;
                    m_dwWriteBlockCount += dwSize;
                    SetEvent(m_evRead);
                }
                else
                {
                    ResetEvent(m_evRead);
                    RSLOG_DEBUG << "((m_dwWritePtr + dwSize) % m_dwBufferPoolSize) > m_dwReadPtr";
                    return false;
                }
            }
            else
            {
                ResetEvent(m_evRead);
                RSLOG_DEBUG << "m_dwWritePtr < m_dwReadPtr";
                return false;
            }
        }

    }

	return true;
}

bool BufferPool::DataRead(unsigned char *pBuffer, unsigned int dwSize)
{
    if (m_pDataBufPool == nullptr)
    {
        RSLOG_ERROR << "this data buf pool is nullptr";
        return false;
    }

    smart_event_t evs[2] = {m_evCancel, m_evWrite};
    int ret = WaitForMultipleEvents(evs, 2, false, WAIT_INFINITE);
    if (ret == 0)
    {
        RSLOG_DEBUG << "get exit sig, the event is set， return";
        SetEvent(m_evRead);
        return false;
    }
    else if (ret == 1)
    {
        std::lock_guard locker(m_mutexDataBuf);
        if (m_dwReadPtr + dwSize < m_dwBufferPoolSize)
        {
            RSLOG_DEBUG << "m_dwReadPtr + dwSize < m_dwBufferPoolSize";
            if (m_dwReadPtr + dwSize < m_dwWritePtr)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char", GetTickCount()/1000, m_dwReadPtr, dwSize);
                memcpy(pBuffer, m_pDataBufPool + m_dwReadPtr, dwSize);
                m_dwReadPtr += dwSize;
                m_dwValidDataLen -= dwSize;
                m_dwReadBlockCount += dwSize;
                SetEvent(m_evWrite);
            }
            else if (m_dwReadPtr + dwSize == m_dwWritePtr)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char", GetTickCount()/1000, m_dwReadPtr, dwSize);
                memcpy(pBuffer, m_pDataBufPool + m_dwReadPtr, dwSize);
                m_dwReadPtr += dwSize;
                m_dwValidDataLen -= dwSize;
                m_dwReadBlockCount += dwSize;
                SetEvent(m_evWrite);
            }
            else if (m_dwReadPtr > m_dwWritePtr)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char", GetTickCount()/1000, m_dwReadPtr, dwSize);
                memcpy(pBuffer, m_pDataBufPool + m_dwReadPtr, dwSize);
                m_dwReadPtr += dwSize;
                m_dwValidDataLen -= dwSize;
                m_dwReadBlockCount += dwSize;
                SetEvent(m_evWrite);
            }
            else if (m_dwReadPtr == m_dwWritePtr && m_dwValidDataLen >= dwSize)
            {
                LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char", GetTickCount()/1000, m_dwReadPtr, dwSize);
                memcpy(pBuffer, m_pDataBufPool + m_dwReadPtr, dwSize);
                m_dwReadPtr += dwSize;
                m_dwValidDataLen -= dwSize;
                m_dwReadBlockCount += dwSize;
                SetEvent(m_evWrite);
            }
            else
            {
                RSLOG_DEBUG << "m_dwReadPtr + dwSize > m_dwWritePtr";
                ResetEvent(m_evWrite);
                return false;
            }
        }
        else
        {
            if (m_dwReadPtr > m_dwWritePtr)
            {
                if (((m_dwReadPtr + dwSize) % m_dwBufferPoolSize) <= m_dwWritePtr)
                {
                    LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, m_dwReadPtr, m_dwBufferPoolSize-m_dwReadPtr);
                    memcpy(pBuffer, m_pDataBufPool + m_dwReadPtr, m_dwBufferPoolSize - m_dwReadPtr);
                    LOG_TRACE_F4("CBufferPoolManager::DataRead::[%d] Addr=%d Size= %d unsigned char\n", GetTickCount()/1000, 0, dwSize-(m_dwBufferPoolSize-m_dwReadPtr));
                    memcpy(pBuffer + (m_dwBufferPoolSize - m_dwReadPtr), m_pDataBufPool, dwSize - (m_dwBufferPoolSize - m_dwReadPtr));
                    m_dwReadPtr = (m_dwReadPtr + dwSize) % m_dwBufferPoolSize;
                    m_dwValidDataLen -= dwSize;
                    m_dwReadBlockCount += dwSize;
                    SetEvent(m_evWrite);
                }
                else
                {
                    RSLOG_DEBUG << "m_dwReadPtr > m_dwWritePtr";
                    ResetEvent(m_evWrite);
                    return false;
                }
            }
            else
            {
                RSLOG_DEBUG << "m_dwReadPtr > m_dwWritePtr";
                ResetEvent(m_evWrite);
                return false;
            }
        }

    }

    return true;
}

void BufferPool::UnInit()
{
	Cancel();
    RSLOG_DEBUG << "Cancel buffer pool";

    smart_event_t evs[3] = {m_evCancel, m_evWrite, m_evRead};
    WaitForMultipleEvents(evs, 3, true, WAIT_INFINITE);
    RSLOG_DEBUG << "Waited for all evevent exit!";

    {
        std::lock_guard locker(m_mutexDataBuf);
        if (m_pDataBufPool)
        {
            delete[] m_pDataBufPool;
            m_pDataBufPool = NULL;
        }
    }
}

void BufferPool::Cancel()
{
    SetEvent(m_evCancel);
}

unsigned int BufferPool::GetWriteDataCount()
{
	return m_dwWriteBlockCount;
}

unsigned int BufferPool::GetReadDataCount()
{
	return m_dwReadBlockCount;
}

// 新计算方式
unsigned char CheckSum_V1(unsigned char *addr, int count)
{
	register long sum = 0;

	while (count > 1)
	{
		/*  This is the inner loop */
		sum += *(unsigned char *)addr++;
		count -= 1;
	}

	/*  Fold 32-bit sum to 8 bits */
	while (sum >> 8)
	{
		sum = (sum & 0xff) + (sum >> 8);
	}

	return ~sum;
}

// 旧计算方式，有缺陷
unsigned short CheckSum_V0(unsigned char *addr, int count)
{
	/* Compute Internet Checksum for "count" unsigned chars
	 *         beginning at location "addr".
	 */
	register long sum = 0;

	while (count > 1)
	{
		/*  This is the inner loop */
		sum += *(unsigned short *)addr++;
		count -= 2;
	}

	/*  Add left-over unsigned char, if any */
	if (count > 0)
		sum += *(unsigned char *)addr;

	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

unsigned char CheckSum(unsigned char *addr, int count)
{
	if (g_byCheckSumFlag == CHECKSUM_V0)
	{
		return CheckSum_V0(addr, count);
	}
	else
	{
		return CheckSum_V1(addr, count);
	}
}

// 从接收缓存中获取一个完整包数据
bool BufferPool::PacketRead(NET_PACKET *pNetPacket)
{
    if (pNetPacket == nullptr)
    {
        RSLOG_ERROR << "net packet pointer is null";
        return false;
    }

	unsigned int dwProcess = PACKET_HEAD_SIZE;
	unsigned char pDataReadBuff[PACKET_HEAD_SIZE] = {0};
	unsigned char pDataProcessBuff[PACKET_HEAD_SIZE] = {0};
	unsigned char byCheckSum = 0;

    if (!IsPacketReady(PACKET_HEAD_SIZE))
        return false;

    if(!DataRead(pDataReadBuff, dwProcess))
    {
        LOG_TRACE_F3("CBufferPoolManager::PacketRead::读取数据.............(%d)(%d unsigned char)", GetTickCount()/1000, dwProcess);
        return false;
    }

    memcpy(pDataProcessBuff, pDataReadBuff, dwProcess);
    // 找到PACKET_HEAD_FLAG
    // 校验为完整包则退出
    byCheckSum = CheckSum(pDataProcessBuff, PACKET_HEAD_SIZE - 1);
    if (byCheckSum == pDataProcessBuff[PACKET_HEAD_SIZE - 1])
    {
        memset(m_pPacketHead, 0, PACKET_HEAD_SIZE);
        memcpy(m_pPacketHead, pDataProcessBuff, PACKET_HEAD_SIZE);
    }
    else
    {
        return false;
    }

    // 创建包
    unsigned char data[4] = {0};
    unsigned int dwOffset = 0;
    memset(data, 0, 4);

    memcpy(data, m_pPacketHead + dwOffset, 4);
    dwOffset += 4;
    pNetPacket->dwHead = *(unsigned int *)data;

    memset(data, 0, 4);
    memcpy(data, m_pPacketHead + dwOffset, 4);
    dwOffset += 4;
    pNetPacket->dwTimeStamp = *(unsigned int *)data;

    memset(data, 0, 4);
    memcpy(data, m_pPacketHead + dwOffset, 1);
    dwOffset += 1;
    pNetPacket->byType = *(unsigned char *)data;

    memset(data, 0, 4);
    memcpy(data, m_pPacketHead + dwOffset, 4);
    dwOffset += 4;
    pNetPacket->dwDataLen = *(unsigned int *)data;

    memset(data, 0, 4);
    memcpy(data, m_pPacketHead + dwOffset, 4);
    dwOffset += 4;
    pNetPacket->dwDataCrc = *(unsigned int *)data;

    memset(data, 0, 4);
    memcpy(data, m_pPacketHead + dwOffset, 1);
    dwOffset += 1;
    pNetPacket->dwCheckSum = *(unsigned char *)data;

    pNetPacket->pData = NULL;
    if (pNetPacket->dwDataLen > 0)
    {
        pNetPacket->pData = new unsigned char[pNetPacket->dwDataLen];
        if (pNetPacket->pData == NULL)
        {
            // 申请内存错误
            LOG_TRACE_F("====>申请内存错误!\r\n");
            return false;
        }
    }
    else
    {
        return true;
    }

    if (IsPacketReady(pNetPacket->dwDataLen))
    {
        RSLOG_ERROR << "read packet data is failed!";
        if (pNetPacket->pData)
        {
            delete[] pNetPacket->pData;
            pNetPacket->pData = NULL;
        }
        return false;
    }

    memset(pNetPacket->pData, 0, pNetPacket->dwDataLen);
    if( !DataRead(pNetPacket->pData, pNetPacket->dwDataLen))
    {
        RSLOG_ERROR << "read packet data  is failed!";
        return true;
    }

    return true;
}

bool BufferPool::IsPacketReady(unsigned int dwSize)
{
    std::lock_guard locker(m_mutexDataBuf);

	if (m_dwReadPtr + dwSize < m_dwBufferPoolSize)
	{
		if (m_dwReadPtr + dwSize <= m_dwWritePtr)
		{
			return true;
		}
		else if (m_dwReadPtr > m_dwWritePtr)
		{
			return true;
		}
	}
	else
	{
		if (((m_dwReadPtr + dwSize) % m_dwBufferPoolSize) < m_dwWritePtr)
		{
			return true;
		}
	}

	return false;
}

void BufferPool::Restart()
{
	memset(m_pDataBufPool, 0, m_dwBufferPoolSize);
	m_dwValidDataLen = 0;
	m_dwWriteBlockCount = 0;
    m_dwReadBlockCount = 0;
	m_dwWritePtr = 0;
	m_dwReadPtr = 0;
}
