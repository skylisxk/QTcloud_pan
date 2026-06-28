#include "privatechat.h"
#include "ui_privatechat.h"
#include "protocol.h"
#include "tcpclient.h"

PrivateChat::PrivateChat(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PrivateChat)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
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

    QString login_name = QString::fromUtf8(pdu->caData);
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

    QByteArray utf8Input = input.toUtf8();
    PDU* pdu = makePDU(utf8Input.size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST;

    //把双方的名字拷贝进去
    qstrncpy(pdu->caData, login_name.toUtf8().constData(), 32);
    qstrncpy(pdu->caData+32, des_name.toUtf8().constData(), 32);

    //把聊天信息拷贝
    qstrncpy((char*)pdu->caMsg, utf8Input.constData(), utf8Input.size()+1);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

