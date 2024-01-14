#ifndef PUBLICPARAM_H
#define PUBLICPARAM_H
#include <vector>
#include <map>
#include <list>

#define MAX_SEND_QUEUE_SIZE 10                              // 3 分包时改为10		//发送打印数据队列缓存长度(1pass有数据包和信息包,故占用为2)
#define PACKET_HEAD_SIZE 18
#define PACKET_HEAD_FLAG_HS 0xFEFE555d

#define PACKET_HEAD_SIZE 18
#define PACKET_HEAD_FLAG 0xFEFE555d

#define MAX_DATA_RECEIVE_SIZE   5 * 1024 * 1024             // 5m	//5k
#define MAX_POOL_SIZE           20 * 1024 * 1024            // 20M
#define MAX_SEND_DATA_SIZE      256 * 1024                  // 512K
#define MAX_SEND_PARAM_SIZE     1024                        // 1024 unsigned char
#define MAX_TIME_OUT            5000                        // 等待超时时间(毫秒)

#define FT_DAT 0x0
#define FT_CMD 0x1
#define FT_NTY 0x2
#define FT_ACK 0x3
#define FT_GET 0x4
#define FT_LKA 0x5
#define FT_FUL 0x6

#define TYPE_CMD 1
#define TYPE_PASSDATA 2
#define TYPE_PASSINFO 3
#define TYPE_EXIT 4

#define INVALID_SOCKET (~0)
#define SOCKET_ERROR (-1)


typedef struct __NET_PACKET
{
    unsigned int dwHead             = 0;            // 包头标志(值为PACKET_HEAD_FLAG)
    unsigned int dwTimeStamp        = 0;            // 时间戳(标示)
    unsigned char byType            = 0;            // 类型
    unsigned int dwDataLen          = 0;            // 数据长度
    unsigned int dwDataCrc          = 0;            // 数据的32位CRC
    unsigned char dwCheckSum        = 0;            // 包头校验码(包头所有数据累加和的补码)
    unsigned char *pData            = nullptr;      // 数据内容
}NET_PACKET;

typedef std::vector<NET_PACKET* > CNetPacketArray;
typedef std::list<NET_PACKET*> CNetPacketList;
typedef std::map<uint32_t, NET_PACKET*> CNetPacketMap;


typedef enum __NET_ERROR_FLAG
{
    SNEF_SUCCESS = 0,					 // 通讯正常
    SNEF_SOCKET_INIT_FAILED = 1,		 // 初始化网络服务器失败
    SNEF_INVALID_SOCKET = 2,			 // 网络服务器失效
    SNEF_BIND_FAILED = 3,				 // 网络服务器绑定失败
    SNEF_LISTEN_START_FAILED = 4,		 // 开启监听失败,可能端口已被占用
    SNEF_RECEIVE_DATA_FAILED = 5,		 // 数据接收失败
    SNEF_CLIENT_CLOSE_CONNECTION = 6,	 // 客户端断开连接
    SNEF_CREATE_FILE_FAILED = 7,		 // 创建文件失败
    SNEF_WRITE_FILE_FAILED = 8,			 // 写数据失败
    SNEF_NETWORK_EXCETIPN = 9,			 // 网络数据接收出现异常
    SNEF_DATA_RECEIVE_FORMAT_ERROR = 10, // 接受的数据格式无效
    SNEF_WSANOTINITIALISED = 11,		 // 在使用此API之前应首先成功地调用WSAStartup()。
    SNEF_WSAENETDOWN = 12,				 // WINDOWS套接口实现检测到网络子系统失效。
    SNEF_WSAENOTCONN = 13,				 // 套接口未连接。
    SNEF_WSAEINTR = 14,					 // 阻塞进程被WSACancelBlockingCall()取消。
    SNEF_WSAEINPROGRESS = 15,			 // 一个阻塞的WINDOWS套接口调用正在运行中。
    SNEF_WSAENOTSOCK = 16,				 // 描述字不是一个套接口。
    SNEF_WSAEOPNOTSUPP = 17,			 // 指定了MSG_OOB，但套接口不是SOCK_STREAM类型的。
    SNEF_WSAESHUTDOWN = 18,				 // 套接口已被关闭。当一个套接口以0或2的how参数调用shutdown()关闭后，无法再用recv()接收数据。
    SNEF_WSAEWOULDBLOCK = 19,			 // 套接口标识为非阻塞模式，但接收操作会产生阻塞。
    SNEF_WSAEMSGSIZE = 20,				 // 数据报太大无法全部装入缓冲区，故被剪切。
    SNEF_WSAEINVAL = 21,				 // 套接口未用bind()进行捆绑。
    SNEF_WSAECONNABORTED = 22,			 // 由于超时或其他原因，虚电路失效。
    SNEF_WSAECONNRESET = 23,			 // 远端强制中止了虚电路。
    SNEF_CONNECT_FAILED = 24,			 // 连接服务器失败

} NET_ERROR_FLAG;

typedef enum __NET_CMD_DEFINE
{
    CMD_HEART_BEAT = 0,		  // 心跳包,每个若干秒钟发送一次到板卡,防止连接断开
    CMD_RW_MODUAL_PARAM = 1,  // 存储/读取指定模板参数
    CMD_R_MODUAL_IMAGE = 2,	  // 读取指定模板生成的图像数据
    CMD_R_MODUAL_STRING = 3,  // 读取指定模板生成的字符串数据
    CMD_RW_FONT = 4,		  // 存储/读取字库
    CMD_SET_UP_MACH_TYPE = 5, // 设置上位机类型(WINCE/PC)
    CMD_TCPIP_LOOP_TEST = 6,  // TCPIP通讯环接测试命令
    CMD_IMAGE_LOOP_TEST = 7,  // 图像数据环接测试命令
    CMD_PRINT_LOOP_TEST = 8,  // 打印数据环接测试命令
    CMD_SEND_PRINT_DATA = 9,  // 发送打印数据(作业)到板卡
    CMD_SEND_STRING = 10,	  // 板卡发送字符串信息命令
    CMD_UPDATE_SYS = 11,	  // （终端）升级板卡程序命令
    CMD_FPGA_REG = 12,		  // （终端）读写FPGA寄存器
    CMD_DEVICE_PARAM = 13,	  // （终端）配置/（终端）读取打印设备参数
    CMD_DEVICE_INFO = 14,	  // （服务端）发送/（终端）读取打印设备信息
    CMD_STOP_WORKING = 15,	  // （终端）工作中断命令
} NET_CMD_DEFINE;

typedef enum __NET_TRAN_MODE
{
    TRAN_MODE_SEND = 0,	 // 0:发送,1:请求,2:应答
    TRAN_MODE_APPLY = 1, // 0:发送,1:请求,2:应答
    TRAN_MODE_ACK = 2	 // 0:发送,1:请求,2:应答
} NET_NET_TRAN_MODE;

typedef enum __NET_SEND_PACKET_RET
{
    NET_SEND_SUCCESS = 0,		 // 发送成功
    NET_SEND_TIMEOUT = 1,		 // 发送超过
    NET_SEND_RETRANSMISSION = 2, // 要求重传
    NET_SEND_NOT_ACK = 3,		 // 未收到下位机应答
} NET_SEND_PACKET_RET;

#endif // PUBLICPARAM_H
