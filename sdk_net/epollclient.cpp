#include "epollclient.h"
#include "rslogging.h"
#include "utils.h"
#include "publicparam.h"

#define MAX_EVENTS 10



/////////////////////////////////////////////
/// \brief EpollTcpClient::EpollTcpClient
/// \param server_ip
/// \param server_port
///

EpollTcpClient::EpollTcpClient()
{

}

EpollTcpClient::EpollTcpClient(const std::string& server_ip, uint16_t server_port)
    : server_ip_ { server_ip }
    , server_port_ { server_port }
{

}

EpollTcpClient::~EpollTcpClient()
{
    Stop();
}

bool EpollTcpClient::Start(const std::string& server_ip, uint16_t server_port)
{
    if (server_ip != "")
    {
        server_ip_ = server_ip;
    }
    if (server_port != 0)
    {
        server_port_ =   server_port;
    }

    // create epoll instance
    if (CreateEpoll() < 0)
    {
        LOG_ERROR_F("create epoll failed!");
        return false;
    }

    // create socket and bind
    int cli_fd  = CreateSocket();
    if (cli_fd < 0)
    {
        LOG_ERROR_F("create socket failed!");
        return false;
    }

    // connect to server
    int lr = Connect(cli_fd);
    if (lr < 0)
    {
        LOG_ERROR_F("create socket error!");
        return false;
    }
    LOG_INFO_F("EpollTcpClient Init success!");
    handle_ = cli_fd;

    // after connected successfully, add this socket to epoll instance, and focus on EPOLLIN and EPOLLOUT event
    int er = UpdateEpollEvents(efd_, EPOLL_CTL_ADD, handle_, EPOLLIN | EPOLLET);
    if (er < 0)
    {
        // if something goes wrong, close listen socket and return false
        LOG_ERROR_F("update epoll events failed!");
        ::close(handle_);
        // CloseSocket(fd);
        return false;
    }

    assert(!th_loop_);

    // the implementation of one loop per thread: create a thread to loop epoll
    th_loop_ = std::make_shared<std::thread>(&EpollTcpClient::EpollLoop, this);
    if (!th_loop_)
    {
        return false;
    }
    // detach the thread(using loop_flag_ to control the start/stop of loop)
    // th_loop_->detach();
    running_status_ = true;

    return true;
}


// stop epoll tcp client and release epoll
bool EpollTcpClient::Stop()
{
    loop_flag_ = false;
    running_status_ = false;
    th_loop_->join();
    ::close(handle_);
    // CloseSocket(fd);
    ::close(efd_);
    LOG_INFO_F("stop epoll!");

    while (send_queue_packet_.isEmpty() == false)
    {
        Packet data;
        int r = send_queue_packet_.Pop(&data);
        if (r == -1)
        {
            RSLOG_DEBUG << "the send queue packet pop failed!";
        }
        if (send_callback_)
        {
            // handle send packet
            send_callback_(send_obj_, data.id_, data.len_, -2, 0, 0); // 清空发送队列缓存
        }
    }

    UnRegisterOnRecvCallback();
    UnRegisterOnSendCallback();
    return true;
}

int32_t EpollTcpClient::CreateEpoll()
{
    // the basic epoll api of create a epoll instance
    int epollfd = epoll_create(1);
    if (epollfd < 0)
    {
        // if something goes wrong, return -1
        LOG_INFO_F("epoll_create failed!");
        return -1;
    }
    efd_ = epollfd;
    return epollfd;
}

int32_t EpollTcpClient::CreateSocket()
{
    // create tcp socket
    int cli_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (cli_fd < 0)
    {
        LOG_INFO_F("create socket failed!");
        return -1;
    }

    return cli_fd;
}

