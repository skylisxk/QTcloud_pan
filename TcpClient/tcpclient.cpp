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

#define REGIST_DONE "reigst success"
#define REGIST_FAIL "regist fail"
#define LOGIN_DONE "login success"
#define LOGIN_FAIL "login fail"
#define SEARCH_USR_NO "no such user"
#define SEARCH_USR_ONLINE "online"
#define SEARCH_USR_OFFLINE "offline"

#define FRIEND_EXIST "friend exist"
#define DELETE_FINISH "delete finish"

#define DIR_NOT_EXIST "dir not exist"
#define FILE_EXIST "file exist"
#define FILE_CREATE_DONE "file create success"
#define DIR_FILE_DELETE_DONE "delete success"
#define DIR_FILE_DELETE_FAIL "delete fail"
#define DIR_FILE_RENAME_DONE "rename success"
#define DIR_FILE_RENAME_FAIL "rename fail"
#define FILE_UPLOAD_DONE "file upload success"
#define FILE_UPLOAD_PROCESS "file uploading"
#define FILE_UPLOAD_FAIL "file upload fail"

#define UNKNOWN "unknown error"

TcpClient::TcpClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TcpClient)
{
    ui->setupUi(this);
    loadConfig();

    book = nullptr;
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
    delete ui;
}

