#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "netclient.h"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "rslogging.h"
#include "utils.h"
#include "epollclient.h"
#include "ringqueuepacket.h"

#define TCP_RECONNET_OUT_TIME (6000)
// #define TCP_RECONNET_OUT_TIME (600000)
#define MAX_SEND_PACKET_SIZE (0x100000 * 20) // 20M


#define ID_THREAD_PROCESS_DATA 0
#define ID_THREAD_SEND_PACKAGE 1
#define ID_THREAD_CONNECT_DETECTIOIN 2

pid_t CNetClient::m_pidProcData = 0;
pid_t CNetClient::m_pidConnDetect = 0;
pid_t CNetClient::m_pidSendPacket = 0;

void CNetClient::CallBackRecvData(void* pObj, int ret, const PacketPtr& data)
{
    // ToDo
    if (pObj == nullptr)
        return;

    CNetClient* pClient = (CNetClient*)pObj;
    if (pClient == nullptr)
        return;
    if (ret >= 0)
    {
        unsigned long startTime = GetTickCount();
        bool res = pClient->recvDataSocketPacket(data);
        RSLOG_DEBUG << "Recv data socket packet end, res: " << res << ", timespec: " << (GetTickCount() - startTime) / 1000;
    }
}


void CNetClient::CallBackSendDataRes(void* pObj, int msg_id, int msg_len, int msg_ret, int msg_write, int spend_time)
{
    // ToDo
    CNetClient* pClient = (CNetClient*)pObj;
    if (pClient == nullptr)
        return;

    std::lock_guard locker(pClient->m_mutexSendNetPacket);
    CNetPacketMap::iterator itPacket;
    std::map<uint32_t ,int>::iterator itIndex;
    {
        itPacket = pClient->m_mapSendingNetPacket.find(msg_id);

        itIndex = pClient->m_mapSendingNetPacketIndex.find(msg_id);
    }

    if (msg_ret >= 0)
    {
        if ((itPacket != pClient->m_mapSendingNetPacket.end())
            && itIndex != pClient->m_mapSendingNetPacketIndex.end())
        {
            RSLOG_DEBUG << "send packet data success, msg_id = " << msg_id << ", msg_index = " << itIndex->second;
        }
        else
        {
            RSLOG_DEBUG << "send packet data success!";
        }

    }
    else
    {
        if ((itPacket != pClient->m_mapSendingNetPacket.end())
            && itIndex != pClient->m_mapSendingNetPacketIndex.end())
        {
            RSLOG_DEBUG << "send packet data faield, msg_id = " << msg_id << ", msg_index = " << itIndex->second;
            // 重新提交数据包？
        }
        else
        {
            RSLOG_DEBUG << "send packet data failed!";
        }

    }

    if (itPacket != pClient->m_mapSendingNetPacket.end())
    {
        if (itPacket->second != nullptr)
        {
            if (itPacket->second->pData)
            {
                delete itPacket->second->pData;
                itPacket->second->pData = nullptr;
                delete itPacket->second;
                itPacket->second = nullptr;
            }
        }
        pClient->m_mapSendingNetPacket.erase(itPacket);
    }

    if (itIndex != pClient->m_mapSendingNetPacketIndex.end())
    {
        pClient->m_mapSendingNetPacketIndex.erase(itIndex);
    }
}


unsigned int CNetClient::ThreadProcessData(void *lparam)
{
    CNetClient::m_pidProcData = syscall(SYS_gettid);
	try
	{
		CNetClient *pNetClient = (CNetClient *)lparam;
		pNetClient->m_bThreadsRunning[ID_THREAD_PROCESS_DATA] = true;
		pNetClient->ProcessDataThread();
		pNetClient->m_bThreadsRunning[ID_THREAD_PROCESS_DATA] = false;
	}
	catch (...)
	{
        RSLOG_ERROR << "ThreadProcessData Exception!";
	}

	return 0;
}

