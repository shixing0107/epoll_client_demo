#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>				// for std::thread
#include "bufferpool.h"
#include "rpevents.h"
#include <atomic>
#include "socketpacket.h"
#include <semaphore.h>



#define MSG_TCP_DATA 0x40001

class EpollTcpBase;

class CNetClient
{
public:
	CNetClient(void);
    ~CNetClient();

public:
    static void CallBackRecvData(void* pObj, int ret, const PacketPtr& data);
    static void CallBackSendDataRes(void* pObj, int msg_id, int msg_len, int msg_ret, int msg_write, int spend_time);

public:
    bool Init(const char *uIP, unsigned short uPort);               // 初始化客户端
    void UnInit();                                                  // 释放DLL资源

    // 接收数据包，暂时没什么用
    bool GetPacket(int nPacketIndex, NET_PACKET **pNetPacket);      // 获取数据包
    unsigned int GetSendPacketFifoSize();                           // 获取发送队列中数据大小
    int GetPacketCount();                                           // 获取数据包队列大小
    void DeletePacket(int nPacketIndex);                            // 释放数据包

    void SetCallBackFun(void *pContext, void *pFun);                // 设置回调函数

    bool GetConnectStatus();                                        // 获取连接状态 注:仅供外部调用接口,状态可能不实时
    bool AbortTran();                                               // 结束传输
    double GetTransportfSpeed();                                    // 获取发送速度 Mb/s

    bool SetDataTranStatus(bool bIsEnable);                         // 设置数据发送使能

    int submitPacket(NET_PACKET *pNetPacket);
    int recvDataSocketPacket(const PacketPtr& spPacket);

public:
    void ProcessDataThread();                                       // 数据包解析线程
    void SendPacketThread();                                        // 数据包发送线程
	void RunConnectDetectionThread();

public:
    static pid_t m_pidProcData;
    static pid_t m_pidConnDetect;
    static pid_t m_pidSendPacket;

protected:
    int convertNetPackToSockPack(NET_PACKET *pNetPacket);

private:
	static unsigned int ThreadProcessData(void *lparam);
	static unsigned int ThreadConnectDetection(void *lparam);
	static unsigned int ThreadSendPacket(void *lparam);



protected:
	int m_nErrorCode;						 // 错误代码
    BufferPool m_bufPool; // 缓冲池管理类

	bool m_bIsConnect;		  // 网络是否连接
	bool m_bIsRefreshConnect; // 是否刷新连接状态
	bool m_bLastStatus;		  // 记录上次连接状态

private:
    CNetPacketArray m_pNetPacketArray;      // 接收包队列
    std::mutex m_csArray;                   // 接收包队列互斥信号量
    std::mutex m_csIsConnect;

	bool m_bEndAllThread;
    bool m_bThreadsRunning[3];

	// 收到应答帧的信号
    smart_event_t m_evAck;
	// 收到心跳包的信号
    smart_event_t m_evLKA;

	std::string m_ulIp;		 // IP
	unsigned short m_usPort; // 端口

	bool m_IsRetransmission; // fifo满,表示下位机丢弃要数据,需要重传
	bool m_bAbortTran;		 // 是否结束传输

	bool m_bSentDataEn; //=1 使能数据,=0 则WriteData()数据直接丢弃

	double m_dbSentTotalData; // 统计发送总字节数
	double m_dbTotalTime;	  // 统计总时间 us
    unsigned int m_dwPackTimeStamp;
    std::mutex m_csPackTimeStamp;
	unsigned int m_dwLastPackTimeStamp;
	unsigned int m_dwLastCheckSum;
	unsigned int m_dwLastDataCrc;

    std::thread* m_pProcDataTh;
    std::thread* m_pConnDetectTh;
    std::thread* m_pSendPacketTh;

    std::atomic_uint32_t m_msgId = 0;
    std::mutex m_mutexSendingNetPacket;
    std::map<uint32_t, int> m_mapSendingNetPacketIndex;
    CNetPacketMap m_mapSendingNetPacket;

    std::mutex m_mutexSendNetPacket;
    sem_t m_semSendNetPacket;
    CNetPacketList m_listSendNetPacket;
    EpollTcpBase* m_epollTcp {nullptr};
    int32_t fd_ = 0;

};

#endif
