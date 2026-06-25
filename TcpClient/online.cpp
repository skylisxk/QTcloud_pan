#include "online.h"
#include "ui_online.h"
#include "tcpclient.h"

Online::Online(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Online)
{
    ui->setupUi(this);
}

Online::~Online()
{
    delete ui;
}

void Online::showUsr(PDU *pdu)
{
    if(pdu == nullptr){

        return;
    }

    //提取pdu数据,包含所有在线的用户
    unsigned int uiSize = pdu->uiMsglen / 32;

    //清屏操作
    ui->onlineList->clear();

    for(unsigned int i = 0; i < uiSize; i++){

        QString userName = QString::fromUtf8((char*)(pdu->caMsg) + i * 32);
        ui->onlineList->addItem(userName);                                //onlinelist就是这个框的objectname
    }

}

void Online::on_addFriend_clicked()
{
    QListWidgetItem* item = ui->onlineList->currentItem();

    if(item == nullptr){

        //没选中直接退出
        return;
    }

    QString desName = item->text();
    QString loginName = TcpClient::getInstance().loginName;

    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_REQUEST;

    //前32放要查找的名字
    qstrncpy(pdu->caData, desName.toUtf8().constData(), 32);
    qstrncpy(pdu->caData+32, loginName.toUtf8().constData(), 32);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