unsigned int CNetClient::ThreadConnectDetection(void *lparam)
{
    CNetClient::m_pidConnDetect = syscall(SYS_gettid);
	try
	{
		CNetClient *pNetClient = (CNetClient *)lparam;
		pNetClient->m_bThreadsRunning[ID_THREAD_CONNECT_DETECTIOIN] = true;
		pNetClient->RunConnectDetectionThread();
		pNetClient->m_bThreadsRunning[ID_THREAD_CONNECT_DETECTIOIN] = false;
	}
	catch (...)
	{
        RSLOG_ERROR << "ThreadConnectDetection Exception!";
	}

	return 0;
}

unsigned int CNetClient::ThreadSendPacket(void *lparam)
{
    CNetClient::m_pidSendPacket = syscall(SYS_gettid);
	try
	{
		CNetClient *pNetClient = (CNetClient *)lparam;
		pNetClient->m_bThreadsRunning[ID_THREAD_SEND_PACKAGE] = true;
		pNetClient->SendPacketThread();
		pNetClient->m_bThreadsRunning[ID_THREAD_SEND_PACKAGE] = false;
	}
	catch (...)
	{
        RSLOG_ERROR << "ThreadSendPacket Exception!";
	}

	return 0;
}

CNetClient::CNetClient(void)
{
	m_bIsConnect = false;
	m_bIsRefreshConnect = true;
	m_bLastStatus = m_bIsConnect;

	m_IsRetransmission = false;
	m_bAbortTran = false;

	m_dbSentTotalData = 0;
	m_dbTotalTime = 0;

	m_bSentDataEn = true;

	m_bEndAllThread = false;
	m_bThreadsRunning[ID_THREAD_PROCESS_DATA] = false;
	m_bThreadsRunning[ID_THREAD_SEND_PACKAGE] = false;
	m_bThreadsRunning[ID_THREAD_CONNECT_DETECTIOIN] = false;

	m_dwPackTimeStamp = 0;
	m_dwLastPackTimeStamp = 0;
	m_dwLastCheckSum = 0;
	m_dwLastDataCrc = 0;

    sem_init(&m_semSendNetPacket, 0, 0);
    m_epollTcp = new EpollTcpClient();
    m_evAck = CreateEvent(true);
    m_evLKA = CreateEvent(true);
}

CNetClient::~CNetClient()
{
    sem_destroy(&m_semSendNetPacket);
    if (m_epollTcp)
    {
        delete m_epollTcp;
        m_epollTcp = nullptr;
    }

    DestroyEvent(m_evAck);
    DestroyEvent(m_evLKA);
}

bool CNetClient::Init(const char *uIP, unsigned short uPort)
{
	m_ulIp = uIP;
	m_usPort = uPort;

	m_bEndAllThread = false;

	// init buffer
    m_bufPool.Init(MAX_POOL_SIZE);

    m_bIsConnect = m_epollTcp->Start(m_ulIp, uPort);

    m_pProcDataTh = new std::thread(CNetClient::ThreadProcessData, this);
    m_pConnDetectTh = new std::thread(CNetClient::ThreadConnectDetection, this);
    m_pSendPacketTh = new std::thread(CNetClient::ThreadSendPacket, this);

	return true;
}

void CNetClient::UnInit()
{
	m_bEndAllThread = true;
	while (1)
	{
        if (m_bThreadsRunning[ID_THREAD_PROCESS_DATA] ||
              m_bThreadsRunning[ID_THREAD_SEND_PACKAGE] ||
              m_bThreadsRunning[ID_THREAD_CONNECT_DETECTIOIN])
		{
			break;
		}
        msleep(50);
	}

    m_pProcDataTh->join();
    m_pConnDetectTh->join();
    m_pSendPacketTh->join();


    CNetPacketArray::reverse_iterator it = m_pNetPacketArray.rbegin();
    while(it != m_pNetPacketArray.rend())
    {
        NET_PACKET *pPacket = *it;
        if (pPacket->pData)
        {
            delete[] pPacket->pData;
            pPacket->pData = nullptr;
        }
        delete pPacket;
        pPacket = nullptr;
        it++;
        CNetPacketArray::iterator tmpIt = m_pNetPacketArray.erase( --it.base());
        it = CNetPacketArray::reverse_iterator(tmpIt);
    }


    m_bufPool.UnInit();
    m_epollTcp->Stop();
}