// connect to tcp server
int32_t EpollTcpClient::Connect(int32_t cli_fd , bool no_block)
{
    int ret = -1;
    struct sockaddr_in addr;  // server info
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port_);
    addr.sin_addr.s_addr  = inet_addr(server_ip_.c_str());

    // set socket no block
    if (no_block)
    {
        int flag = fcntl(efd_, F_GETFL, 0);
        if (fcntl(efd_, F_SETFL, flag | SOCK_NONBLOCK) == -1)
        {
            RSLOG_ERROR << "fcntl failed";
            close(efd_);
            return ret;
        }
    }

    // 为了处理EINTR,将connect放在循环内
    for (;;)
    {
        int ret = ::connect(cli_fd, (sockaddr*)&addr, sizeof(struct sockaddr_in));
        if (ret == 0)
        {
            RSLOG_INFO << "connect successfully";
            return 0;
        }
        else if (ret == -1)
        {
            // 非阻塞 connect 一般都是返回 -1 ， errno 设置为 EINPROGRESS
            if (errno == EINTR)
            {
                RSLOG_ERROR << "signal interrupt";
                continue;
            }
            else if (errno != EINPROGRESS)
            {
                RSLOG_ERROR << "can not connect to server, errno: " << errno;
                close(cli_fd);
                // CloseSocket(cli_fd);
                return ret;
            }
            else
            {
                UpdateEpollEvents(efd_, EPOLL_CTL_ADD, cli_fd, EPOLLOUT | EPOLLERR);
                break;
            }
        }
    }

    struct epoll_event* alive_events =  static_cast<epoll_event*>(calloc(kMaxEvents, sizeof(epoll_event)));
    if (!alive_events)
    {
        RSLOG_DEBUG << "calloc memory failed for epoll_events!";
        return -1;
    }

    /****************************************************************************************
    使用非阻塞 connect 需要注意的问题是：
        1. 很可能 调用 connect 时会立即建立连接（比如，客户端和服务端在同一台机子上），必须处理这种情况。
        2. Posix 定义了两条与 select 和 非阻塞 connect 相关的规定：
        1）连接成功建立时，socket 描述字变为可写。（连接建立时，写缓冲区空闲，所以可写）
        2）连接建立失败时，socket 描述字既可读又可写。 （由于有未决的错误，从而可读又可写）
    不过我同时用epoll也做了实验(connect一个无效端口，errno=110, errmsg=connect refused)，当连接失败的时候，会触发epoll的EPOLLERR与EPOLLIN，不会触发EPOLLOUT。
    当用select检测连接时，socket既可读又可写，只能在可读的集合通过getsockopt获取错误码。
    当用epoll检测连接时，socket既可读又可写，只能在EPOLLERR中通过getsockopt获取错误码。
    *****************************************************************************************
    *****************************************************************************************/
    for (;;)
    {
        ret = epoll_wait(efd_, alive_events, kMaxEvents, kEpollWaitTime);
        if (ret > 0)
        {
            for (int i = 0; i < ret; i++)
            {
                if (alive_events[i].events & EPOLLERR)
                {
                    RSLOG_ERROR << "EPOLLERR exists";
                    // 通过 socket 获取具体失败原因
                    int error;
                    socklen_t error_len = sizeof(int);
                    ret = getsockopt(alive_events[i].data.fd, SOL_SOCKET, SO_ERROR, &error, &error_len);
                    LOG_ERROR_F3("getsockopt connect failed, errno: [%d] %s", error, strerror(error));
                    ret = -1;
                }
                else if (alive_events[i].events & EPOLLOUT)
                {
                    RSLOG_INFO << "connect successful!!!";
                    ret = 0;
                }
                epoll_ctl(efd_, EPOLL_CTL_DEL, alive_events[i].data.fd, NULL);
            }
        }
        else if (ret == 0)
        {
            RSLOG_INFO << "epoll timeout";
        }
    }

    free(alive_events);
    RSLOG_INFO << "Connection established!";
    return ret;
}

void EpollTcpClient::CloseSocket(int32_t fd)
{
    linger ling;
    ling.l_onoff = 1;
    ling.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
    ::close(fd);
}

