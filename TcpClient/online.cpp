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

    //提取pdu数据
    char tp[32];
    unsigned int uiSize = pdu->uiMsglen / 32;

    //清屏操作
    ui->onlineList->clear();

    for(unsigned int i = 0; i < uiSize; i++){

        memcpy(tp, (char*)(pdu->caMsg) + i * 32, 32);

        ui->onlineList->addItem(tp);                                //onlinelist就是这个框的objectname
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
    memcpy(pdu->caData, desName.toStdString().c_str(), qMin(desName.size(),32));
    memcpy(pdu->caData+32, loginName.toStdString().c_str(), qMin(loginName.size(), 32));

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