bool CNetClient::GetPacket(int nPacketIndex, NET_PACKET **pNetPacket)
{
    std::lock_guard locker(m_csArray);
    int arrSize = m_pNetPacketArray.size();
    if (nPacketIndex < 0 || nPacketIndex >= arrSize)
    {
        return false;
    }

	if (!m_pNetPacketArray.empty())
    {
        if (nPacketIndex < arrSize)
		{
			*pNetPacket = m_pNetPacketArray[nPacketIndex];
		}
		else
		{
			return false;
		}

		return true;
	}
	else
	{
		return false;
	}
}


void CNetClient::DeletePacket(int nPacketIndex)
{
    std::lock_guard locker(m_csArray);
    int arrSize = m_pNetPacketArray.size();
    if (nPacketIndex < 0 || nPacketIndex >= arrSize)
    {
        return;
    }

	if (!m_pNetPacketArray.empty())
	{
		NET_PACKET *pNetPacket = m_pNetPacketArray[nPacketIndex];

		if (pNetPacket)
		{
			delete[] pNetPacket->pData;
			pNetPacket->pData = nullptr;

			delete pNetPacket;
			pNetPacket = nullptr;
		}

		m_pNetPacketArray.erase(m_pNetPacketArray.begin() + nPacketIndex);
	}
}

void CNetClient::SetCallBackFun(void *pContext, void *pFun)
{
    // m_pContext = pContext;
	// m_p_rx_proc = (FRAME_RX_PROC)pFun;
}


void CNetClient::ProcessDataThread()
{
    unsigned int lastTime = GetTickCount();
    unsigned int endTime = 0;
    while (!m_bEndAllThread)
    {
        unsigned int uiTimeCount = endTime - lastTime;
        if (uiTimeCount / 1000 > 2)
        {
            LOG_DEBUG_F2("ProcessDataThread update time:%dms ", uiTimeCount);
        }
        lastTime = endTime;

        NET_PACKET *pNetPacket = new NET_PACKET;
        pNetPacket->pData = nullptr;

        LOG_DEBUG_F2("CNetClient::m_bufPool.PacketRead()1(%d)", GetTickCount()/1000);
        bool bRet = m_bufPool.PacketRead(pNetPacket);
        LOG_DEBUG_F2("CNetClient::m_bufPool.PacketRead()2(%d)", GetTickCount()/1000);
        if (bRet)
        {
            if (pNetPacket->byType == FT_ACK)
            {
                RSLOG_DEBUG << "receive by type is FT_ACK";
                SetEvent(m_evAck);
                if (pNetPacket->pData)
                {
                    delete[] pNetPacket->pData;
                    pNetPacket->pData = nullptr;
                }
                delete pNetPacket;
                pNetPacket = nullptr;
            }
            else if (pNetPacket->byType == FT_LKA)
            {
                RSLOG_DEBUG << "receive by type is FT_LKA";
                SetEvent(m_evLKA);

                if (pNetPacket->pData)
                {
                    delete[] pNetPacket->pData;
                    pNetPacket->pData = nullptr;
                }
                delete pNetPacket;
                pNetPacket = nullptr;
            }
            if (pNetPacket->byType == FT_FUL)
            {
                RSLOG_DEBUG << "receive by type is FT_FUL";
                SetEvent(m_evAck);
                m_IsRetransmission = true;

                if (pNetPacket->pData)
                {
                    delete[] pNetPacket->pData;
                    pNetPacket->pData = nullptr;
                }
                delete pNetPacket;
                pNetPacket = nullptr;
            }
            else
            {
                unsigned int dwCalCRC = GetCrc32(pNetPacket->pData, pNetPacket->dwDataLen);
                if (dwCalCRC == pNetPacket->dwDataCrc)
                {
                    RSLOG_DEBUG << "Send event EVT_TCPIP_PROC";
                    // SendEvent(EVT_TCPIP_PROC, pNetPacket->pData, pNetPacket->dwDataLen);
                }
                if (pNetPacket->pData)
                {
                    delete[] pNetPacket->pData;
                    pNetPacket->pData = nullptr;
                }
                delete pNetPacket;
                pNetPacket = nullptr;
            }
        }
        else
        {
            if (pNetPacket->pData)
            {
                delete[] pNetPacket->pData;
                pNetPacket->pData = nullptr;
            }
            delete pNetPacket;
            pNetPacket = nullptr;
            RSLOG_ERROR << "read recv data failed!";
        }
    }

    RSLOG_DEBUG << "process thread exit!";
}

