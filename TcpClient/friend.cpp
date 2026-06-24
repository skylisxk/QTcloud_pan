#include "friend.h"
#include "tcpclient.h"
#include "privatechat.h"
#include<QInputDialog>
#include<QTimer>

Friend::Friend(QWidget *parent)
    : QWidget{parent}
{
    textEdit = new QTextEdit;
    textEdit->setReadOnly(true);
    textEdit->setPlaceholderText("群聊消息将显示在这里...");
    textEdit->setStyleSheet(
        "QTextEdit {"
        "   background-color: #fafafa;"
        "   border: 1px solid #e4e7ed;"
        "   border-radius: 6px;"
        "   padding: 10px;"
        "   font-size: 13px;"
        "}"
    );

    lineEdit = new QLineEdit;
    lineEdit->setFixedHeight(36);
    lineEdit->setPlaceholderText("输入群聊消息，按回车发送...");
    lineEdit->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid #e4e7ed;"
        "   border-radius: 18px;"
        "   padding: 4px 16px;"
        "   background-color: #ffffff;"
        "}"
        "QLineEdit:focus {"
        "   border-color: #409eff;"
        "}"
    );

    listWidget = new QListWidget;
    listWidget->setMaximumWidth(200);
    listWidget->setMinimumWidth(160);
    listWidget->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #e4e7ed;"
        "   border-radius: 6px;"
        "}"
    );

    delFriendButton = new QPushButton("删除好友");
    delFriendButton->setObjectName("delFriendButton");

    flushFriendButton = new QPushButton("刷新列表");
    showOnlineUsrButton = new QPushButton("在线用户");
    searchUsrButton = new QPushButton("搜索用户");
    msgSendButton = new QPushButton("发送消息");
    privateChatButton = new QPushButton("私信对方");

    //创建垂直布局
    QVBoxLayout* rightPBL = new QVBoxLayout;
    rightPBL->setSpacing(8);
    rightPBL->addWidget(delFriendButton);
    rightPBL->addWidget(flushFriendButton);
    rightPBL->addWidget(searchUsrButton);
    rightPBL->addWidget(privateChatButton);
    rightPBL->addWidget(showOnlineUsrButton);
    rightPBL->addStretch();

    //水平布局
    QHBoxLayout* topHBL = new QHBoxLayout;
    topHBL->setSpacing(10);
    topHBL->addWidget(textEdit, 1);
    topHBL->addWidget(listWidget, 0);
    topHBL->addLayout(rightPBL, 0);

    //添加消息输入框一栏
    QHBoxLayout* msgHBL = new QHBoxLayout;
    msgHBL->addWidget(lineEdit, 1);
    msgHBL->addWidget(msgSendButton);

    pOnline = new Online;
    //online.ui大小
    pOnline->setMinimumSize(600, 250);

    pFriendStackWidget = new QStackedWidget;
    // 空白占位，索引0
    pFriendStackWidget->addWidget(new QWidget);
    // 索引1
    pFriendStackWidget->addWidget(pOnline);
    // 切换到索引0
    pFriendStackWidget->setCurrentIndex(0);
    //初始隐藏
    pFriendStackWidget->setMaximumHeight(0);

    QVBoxLayout* mainWidget = new QVBoxLayout;
    mainWidget->setSpacing(8);
    mainWidget->setContentsMargins(12, 12, 12, 12);
    mainWidget->addLayout(topHBL);
    mainWidget->addLayout(msgHBL);
    mainWidget->addWidget(pFriendStackWidget);

    setLayout(mainWidget);

    //点击在线用户时，展开显示
    connect(showOnlineUsrButton, &QAbstractButton::clicked, [=](){

        if(pFriendStackWidget->currentIndex() == 0){

            // 切换页面
            pFriendStackWidget->setCurrentIndex(1);
            // 展开
            pFriendStackWidget->setMaximumHeight(300);
        }
        else{

            pFriendStackWidget->setCurrentIndex(0);
            pFriendStackWidget->setMaximumHeight(0);
        }
    });

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

    int uiSize = pdu->uiMsglen / 32;
    /************************多线程********************/
    // 判断是否使用线程池
    bool useThreadPool = (m_threadPool != nullptr && uiSize > 5);

    if(useThreadPool){
        //拷贝pdu避免被释放
        QByteArray rawData((const char*)pdu->caMsg, pdu->uiMsglen);

        m_threadPool->enqueue([this, rawData, uiSize](){

            QStringList friend_list;
            friend_list.reserve(uiSize);

            for(int i = 0; i < uiSize; i++){

                QString login_name = QString::fromUtf8(rawData.data() + i * 32, 32).trimmed();
                friend_list.append(login_name);

            }

            QMetaObject::invokeMethod(this, [this, friend_list](){

                listWidget->clear();

                for(auto& name : friend_list){
                    qDebug() << name;
                    listWidget->addItem(name);
                }

            }, Qt::QueuedConnection);

        });

    }

    else{

        //获取好友名称
        listWidget->clear();

        for(int i = 0; i < uiSize; i++){

            QString login_name = QString::fromUtf8((char*)(pdu->caMsg) + 32 * i, 32).trimmed();

            //把结果输出到窗口
            listWidget->addItem(login_name);

        }
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

void Friend::setThreadPool(ThreadPool *pool)
{
    m_threadPool = pool;
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
    qstrncpy(pdu->caData, searchName.toUtf8().constData(), 32);
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
    qstrncpy(pdu->caData, name.toUtf8().constData(), 32);
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

    qstrncpy(pdu->caData, login_name.toUtf8().constData(), 32);
    qstrncpy(pdu->caData+32, des_name.toUtf8().constData(), 32);

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
    QByteArray utf8Msg = strMsg.toUtf8();
    memcpy((char*)pdu->caMsg, utf8Msg.constData(), utf8Msg.size());             //拷贝内容
    ((char*)pdu->caMsg)[strMsg.toUtf8().size()] = '\0';        // 手动加结尾
    //发送到服务器
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

}

