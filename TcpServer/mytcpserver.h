#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QTcpServer>
#include <QList>
#include "mytcpsocket.h"
#include "threadpool.h"


class MyTcpServer : public QTcpServer
{
    Q_OBJECT

private:

    QList<MyTcpSocket*> tcpSocketList;
    ThreadPool m_threadPool;

public:
    MyTcpServer();

    static MyTcpServer &getInstance();
    ThreadPool& getThreadPool();

    void incomingConnection(qintptr socketDescriptor);
    void transcation(const char* des_name, PDU* pdu);                                   //转发名字

public slots:

    void deleteSocket(MyTcpSocket* mySocket);                                            //下线并删除socket
};

#endif // MYTCPSERVER_H
