#include "mytcpserver.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>

//创建十个线程
MyTcpServer::MyTcpServer() : m_threadPool(10)
{
    // 启动检查定时器
    QTimer* checkTimer = new QTimer(this);
    checkTimer->setInterval(10000);  // 每 10 秒检查一次
    connect(checkTimer, &QTimer::timeout, this, &MyTcpServer::checkDeadConnections);
    checkTimer->start();
}

MyTcpServer::~MyTcpServer()
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

void MyTcpServer::checkDeadConnections()
{
    //tcpSocketList 是 MyTcpServer 中保存所有当前连接客户端的列表
    QMutableListIterator<MyTcpSocket*> iter(tcpSocketList);
    //遍历检查
    while (iter.hasNext()) {

        MyTcpSocket* socket = iter.next();
        //如果状态不是 ConnectedState（比如客户端已经主动断开、网络异常、服务器关闭连接等），则认为该连接已失效
        if (socket->state() != QAbstractSocket::ConnectedState) {

            qDebug() << "Socket 状态断开，清理:" << socket->getName();
            socket->clientOffline();
            socket->deleteLater();
            iter.remove();
        }
    }
}

ThreadPool& MyTcpServer::getThreadPool(){

    return m_threadPool;
}