int CNetClient::GetPacketCount()
{
    std::lock_guard locker(m_csArray);
	int nArraySize = m_pNetPacketArray.size();

	return nArraySize;
}

void CNetClient::RunConnectDetectionThread()
{
    RSLOG_DEBUG << "Connect detection Th entry ...";
    while (!m_bEndAllThread)
    {
        RSLOG_DEBUG << "Wait for LKA EVENT entry";
        int ret = WaitForEvent(m_evLKA, 15000);
        RSLOG_DEBUG << "Wait for LKA EVENT leave, ret = " << ret;
        if (ret == 0)
        {
            RSLOG_DEBUG << "WaitForEvent is not timeout, m_bIsConnect is " << m_bIsConnect;
            if (m_bIsConnect != true)
            {
                std::lock_guard locker(m_csIsConnect);
                m_bIsConnect = true;
                RSLOG_DEBUG << "m_bIsConnect set true.";
            }
        }
        else if (ret == WAIT_TIMEOUT)
        {
            RSLOG_DEBUG << "WAIT_TIMEOUT, m_bIsConnect is " << m_bIsConnect;
            if (m_bIsConnect != false)
            {
                std::lock_guard locker(m_csIsConnect);
                m_bIsConnect = false;
                RSLOG_DEBUG << "m_bIsConnect set false.";
            }
        }
        ResetEvent(m_evLKA);
        RSLOG_DEBUG << "reset event lka.";
    }

    RSLOG_DEBUG << "Connect detection Th leave!";
}

bool CNetClient::GetConnectStatus()
{
	// 如果不刷新连接状态,则返回上次状态
	// 防止发送线程主动断开重连时,应用层出现连接断开
	if (!m_bIsRefreshConnect)
	{
		return m_bLastStatus;
	}

	m_bLastStatus = m_bIsConnect;

	return m_bIsConnect;
}

void CNetClient::SendPacketThread()
{
    int ret = 0;
    NET_PACKET *pNetPacket = nullptr;
    while (true)
    {
        ret = sem_wait(&m_semSendNetPacket);
        if (ret == EAGAIN)
        {
            LOG_INFO_F("the net packet queue is full!");
        }

        {
            std::lock_guard locker(m_mutexSendNetPacket);
            pNetPacket = m_listSendNetPacket.front();
            m_listSendNetPacket.pop_front();
        }

        if (pNetPacket->byType == TYPE_EXIT)
        {
            RSLOG_DEBUG << "get exit packet , break it";
            break;
        }

        ret = convertNetPackToSockPack(pNetPacket);
        if (ret > 0)
        {
            ret = WaitForEvent(m_evAck);
            if (ret == -1)
            {
                RSLOG_ERROR << "wait for event ack error!";
            }
            else
            {
               RSLOG_DEBUG << "waited for ack event it";
               LOG_DEBUG_F("TCP模块--->TCP发送包线程收到取消传输信号");
            }
            RSLOG_ERROR << "TCP模块--->发送数据网络成功!";
        }
        else
        {
            LOG_DEBUG_F("TCP--->TCP send pack faild!!!");
            LOG_DEBUG_F("TCP模块--->TCP发送请求包失败!!!");
        }

        if (pNetPacket->pData != nullptr)
        {
            delete[] pNetPacket->pData;
            pNetPacket->pData = nullptr;
        }
        delete pNetPacket;
        pNetPacket = nullptr;
    }


    LOG_DEBUG_F("Exit Send Net Packet thread");
}

