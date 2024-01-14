#ifndef TCPIP_H
#define TCPIP_H
#include "common_declare.h"
#include <mutex>
// #include <sdk/Interface/Communication/TcpIpVirtual.h>

class CNetClient;
typedef struct __NET_PACKET NET_PACKET;
class RS_CLASS_DECL TcpIp //:  public TcpIpVirtual
{
public:
	TcpIp();
	virtual ~TcpIp();

public:
	bool Init(const char *uIP, unsigned short uPort);		   // 初始化客户端
	void UnInit();											   // 释放DLL资源
	bool GetPacket(int nPacketIndex, NET_PACKET **pNetPacket); // 获取数据包
	bool SendPacket(NET_PACKET *pNetPacket);				   // 发送数据包
	int GetPacketCount();									   // 获取数据包队列大小
	void DeletePacket(int nPacketIndex);					   // 释放数据包

	bool GetConnectStatus();						 // 获取连接状态
	void SetCallBackFun(void *pContext, void *pFun); // 设置回调函数

	unsigned int GetSendPacketFifoSize(); // 获取发送队列中数据大小
	bool AbortTran();					  // 结束传输
	double GetTransportfSpeed();		  // 获取发送速度 Mb/s

	bool SetDataTranStatus(bool bIsEnable);

private:
    CNetClient *m_pNetClient;

    std::mutex m_csTcp;
};

#endif
