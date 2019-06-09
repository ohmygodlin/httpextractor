#pragma once

#include<Winsock2.h>
#include<stack>
using namespace std;

class SocketNode
{
public:
    static const int RECV_BUFF_LENGTH = 8*1024;
    SocketNode(void);
    ~SocketNode(void);
    SOCKET m_socket;
    char m_recv_buff[RECV_BUFF_LENGTH];
};

class SocketPool
{
public:
    static const int MAX_SOCKET_NUM = 16*1024;
    static const int MAX_IDLE_NUM = 16;
    static SocketNode* get_node();
    static void recycle_node(SocketNode*);
private:
    static stack<SocketNode*> m_socket_stack;
};
