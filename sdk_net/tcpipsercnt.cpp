#include "tcpipsercnt.h"
#include "netclient.h"

TcpIp::TcpIp()
{
    m_pNetClient = new CNetClient();
}

TcpIp::~TcpIp()
{
    if (m_pNetClient)
    {
        delete m_pNetClient;
        m_pNetClient = nullptr;
    }
}

// 客户端
bool TcpIp::Init(const char *uIP, unsigned short uPort)
{
    if (m_pNetClient->Init(uIP, uPort))
	{
		return true;
	}

	return false;
}

void TcpIp::UnInit()
{
    m_pNetClient->UnInit();
}

bool TcpIp::GetPacket(int nPacketIndex, NET_PACKET **pNetPacket)
{
	bool bRet = false;
	for (int i = 0; i < 3; i++)
    {
        std::lock_guard locker(m_csTcp);
        bRet = m_pNetClient->GetPacket(nPacketIndex, pNetPacket);

		if (bRet)
		{
			return true;
		}
	}
	return bRet;
}

bool TcpIp::SendPacket(NET_PACKET *pNetPacket)
{
    int ret = m_pNetClient->submitPacket(pNetPacket);

    return ret;
}

void TcpIp::SetCallBackFun(void *pContext, void *pFun)
{
	// 设置回调函数
    m_pNetClient->SetCallBackFun(pContext, pFun);
}

void TcpIp::DeletePacket(int nPacketIndex)
{
    std::lock_guard locker(m_csTcp);
    m_pNetClient->DeletePacket(nPacketIndex);
}

int TcpIp::GetPacketCount()
{
    return m_pNetClient->GetPacketCount();
}

bool TcpIp::GetConnectStatus()
{
    return m_pNetClient->GetConnectStatus();
}

unsigned int TcpIp::GetSendPacketFifoSize()
{
    return m_pNetClient->GetSendPacketFifoSize();
}

bool TcpIp::AbortTran()
{
    return m_pNetClient->AbortTran();
}

double TcpIp::GetTransportfSpeed()
{
    return m_pNetClient->GetTransportfSpeed();
}

bool TcpIp::SetDataTranStatus(bool bIsEnable)
{
    return m_pNetClient->SetDataTranStatus(bIsEnable);
}