void TcpClient::loadConfig()
{
    QFile file(":/client.config");

    if(!file.open(QIODevice::ReadOnly)){

        QMessageBox::critical(this, "open config", "fail");
        return;
    }

    QByteArray baData = file.readAll();
    QString strData = baData.toStdString().c_str();

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

void TcpClient::testConnect()
{
    QMessageBox::information(this, "connecting", "success");
}

void TcpClient::receiveMsg()
{

    unsigned int uiPDUlen = 0;                                  //总的长度
    tcpSocket.read((char*)&uiPDUlen, sizeof(unsigned int));     //将uint类型传入到uiPDUlen的第一个字符里面

    // 检查PDU长度合法性
    if(uiPDUlen < sizeof(PDU) || uiPDUlen > 10 * 1024 * 1024) {
        qDebug() << "非法PDU长度:" << uiPDUlen;
        tcpSocket.readAll();  // 清空缓冲区
        return;
    }

    unsigned int uiMsgLen = uiPDUlen - sizeof(PDU);             //实际消息长度
    PDU* pdu = makePDU(uiMsgLen);                               //构造函数,pdu是操作的一个对象
    tcpSocket.read((char*)pdu + sizeof(unsigned int), uiPDUlen - sizeof(unsigned int));       //实际操作的是地址，从第pdu+sizeof(uint)的地址开始传入，读取剩余的长度

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

            OpeWidget::getInstance().show();                        //跳转到成功后的窗口
            hide();

        }


        else if(strcmp(pdu->caData, LOGIN_FAIL) == 0){

            QMessageBox::warning(this, "Login", LOGIN_FAIL);

        }

        break;

    }

    case ENUM_MSG_TYPE_ALL_ONLINE_RESPOND:{

        OpeWidget::getInstance().getFriend()->showAllOnlineUsr(pdu);        //将pdu对象传入

        break;
    }

    case ENUM_MSG_TYPE_SEARCH_USR_RESPOND:{

        if(strcmp(SEARCH_USR_NO, pdu->caData) == 0){

            QMessageBox::information(this, "搜索", QString("%1: 不存在").arg(OpeWidget::getInstance().getFriend()->searchName));
            break;

        }else if (strcmp(SEARCH_USR_OFFLINE, pdu->caData) == 0){

            QMessageBox::information(this, "搜索", QString("%1: 不在线").arg(OpeWidget::getInstance().getFriend()->searchName));
            break;
        }

        QMessageBox::information(this, "搜索", QString("%1: 在线").arg(OpeWidget::getInstance().getFriend()->searchName));

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:{                               //添加好友请求

        char login_name[32] = {'\0'};
        PDU* res_pdu = makePDU();

        qstrncpy(login_name, pdu->caData+32, 32);

        int check = QMessageBox::information(this, "添加好友", QString("%1想要添加您的好友").arg(login_name),
                                             QMessageBox::Yes, QMessageBox::No);

        memcpy(res_pdu->caData+32, login_name, 32);
        memcpy(res_pdu->caData, pdu->caData, 32);                          //将des_name传入到respdu

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
        char login_name[32] = {'\0'};
        memcpy(login_name, pdu->caData, 32);
        QMessageBox::information(this, "删除好友", QString("%1已将你删除").arg(login_name));

        break;
    }

    case ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND:{                           //删除好友回复

        QMessageBox::information(this, "删除好友", "删除成功");
        break;
    }


    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND:{                             //更新好友列表

        OpeWidget::getInstance().getFriend()->updateFriendList(pdu);

        break;
    }

    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:{                             //聊天请求

        //将信息展示在聊天窗口
        if(PrivateChat::getInstance().isHidden()){

            PrivateChat::getInstance().show();
        }
        char login_name[32] = {'\0'};
        memcpy(login_name, pdu->caData, 32);

        PrivateChat::getInstance().setChatName((QString)login_name);
        PrivateChat::getInstance().updateMsg(pdu);
        break;
    }

    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:{                             //群聊请求

        OpeWidget::getInstance().getFriend()->updateGroupMsg(pdu);
        break;
    }

    case ENUM_MSG_TYPE_CREATE_DIR_RESPOND:{                             //创建目录回复

        QMessageBox::information(this, "创建文件夹", pdu->caData);
        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FILE_RESPOND:{                             //刷新文件回复

        //将收到的信息打印在屏幕上
        //将数据添加到booklist
        OpeWidget::getInstance().getBook()->updateFileList(pdu);
        break;
    }

    case ENUM_MSG_TYPE_DELETE_DIR_FILE_RESPOND:{                             //删除文件夹回复

        //QMessageBox::information(this, "删除文件夹", pdu->caData);
        break;
    }

    case ENUM_MSG_TYPE_RENAME_DIR_FILE_RESPOND:{

        QMessageBox::information(this, "重命名文件", pdu->caData);
        OpeWidget::getInstance().getBook()->flushFile();
        break;
    }

    case ENUM_MSG_TYPE_ENTER_DIR_RESPOND:{

        curPath = curPath + "/" + QString::fromUtf8(pdu->caData);
        OpeWidget::getInstance().getBook()->updateFileList(pdu);
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_PROCESS:{

        QMessageBox::information(this, "上传文件", pdu->caData);
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_FINISH:{

        QMessageBox::information(this, "上传完成", pdu->caData);

        OpeWidget::getInstance().getBook()->flushFile();

        QCoreApplication::processEvents();
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_RESPOND:{

        book->handleDownloadRespond(pdu);
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_ERROR:{

        book->handleDownloadError(pdu);
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_FINISH:{

        book->handleDownloadComplete();
        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_PROCESS:{

        book->handleDownloadData(pdu);
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_INFORM:{

        book->handleShareResponse(pdu);
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_DONE:{

        book->handleShareComplete();
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_RESPOND:{

        book->handleShareReceive();
        break;
    }

    default:

        break;

    }

    free(pdu);
    pdu = nullptr;

}

void TcpClient::onReadyRead()
{
    receiveMsg();
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
    // strncpy(pdu->caData, name.toStdString().c_str(), 32);
    // strncpy(pdu->caData+32, pwd.toStdString().c_str(), 32);
    std::copy_n(name.toStdString().c_str(),
                qMin(name.length(), 32),
                pdu->caData);

    std::copy_n(pwd.toStdString().c_str(),
                qMin(pwd.length(), 32),
                pdu->caData + 32);

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

    std::copy_n(name.toStdString().c_str(),
                qMin(name.length(), 32),
                pdu->caData);

    std::copy_n(pwd.toStdString().c_str(),
                qMin(pwd.length(), 32),
                pdu->caData + 32);

    tcpSocket.write((char*)pdu, pdu->uiPDUlen);
    free(pdu);
    pdu = NULL;
}


