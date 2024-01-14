#include "socketpacket.h"
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////
///
///
Packet::Packet()
{

}

Packet::Packet(int fd, int id)
    : id_(id)
    , fd_(fd)
{

}

Packet::Packet(const unsigned char* msg, int len, int index)
{
    if (len <= 0)
        return;

    msg_ = (unsigned char*)malloc(sizeof(unsigned char) * len);
    if (msg_ == NULL)
    {
        return;
    }

    memset(msg_, 0, sizeof(unsigned char) * len);
    memcpy(msg_, msg, len);
    len_ = len;
    index_ = index;
}

Packet::Packet(int fd, int id, const unsigned char* msg, int len, int index)
    : id_(id)
    , fd_(fd)
    , len_(len)
    , index_(index)
{
    if (len <= 0)
        return;

    msg_ = (unsigned char*)malloc(len);
    if (msg_ == NULL)
    {
        return;
    }

    memset(msg_, 0, len);
    memcpy(msg_, msg, len);
}

Packet::Packet(const Packet& other)
{
    if (this == & other)
        return;
    if (other.len_ <= 0)
        return;

    if (this->msg_)
    {
        free(this->msg_);
        this->msg_ = nullptr;
    }
    this->msg_ = (unsigned char*)malloc(other.len_);
    if (msg_ == nullptr)
    {
        return;
    }

    this->fd_ = other.fd_;
    this->id_ = other.id_;
    this->len_ = other.len_;
    this->index_ = other.index_;
    memcpy(this->msg_, other.msg_, other.len_);
}

Packet& Packet::operator= (const Packet& other)
{
    if (this == & other)
        return *this;

    if (other.len_ <= 0)
        return *this;

    if (this->msg_)
    {
        free(this->msg_);
        this->msg_ = nullptr;
    }
    this->msg_ = (unsigned char*)malloc(other.len_);
    if (msg_ == nullptr)
    {
        return *this;
    }

    this->fd_ = other.fd_;
    this->id_ = other.id_;
    this->len_ = other.len_;
    this->index_ = other.index_;
    memcpy(this->msg_, other.msg_, other.len_);

    return *this;
}

Packet::~Packet()
{
    if (msg_)
    {
        free(msg_);
        msg_ = NULL;
    }
}
