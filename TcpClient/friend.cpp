#include "friend.h"
#include "tcpclient.h"
#include "privatechat.h"
#include<QInputDialog>
#include<QTimer>

Friend::Friend(QWidget *parent)
    : QWidget{parent}
{
    textEdit = new QTextEdit;
    lineEdit = new QLineEdit;
    listWidget = new QListWidget;

    delFriendButton = new QPushButton("删除好友");
    flushFriendButton = new QPushButton("刷新列表");
    showOnlineUsrButton = new QPushButton("在线用户");
    searchUsrButton = new QPushButton("搜索用户");
    msgSendButton = new QPushButton("发送消息");
    privateChatButton = new QPushButton("私信对方");

    //创建垂直布局
    QVBoxLayout* rightPBL = new QVBoxLayout;
    rightPBL->addWidget(delFriendButton);
    rightPBL->addWidget(flushFriendButton);
    rightPBL->addWidget(searchUsrButton);
    rightPBL->addWidget(privateChatButton);
    rightPBL->addWidget(showOnlineUsrButton);

    //水平布局
    QHBoxLayout* topHBL = new QHBoxLayout;
    topHBL->addWidget(textEdit);
    topHBL->addWidget(listWidget);
    topHBL->addLayout(rightPBL);

    //添加消息输入框一栏
    QHBoxLayout* msgHBL = new QHBoxLayout;
    msgHBL->addWidget(lineEdit);
    msgHBL->addWidget(msgSendButton);

    QVBoxLayout* mainWidget = new QVBoxLayout;
    mainWidget->addLayout(topHBL);
    mainWidget->addLayout(msgHBL);

    pOnline = new Online;
    mainWidget->addWidget(pOnline);
    pOnline->hide();

    setLayout(mainWidget);

    //online.ui大小
    pOnline->setFixedSize(800, 400);
    //点击在线用户，会显示online.ui
    connect(showOnlineUsrButton, &QAbstractButton::clicked, this, &Friend::showOnline);     //这个button关联到showOnline这个函数
    connect(searchUsrButton, &QAbstractButton::clicked, this, &Friend::searchUsr);
    connect(flushFriendButton, &QAbstractButton::clicked, this, &Friend::flushFriend);
    connect(delFriendButton, &QAbstractButton::clicked, this, &Friend::deleteFriend);
    connect(privateChatButton, &QAbstractButton::clicked, this, &Friend::chatFriend);
    connect(msgSendButton, &QAbstractButton::clicked, this, &Friend::groupChat);

}

void Friend::showAllOnlineUsr(PDU *pdu)
{
    if(pdu == nullptr){

        return;
    }

    pOnline->showUsr(pdu);

}

void Friend::updateFriendList(PDU *pdu)                     //客户端接收并更新好友列表
{
    if(!pdu){

        return;
    }

    listWidget->clear();

    int uiSize = pdu->uiMsglen / 32;
    char login_name[32] = {'\0'};

    for(int i = 0; i < uiSize; i++){

        memcpy(login_name, (char*)(pdu->caMsg) + 32 * i, 32);

        //把结果输出到窗口
        listWidget->addItem(login_name);

    }

}

void Friend::updateGroupMsg(PDU *pdu)
{
    QString strMsg = QString("%1: %2").arg(pdu->caData).arg((char*)pdu->caMsg);
    //显示在窗口上
    textEdit->append(strMsg);
}

QListWidget *Friend::getListWidget()
{
    return listWidget;
}

void Friend::showOnline()
{   
    if(!pOnline->isHidden()){

        pOnline->hide();
        return;
    }

    pOnline->show();

    //封装请求信息
    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_REQUEST;
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    free(pdu);
    pdu = nullptr;
}

void Friend::searchUsr()
{
    searchName = QInputDialog::getText(this, "搜索", "用户名：");

    PDU* pdu = makePDU();

    pdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USR_REQUEST;
    memcpy(pdu->caData, searchName.toStdString().c_str(), searchName.size());
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    free(pdu);
    pdu = nullptr;
}

void Friend::flushFriend()
{
    //获取当前账户的用户名
    QString name = TcpClient::getInstance().loginName;
    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST;
    memcpy(pdu->caData, name.toStdString().c_str(), name.size());
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    delete pdu;
}

void Friend::deleteFriend()
{

    if(!listWidget->currentItem()){

        return;
    }

    QString des_name = listWidget->currentItem()->text();
    QString login_name = TcpClient::getInstance().loginName;

    PDU* pdu = makePDU();

    pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST;

    memcpy(pdu->caData, login_name.toStdString().c_str(), login_name.size());
    memcpy(pdu->caData+32, des_name.toStdString().c_str(), des_name.size());

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

void Friend::chatFriend()
{
    if(!listWidget->currentItem()){

        return;
    }
    QString des_name = listWidget->currentItem()->text();
    PrivateChat::getInstance().setChatName(des_name);

    //如果窗口隐藏了
    if(PrivateChat::getInstance().isHidden()){

        PrivateChat::getInstance().show();
    }
}

void Friend::groupChat()
{
    //strMsg获取聊天信息
    QString strMsg = lineEdit->text();

    if(strMsg.isEmpty()){
        return;
    }

    PDU* pdu = makePDU(strMsg.toUtf8().size()+1);
    QString login_name = TcpClient::getInstance().loginName;
    pdu->uiMsgType = ENUM_MSG_TYPE_GROUP_CHAT_REQUEST;

    qstrncpy(pdu->caData, login_name.toUtf8().constData(), 64);         //拷贝名字
    memcpy((char*)pdu->caMsg, strMsg.toUtf8().constData(), strMsg.toUtf8().size());             //拷贝内容
    ((char*)pdu->caMsg)[strMsg.toUtf8().size()] = '\0';        // 手动加结尾
    //发送到服务器
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

}

