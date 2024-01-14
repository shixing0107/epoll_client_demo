#ifndef SOCKETPACKET_H
#define SOCKETPACKET_H

#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <string>
#include <iostream>
#include <memory>
#include <functional>


class Packet {
public:
    Packet();
    Packet(int fd, int id = 0);
    Packet(const unsigned char* msg, int len, int index = 0);
    Packet(int fd, int id, const unsigned char* msg, int len, int index = 0);
    Packet(const Packet& other);
    Packet& operator= (const Packet& other);
    ~Packet();

public:
    uint32_t id_ { 0 };
    int fd_ { -1 };         // meaning socket
    unsigned char* msg_ {NULL};
    uint32_t len_ {0};
    int index_ {0};
};

typedef std::shared_ptr<Packet> PacketPtr;

// callback when packet received
using callback_recv_t = std::function<void(void* pObj, int ret, const PacketPtr& data)>;
using callback_send_t = std::function<void(void* pObj, int msg_id, int msg_len, int msg_ret, int msg_write, int spend_time)>;

#endif // SOCKETPACKET_H