unsigned int CNetClient::GetSendPacketFifoSize()
{
	NET_PACKET *pNetPacket = nullptr;
	unsigned int dwDataSize = 0;

    std::lock_guard locker(m_mutexSendNetPacket);
    CNetPacketList::iterator it = m_listSendNetPacket.begin();
    for (; it !=  m_listSendNetPacket.end(); it++)
	{
        pNetPacket = *it;
		if (pNetPacket != nullptr)
		{
			dwDataSize += (pNetPacket->dwDataLen + PACKET_HEAD_SIZE);
		}
	}

	return dwDataSize;
}

bool CNetClient::AbortTran()
{
	m_bAbortTran = true;

	return true;
}

double CNetClient::GetTransportfSpeed()
{
	return m_dbSentTotalData / m_dbTotalTime / 1000000;
}

bool CNetClient::SetDataTranStatus(bool bIsEnable)
{
	m_bSentDataEn = bIsEnable;

	return true;
}


int CNetClient::submitPacket(NET_PACKET *pNetPacket)
{
    int ret = -2;
    if (pNetPacket == nullptr)
    {
        LOG_ERROR_F("the net packet pointer is null!");
        return ret;
    }

    {
        std::lock_guard locker(m_mutexSendNetPacket);
        if (pNetPacket->byType == TYPE_CMD)
        {
            CNetPacketList::iterator it  = m_listSendNetPacket.begin();
            int index = 0;
            for (; it != m_listSendNetPacket.end(); it++)
            {
                if ((*it)->byType != TYPE_CMD)
                {
                    break;
                }
                index++;
            }

            m_listSendNetPacket.insert(it, pNetPacket);
            ret = index;
            sem_post(&m_semSendNetPacket);
        }
        else
        {
            if (m_listSendNetPacket.size() < MAX_SEND_QUEUE_SIZE)
            {
                ret = m_listSendNetPacket.size();
                m_listSendNetPacket.push_back(pNetPacket);
                sem_post(&m_semSendNetPacket);
            }
            else
            {
                LOG_ERROR_F("the queue is max send queue size!");
                ret = -1;
            }
        }
    }

    return ret;
}

