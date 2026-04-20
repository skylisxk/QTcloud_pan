#include "mytcpserver.h"
#include <QDebug>

//创建十个线程
MyTcpServer::MyTcpServer() : m_threadPool(10)
{

}

MyTcpServer &MyTcpServer::getInstance()
{
    static MyTcpServer instance;
    return instance;
}

void MyTcpServer::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "新客户端连接，描述符:" << socketDescriptor;

    // ✅ 在主线程创建 socket
    MyTcpSocket* p_tcpSocket = new MyTcpSocket();

    if(p_tcpSocket->setSocketDescriptor(socketDescriptor)) {
        qDebug() << "客户端连接成功";

        // 线程池只用于文件传输等耗时操作
        p_tcpSocket->setThreadPool(&m_threadPool);

        {
            std::lock_guard<std::mutex> lock(m_threadPool.getMutex());
            tcpSocketList.append(p_tcpSocket);
        }

        connect(p_tcpSocket, &MyTcpSocket::offline,
                this, &MyTcpServer::deleteSocket);
    } else {
        delete p_tcpSocket;
    }
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

ThreadPool& MyTcpServer::getThreadPool(){

    return m_threadPool;
}
