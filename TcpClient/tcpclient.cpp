#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QByteArray>
#include <QDebug>
#include <QMessageBox>
#include <QHostAddress>
#include <QLineEdit>
#include <Qtmath>
#include "privatechat.h"
#include <QOverload>
#include <QTimer>
#include "protocol.h"



TcpClient::TcpClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TcpClient)
    , m_threadPool(4)
{
    ui->setupUi(this);
    // ★调试便利：预填默认登录账号密码
    ui->name_le->setText("jack");
    ui->pwd_le->setText("123");
    loadConfig();

    book = nullptr;
    pFriend = nullptr;
    curPath = "./";
    rootPath = "./";


    //connect(&tcpSocket, SIGNAL(readyRead()), this, SLOT(receiveMsg()));                 //为received函数添加信号槽
    connect(&tcpSocket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(&tcpSocket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(&tcpSocket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(&tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        onError(error);
    });


    //连接服务器
    tcpSocket.connectToHost(QHostAddress(ip), port);


}

TcpClient::~TcpClient()
{
    qDebug() << "TcpClient 析构开始";

    // 先断开所有 socket 信号，防止清理过程触发错误弹窗
    disconnect(&tcpSocket, nullptr, this, nullptr);
    m_shuttingDown = true;

    // 兜底：如果 OpeWidget 的 closeEvent 没触发到，这里再发一次下线请求
    // 只有 socket 确实连接着才写，避免触发 BrokenPipe
    if (!loginName.isEmpty()
        && tcpSocket.state() == QAbstractSocket::ConnectedState) {
        PDU* pdu = makePDU();
        pdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_OUT_REQUEST;
        qstrncpy(pdu->caData, loginName.toUtf8().constData(), 64);
        tcpSocket.write((char*)pdu, pdu->uiPDUlen);
        tcpSocket.flush();
        free(pdu);
    }

    // book 和 pFriend 由 OpeWidget 管理生命周期，这里只置空
    // （OpeWidget 析构函数先于本析构函数执行，已经 delete 过了）
    book = nullptr;
    pFriend = nullptr;

    // 关闭 socket
    if(tcpSocket.isOpen()) {
        tcpSocket.disconnectFromHost();
        if (tcpSocket.state() != QAbstractSocket::UnconnectedState) {
            tcpSocket.waitForDisconnected(1500);
        }
    }

    delete ui;
    qDebug() << "TcpClient 析构完成";

}

void TcpClient::loadConfig()
{
    QFile file(":/client.config");

    if(!file.open(QIODevice::ReadOnly)){

        QMessageBox::critical(this, "open config", "fail");
        return;
    }

    QByteArray baData = file.readAll();
    QString strData = QString::fromUtf8(baData);

    file.close();

    QStringList strList = strData.replace("\r\n", " ").split(" ");

    ip = strList.at(0);
    port = strList.at(1).toUShort();

}

TcpClient &TcpClient::getInstance()
{
    static TcpClient instance;
    return instance;
}

QTcpSocket &TcpClient::getTcpSocket()
{
    return tcpSocket;
}

QString TcpClient::getServerIp()
{
    return ip;
}

quint16 TcpClient::getServerPort()
{
    return port;
}

void TcpClient::onReadyRead()
{
    receiveMsg();   // 原函数，处理一个PDU

}


void TcpClient::receiveMsg()
{
    // 循环读取，直到没有完整的 PDU 可读（解决 TCP 黏包问题）
    while (tcpSocket.bytesAvailable() >= sizeof(unsigned int)) {

        unsigned int uiPDUlen = 0;

        tcpSocket.peek((char*)&uiPDUlen, sizeof(unsigned int));
        if (uiPDUlen < sizeof(PDU) || uiPDUlen > 10*1024*1024) {

            // 非法，跳过字节
            char c;
            tcpSocket.read(&c, 1);
            continue;
        }
        if (tcpSocket.bytesAvailable() < uiPDUlen)  return;

        unsigned int uiMsgLen = uiPDUlen - sizeof(PDU);
        PDU* pdu = makePDU(uiMsgLen);
        tcpSocket.read((char*)pdu, uiPDUlen);

        handlePdu(pdu);

        free(pdu);
    }
}


void TcpClient::handlePdu(PDU* pdu){

    switch(pdu->uiMsgType){

    case ENUM_MSG_TYPE_REGIST_RESPOND:{                             //如果是注册请求

        if(strcmp(pdu->caData, REGIST_DONE) == 0){                  //如果注册成功

            QMessageBox::information(this, "Regist", REGIST_DONE);

        }

        else if(strcmp(pdu->caData, REGIST_FAIL) == 0){

            QMessageBox::warning(this, "Regist", REGIST_FAIL);

        }

        break;

    }

    case ENUM_MSG_TYPE_LOGIN_RESPOND:{

        if(strcmp(pdu->caData, LOGIN_DONE) == 0){                  //如果登录成功

            //记录当前的文件路径
            curPath = QString("./%1").arg(loginName);
            rootPath = curPath;
            book = OpeWidget::getInstance().getBook();
            pFriend = OpeWidget::getInstance().getFriend();

            book->setThreadPool(&m_threadPool);
            pFriend->setThreadPool(&m_threadPool);

            OpeWidget::getInstance().show();                        //跳转到成功后的窗口
            hide();

        }


        else if(strcmp(pdu->caData, LOGIN_FAIL) == 0){

            QMessageBox::warning(this, "Login", LOGIN_FAIL);

        }

        break;

    }

    case ENUM_MSG_TYPE_LOGIN_OUT_RESPOND:{
        // 关闭过程中不弹窗，避免阻塞退出流程
        if (!m_shuttingDown) {
            QMessageBox::information(this, "Login Out", "login out success");
        }
        break;
    }

    case ENUM_MSG_TYPE_ALL_ONLINE_RESPOND:{                         //在线请求

        pFriend->showAllOnlineUsr(pdu);
        break;
    }

    case ENUM_MSG_TYPE_SEARCH_USR_RESPOND:{                         //搜索回复

        if(strcmp(SEARCH_USR_NO, pdu->caData) == 0){

            QMessageBox::information(this, "搜索", QString("%1: 不存在").arg(pFriend->searchName));
            break;

        }else if (strcmp(SEARCH_USR_OFFLINE, pdu->caData) == 0){

            QMessageBox::information(this, "搜索", QString("%1: 不在线").arg(pFriend->searchName));
            break;
        }

        QMessageBox::information(this, "搜索", QString("%1: 在线").arg(pFriend->searchName));

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:{                               //添加好友请求

        QString login_name = QString::fromUtf8(pdu->caData+32);
        QString des_name   = QString::fromUtf8(pdu->caData);
        PDU* res_pdu = makePDU();

        int check = QMessageBox::information(this, "添加好友", QString("%1想要添加您的好友").arg(login_name),
                                             QMessageBox::Yes, QMessageBox::No);

        qstrncpy(res_pdu->caData+32, login_name.toUtf8().constData(), 32);
        qstrncpy(res_pdu->caData, des_name.toUtf8().constData(), 32);

        (check == QMessageBox::Yes) ? res_pdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_AGREE :
            res_pdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REFUSE;

        tcpSocket.write((char*)res_pdu, res_pdu->uiPDUlen);

        delete res_pdu;

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_RESPOND:{                               //添加好友回复

        QMessageBox::information(this, "添加好友", pdu->caData);

        break;
    }

    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:{                           //删除好友请求

        //显示删除方名字
        QString login_name = QString::fromUtf8(pdu->caData);
        QMessageBox::information(this, "删除好友", QString("%1已将你删除").arg(login_name));

        break;
    }

    case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:{                           //删除好友回复

        QMessageBox::information(this, "删除好友", "删除成功");
        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:{                             //更新好友列表

        pFriend->updateFriendList(pdu);

        break;
    }

    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:{                             //聊天请求

        //将信息展示在聊天窗口
        if(PrivateChat::getInstance().isHidden()){

            PrivateChat::getInstance().show();
        }
        QString login_name = QString::fromUtf8(pdu->caData);

        PrivateChat::getInstance().setChatName(login_name);
        PrivateChat::getInstance().updateMsg(pdu);
        break;
    }

    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:{                             //群聊请求

        pFriend->updateGroupMsg(pdu);
        break;
    }

    case ENUM_MSG_TYPE_CREATE_DIR_RESPOND:{                             //创建目录回复

        QMessageBox::information(this, "创建文件夹", pdu->caData);
        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:{                             //刷新文件回复

        //将收到的信息打印在屏幕上
        //将数据添加到booklist
        book->updateFileList(pdu);
        break;
    }

    case ENUM_MSG_TYPE_DELETE_DIR_FILE_RESPOND:{                             //删除文件夹回复

        //QMessageBox::information(this, "删除文件夹", pdu->caData);
        break;
    }

    case ENUM_MSG_TYPE_RENAME_DIR_FILE_RESPOND:{                             //重命名

        QMessageBox::information(this, "重命名文件", pdu->caData);
        book->flushFile();
        break;
    }

    case ENUM_MSG_TYPE_ENTER_DIR_RESPOND:{                                  //进入文件夹

        curPath = curPath + "/" + QString::fromUtf8(pdu->caData);
        book->updateFileList(pdu);
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_PROCESS:{                                     //上传处理

        qDebug() << "Received upload ready, starting data transmission";
        book->handleUploadRespond(pdu);
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_FINISH:{                                      //上传完成

        // 只有服务端的确认才能标记上传真正完成，重置客户端状态
        book->onServerUploadFinish();
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_RESPOND:{                                   //下载回复

        qDebug() << "Receive DOWNLOAD_RESPOND, book=" << book;

        book->handleDownloadRespond(pdu);
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_ERROR:{                                     //下载报错

        // 将错误信息传递给 Book（可由 Worker 的错误信号处理，这里直接通知界面）
        QString errorMsg = QString::fromUtf8(pdu->caData);

        QMetaObject::invokeMethod(book, "onDownloadError",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, errorMsg));

        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_FINISH:{                                    //下载完成

        qDebug() << "Received DOWNLOAD_FINISH from server, ignore";

        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_PROCESS:{                                   //下载处理

        book->handleDownloadProcess(pdu);

        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_INFORM:{                                  //分享通知

        book->handleShareResponse(pdu);
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_DONE:{                                    //分享完成

        book->handleShareComplete();
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_RESPOND:{                                 //分享回复

        book->handleShareReceive();
        break;
    }

    default:

        break;

    }
}

void TcpClient::clearSocketBuffer()
{
    while (tcpSocket.bytesAvailable() > 0) {
        tcpSocket.readAll();
    }
    qDebug() << "Socket buffer cleared";
}


void TcpClient::onConnected()
{
    qDebug() << "成功连接到服务器";

}

void TcpClient::onDisconnected()
{
     qDebug() << "与服务器断开连接";
}

void TcpClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "socket错误:" << error;
    // 关闭过程中不弹窗，避免阻塞退出
    if (m_shuttingDown) return;
    // 服务端主动断开是正常行为，不弹窗骚扰用户
    if (error == QAbstractSocket::RemoteHostClosedError) return;
    QMessageBox::warning(this, "连接错误", "无法连接到服务器");
}

void TcpClient::on_regist_pb_clicked()
{

    QString name = ui->name_le->text();
    QString pwd = ui->pwd_le->text();

    if(name.isEmpty() || pwd.isEmpty()){


        QMessageBox::warning(this, "send", "null");
        return;
    }

    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_REGIST_REQUEST;

    //cadata放置账户和密码
    QByteArray utf8Name = name.toUtf8();
    QByteArray utf8Pwd  = pwd.toUtf8();
    qstrncpy(pdu->caData, utf8Name.constData(), 32);
    qstrncpy(pdu->caData+32, utf8Pwd.constData(), 32);

    tcpSocket.write((char*)pdu, pdu->uiPDUlen);
    free(pdu);
    pdu = NULL;

}

void TcpClient::on_login_clicked()
{
    QString name = ui->name_le->text();
    QString pwd = ui->pwd_le->text();

    if(name.isEmpty() || pwd.isEmpty()){

        QMessageBox::warning(this, "send", "null");
        return;
    }

    loginName = name;
    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_REQUEST;

    //cadata放置账户和密码
    QByteArray utf8Name = name.toUtf8();
    QByteArray utf8Pwd  = pwd.toUtf8();
    qstrncpy(pdu->caData, utf8Name.constData(), 32);
    qstrncpy(pdu->caData+32, utf8Pwd.constData(), 32);

    tcpSocket.write((char*)pdu, pdu->uiPDUlen);
    free(pdu);
    pdu = NULL;
}

void TcpClient::on_logout_pb_clicked()
{
    QString name = ui->name_le->text();

    if(name.isEmpty()){

        return;
    }

    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_OUT_REQUEST;

    //放置用户名
    qstrncpy(pdu->caData, name.toUtf8().constData(), 64);

    tcpSocket.write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