// add/modify/remove a item(socket/fd) in epoll instance(rbtree), for this example, just add a socket to epoll rbtree
int32_t EpollTcpClient::UpdateEpollEvents(int efd, int op, int fd, int events)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    LOG_INFO_F5("%s fd %d events read %d write %d", op == EPOLL_CTL_MOD ? "mod" : (op == EPOLL_CTL_ADD ? "add" : "del"),
                fd, ev.events & EPOLLIN, ev.events & EPOLLOUT);
    int r = epoll_ctl(efd, op, fd, &ev);
    if (r < 0)
    {
        RSLOG_ERROR << "epoll_ctl failed!";
        return -1;
    }
    return 0;
}

// register a callback when packet received
void EpollTcpClient::RegisterOnRecvCallback(callback_recv_t callback, void* obj)
{
    assert(!recv_callback_);
    recv_callback_ = callback;
    recv_obj_ = obj;
}

void EpollTcpClient::UnRegisterOnRecvCallback()
{
    assert(recv_callback_);
    recv_callback_  = nullptr;
    recv_obj_       = nullptr;

}

void EpollTcpClient::RegisterOnSendCallback(callback_send_t callback, void* obj)
{
    assert(send_callback_);
    send_callback_ = callback;
    send_obj_ = obj;
}

void EpollTcpClient::UnRegisterOnSendCallback()
{
    assert(send_callback_);
    send_callback_  = nullptr;
    send_obj_       = nullptr;
}

void EpollTcpClient::SetMaxDataLen(long max_len)
{
    if (max_len > 0)
        max_data_len_ = max_len;
}

long EpollTcpClient::GetMaxDataLen()
{
    return max_data_len_;
}

// handle read events on fd
void EpollTcpClient::OnSocketRead(int32_t fd)
{
    int err = SNEF_SUCCESS;
    char read_buf[4096];
    bzero(read_buf, sizeof(read_buf));
    int n = -1;
    int total_read_len = 0;
    int buf_len = 4096;
    unsigned char* buf = (unsigned char*)malloc( buf_len);
    if (buf == nullptr)
        return;
    bzero(buf, sizeof(char) * buf_len);
    unsigned long lastTime = 0;
    unsigned long startTime = GetTickCount();
    lastTime = startTime;
    while ((n = ::read(fd, read_buf, sizeof(read_buf))) > 0)
    {
        if (buf_len - total_read_len < n)
        {
            int tmp_len = buf_len + 4096;
            unsigned char* tmp = (unsigned char*)malloc(sizeof(char) * tmp_len);
            if (tmp == nullptr)
                return;
            bzero(tmp, tmp_len);
            memcpy(tmp, buf, total_read_len);
            memcpy(tmp+total_read_len, read_buf, n);
            buf_len = tmp_len;
            free(buf);
            buf = tmp;
        }
        else
        {
            memcpy(buf+total_read_len, read_buf, n);
        }
        total_read_len += n;
        bzero(read_buf, sizeof(read_buf));
        unsigned long curTime  = GetTickCount();
        if (curTime - lastTime >= 500)
        {
            LOG_DEBUG_F2("read datat 刷新间隔为%dms", curTime - lastTime);
        }
        lastTime = curTime;
    }
    unsigned long endTime = GetTickCount();
    LOG_DEBUG_F4("read data sendspeed : %0.3f Mb/s data=%.3lfmb,time=%lds",
                 (total_read_len / 1024.0 / 1024) / (endTime - startTime) * 1000,
                 total_read_len / 1024.0 / 1024, (endTime - startTime));
    if (n == -1)
    {
        err = SOCKET_ERROR;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // read finished
            return;
        }

        // something goes wrong for this fd, should close it
        ::close(fd);
        // CloseSocket(fd);
        return;
    }
    if (n == 0)
    {
        // this may happen when client close socket. EPOLLRDHUP usually handle this, but just make sure; should close this fd
        ::close(fd);
        // CloseSocket(fd);
        err = SNEF_CLIENT_CLOSE_CONNECTION;
        return;
    }
    // callback for recv
    PacketPtr data = std::make_shared<Packet>(fd, 0, buf, total_read_len);
    if (recv_callback_)
    {
        // handle recv packet
        recv_callback_(recv_obj_, err, data);
    }
    free(buf);
    buf = nullptr;
}

