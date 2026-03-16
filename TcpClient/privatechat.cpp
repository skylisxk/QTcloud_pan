#include "privatechat.h"
#include "ui_privatechat.h"
#include "protocol.h"
#include "tcpclient.h"

PrivateChat::PrivateChat(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PrivateChat)
{
    ui->setupUi(this);
}

PrivateChat::~PrivateChat()
{
    delete ui;
}

void PrivateChat::setChatName(QString des_name)
{
    login_name = TcpClient::getInstance().loginName;
    this->des_name = des_name;
}

PrivateChat &PrivateChat::getInstance()
{
    static PrivateChat instance;
    return instance;
}

void PrivateChat::updateMsg(const PDU *pdu)
{
    if(!pdu){

        return;
    }

    char login_name[32] = {'\0'};
    memcpy(login_name, pdu->caData, 32);
    QString strMsg = QString("%1: %2").arg(login_name).arg((char*)pdu->caMsg);
    ui->showMsg->append(strMsg);

}

void PrivateChat::on_sendMsg_clicked()
{
    QString input = ui->inputMsg->text();
    ui->inputMsg->clear();

    if(input.isEmpty()){

        return;
    }

    PDU* pdu = makePDU(input.size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST;

    //把双方的名字拷贝进去
    memcpy(pdu->caData, login_name.toStdString().c_str(), login_name.size());
    memcpy(pdu->caData+32, des_name.toStdString().c_str(), des_name.size());

    //把聊天信息拷贝
    qstrcpy((char*)pdu->caMsg, input.toStdString().c_str());

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

