#include "tcpserver.h"
#include "ui_tcpserver.h"
#include <QFile>
#include <QMessageBox>
#include <QByteArray>
#include <QHostAddress>
#include <QDebug>
#include "mytcpserver.h"

TcpServer::TcpServer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpServer)
{
    ui->setupUi(this);

    loadConfig();

    //监听
    MyTcpServer::getInstance().listen(QHostAddress(ip), port);
}

TcpServer::~TcpServer()
{
    delete ui;
}

void TcpServer::loadConfig()
{
    QFile file(":/server.config");

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
