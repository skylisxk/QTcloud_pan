#include "mytcpserver.h"
#include <QDebug>


MyTcpServer::MyTcpServer()
{

}

MyTcpServer &MyTcpServer::getInstance()
{
    static MyTcpServer instance;
    return instance;
}

 //每次建立连接都会调用这个函数
void MyTcpServer::incomingConnection(qintptr socketDescriptor)
{
    //每当有新客户端连接到服务器时，自动创建一个专用的套接字对象来管理这个连接。
    MyTcpSocket* p_tcpSocket = new MyTcpSocket();
    p_tcpSocket->setSocketDescriptor(socketDescriptor);         // Qt 套接字对象接管一个已经存在的操作系统网络连接。
    tcpSocketList.append(p_tcpSocket);                          //将信息添加到列表

    connect(p_tcpSocket, &MyTcpSocket::offline, this, &MyTcpServer::deleteSocket);
    //connect(p_tcpSocket, SIGNAL(offline(MyTcpSocket*)), this, SLOT(deleteSocket(MyTcpSocket*)));    //建立连接,旧版写法
}

void MyTcpServer::transcation(const char *des_name, PDU* pdu)
{
    if(!des_name || !pdu || !*des_name){

        return;
    }

    QString cmpName = des_name;

    //遍历找到对应的名字
    for(auto& ele : tcpSocketList){

        if(ele->getName() == cmpName){

            //如果找到了，就写入到这个pdu里面,转发出去
            ele->write((char*)pdu, pdu->uiPDUlen);
            break;
        }
    }

}

void MyTcpServer::deleteSocket(MyTcpSocket *mySocket)
{
    if (tcpSocketList.removeOne(mySocket)) {
        mySocket->deleteLater();  // 使用deleteLater更安全
        qDebug() << "Socket已删除";
    }

    // 调试输出
    // for (MyTcpSocket* socket : tcpSocketList) {
    //     if (socket) {  // 安全检查
    //         qDebug() << socket->getName();
    //     }
    // }
}