int CNetClient::convertNetPackToSockPack(NET_PACKET *pNetPacket)
{
    std::vector<Packet> vecPacket;

    int ret = 0;
    NET_PACKET *pNewNetPacket = nullptr;

    unsigned int curPackLen = 0;
    unsigned int totalDataLen = 0;
    unsigned char packetType = 0;
    long maxSendDataSize = PACKET_HEAD_SIZE + MAX_SEND_PACKET_SIZE;
    m_epollTcp->SetMaxDataLen(maxSendDataSize);
    int packCount = 0;

    while (totalDataLen < pNetPacket->dwDataLen)
    {
        if (pNetPacket->dwDataLen - totalDataLen >= MAX_SEND_PACKET_SIZE)
        {
            curPackLen = MAX_SEND_PACKET_SIZE;
        }
        else
        {
            curPackLen = pNetPacket->dwDataLen - totalDataLen;
        }

        unsigned int offset = 0;
        unsigned int msgId = ++m_msgId;
        pNewNetPacket = new NET_PACKET;
        memcpy(pNewNetPacket, pNetPacket, sizeof(NET_PACKET));
        pNewNetPacket->dwDataLen = curPackLen;
        pNewNetPacket->pData = new unsigned char[PACKET_HEAD_SIZE + curPackLen];
        if (nullptr == pNewNetPacket->pData)
        {
            delete pNewNetPacket;
            ret = -1;
            break;
        }

        switch (pNewNetPacket->byType)
        {
        case TYPE_CMD:
        case TYPE_PASSINFO:
            packetType = TYPE_CMD;
            break;
        case TYPE_PASSDATA:
            packetType = TYPE_PASSDATA;
            break;
        }
        char checkSum = CheckSum(pNewNetPacket->pData, PACKET_HEAD_SIZE - 1);
        unsigned dataCrc = GetCrc32(pNetPacket->pData + totalDataLen, pNewNetPacket->dwDataLen);

        pNewNetPacket->dwHead = PACKET_HEAD_FLAG_HS;
        pNewNetPacket->dwTimeStamp = msgId;
        pNewNetPacket->dwDataCrc = dataCrc;
        memset(pNewNetPacket->pData, 0, PACKET_HEAD_SIZE);
        memcpy(pNewNetPacket->pData, &pNewNetPacket->dwHead, 4);
        offset += 4;
        memcpy(pNewNetPacket->pData + offset, &pNewNetPacket->dwTimeStamp, 4);
        offset += 4;
        memcpy(pNewNetPacket->pData + offset, &packetType, 1);
        offset += 1;
        memcpy(pNewNetPacket->pData + offset, &pNewNetPacket->dwDataLen, 4);
        offset += 4;
        memcpy(pNewNetPacket->pData + offset, &pNewNetPacket->dwDataCrc, 4);
        offset += 4;
        pNewNetPacket->pData[PACKET_HEAD_SIZE - 1] = checkSum;
        offset += 1;
        memcpy(pNewNetPacket->pData + PACKET_HEAD_SIZE, pNetPacket->pData + totalDataLen, pNewNetPacket->dwDataLen);
        pNewNetPacket->dwCheckSum = checkSum;

        LOG_DEBUG_F4("TCP模块--->检测包数据校验信息, msgId=%d, CheckSum=%d, DataCrc=%d", msgId, checkSum, dataCrc);

        offset = 0;
        int dataLen = PACKET_HEAD_SIZE + curPackLen;
        unsigned char* data = (unsigned char*) malloc(dataLen);
        if (data == nullptr)
        {

            delete pNewNetPacket->pData;
            pNetPacket->pData = nullptr;
            delete pNewNetPacket;
            pNewNetPacket = nullptr;
            ret = -1;
            break;
        }
        memset(data, 0, dataLen);
        memcpy(data, &pNewNetPacket->dwHead, 4);
        offset += 4;
        memcpy(data + offset, &msgId, 4);
        offset += 4;
        memcpy(data + offset, &packetType, 1);
        offset += 1;
        memcpy(data + offset, &dataLen, 4);
        offset += 4;
        memcpy(data + offset, &dataCrc, 4);
        offset += 4;
        memcpy(data + offset, &checkSum, 1);
        offset += 1;
        memcpy(data + PACKET_HEAD_SIZE, pNetPacket->pData + totalDataLen, curPackLen);

        totalDataLen += curPackLen;

        {
            std::lock_guard locker(m_mutexSendingNetPacket);
            m_mapSendingNetPacket.insert(std::make_pair(msgId, pNetPacket));
            m_mapSendingNetPacketIndex.insert(std::make_pair(msgId, packCount));
        }
        std::shared_ptr<Packet> spPacket(new Packet(fd_, msgId, data, dataLen, packCount++));
        m_epollTcp->SendData(spPacket);
    }

    return ret;
}

int CNetClient::recvDataSocketPacket(const PacketPtr& spPacket)
{
    if (spPacket->len_ > 0)
        return -1;
    bool res = m_bufPool.DataWrite(spPacket->msg_, spPacket->len_);

    RSLOG_DEBUG << "recv data, fd = " << spPacket->fd_ << ", msg id = " << spPacket->id_ << "， msg len = " << spPacket->len_;

    return res == true ? 0 : -1;
}
