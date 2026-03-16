#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QTcpServer>
#include <QList>
#include "mytcpsocket.h"


class MyTcpServer : public QTcpServer
{
    Q_OBJECT

private:

    QList<MyTcpSocket*> tcpSocketList;

public:
    MyTcpServer();

    static MyTcpServer &getInstance();

    void incomingConnection(qintptr socketDescriptor);
    void transcation(const char* des_name, PDU* pdu);                                   //转发名字

public slots:

    void deleteSocket(MyTcpSocket* mySocket);                                            //下线并删除socket
};

#endif // MYTCPSERVER_H