// handle write events on fd (usually happens when sending big files)
void EpollTcpClient::OnSocketWrite(int32_t fd)
{
    RSLOG_DEBUG << "fd: " << fd << " writeable!";
    Packet data(fd);
    int r = send_queue_packet_.Pop(&data);
    if (r == -1)
    {
        RSLOG_DEBUG << "the send queue packet pop failed!";
        return;
    }

    unsigned long startTime = GetTickCount();
    int total_w_len = 0;
    while (total_w_len < data.len_)
    {
        unsigned char *write_msg = data.msg_ + total_w_len;
        int remain_len = data.len_ - total_w_len;
        r = ::write(fd, write_msg, remain_len);
        if (r == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            // error happend
            ::close(fd);
            // CloseSocket(fd);
            RSLOG_DEBUG << "fd: " << fd << " write error, close it!";
            break;
        }
        else
        {
            total_w_len += r;
        }
    }
    unsigned long endTime = GetTickCount();
    LOG_DEBUG_F4("write data sendspeed : %0.3f Mb/s data=%.3lfmb,time=%lds",
                 (total_w_len / 1024.0 / 1024) / (endTime - startTime) * 1000,
                 total_w_len / 1024.0 / 1024, (endTime - startTime));
    int spend_time = endTime - startTime;
    if (total_w_len == data.len_)
    {
        r = 0;
        RSLOG_DEBUG << "send msg success";
    }

    if (send_callback_)
    {
        // handle send packet
        send_callback_(send_obj_, data.id_, data.len_, r, total_w_len, spend_time);
    }
}

int32_t EpollTcpClient::SendData(const PacketPtr& data)
{
    int ret = -1;
    if (running_status_ == false)
    {
        RSLOG_ERROR << "stop status, send failed!";
        return ret;
    }
    ret = send_queue_packet_.Push(*data.get());
    return ret;

    /*
    int r = ::write(handle_, data->msg.data(), data->msg.size());
    if (r == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -1;
        }
        // error happend
        ::close(handle_);
        RSLOG_DEBUG << "fd: " << handle_ << " write error, close it!";
        return -1;
    }
    return r;
    */
}

// one loop per thread, call epoll_wait and handle all coming events
void EpollTcpClient::EpollLoop()
{
    // request some memory, if events ready, socket events will copy to this memory from kernel
    struct epoll_event* alive_events =  static_cast<epoll_event*>(calloc(kMaxEvents, sizeof(epoll_event)));
    if (!alive_events)
    {
        RSLOG_DEBUG << "calloc memory failed for epoll_events!";
        return;
    }
    while (loop_flag_)
    {
        int num = epoll_wait(efd_, alive_events, kMaxEvents, kEpollWaitTime);

        for (int i = 0; i < num; ++i)
        {
            int fd = alive_events[i].data.fd;
            int events = alive_events[i].events;

            if ( (events & EPOLLERR) || (events & EPOLLHUP) )
            {
                RSLOG_DEBUG << "epoll_wait error!";
                // An error has occured on this fd, or the socket is not ready for reading (why were we notified then?).
                ::close(fd);
                // CloseSocket(fd);
            }
            else  if (events & EPOLLRDHUP)
            {
                // Stream socket peer closed connection, or shut down writing half of connection.
                // more inportant, We still to handle disconnection when read()/recv() return 0 or -1 just to be sure.
                RSLOG_DEBUG << "fd:" << fd << " closed EPOLLRDHUP!";
                // close fd and epoll will remove it
                ::close(fd);
                // CloseSocket(fd);
            }
            else if ( events & EPOLLIN )
            {
                // other fd read event coming, meaning data coming
                OnSocketRead(fd);
            }
            else if ( events & EPOLLOUT )
            {
                // write event for fd (not including listen-fd), meaning send buffer is available for big files
                OnSocketWrite(fd);
            }
            else
            {
                RSLOG_DEBUG << "unknow epoll event!";
            }
        } // end for (int i = 0; ...

    } // end while (loop_flag_)
    free(alive_events);
}
