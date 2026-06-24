#include "tcpclient.h"
#include <QApplication>
#include <QFile>
#include <QString>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 加载全局样式表
    QFile qssFile(":/style/global.qss");
    if(qssFile.open(QIODevice::ReadOnly)){
        QString styleSheet = QString::fromUtf8(qssFile.readAll());
        a.setStyleSheet(styleSheet);
        qssFile.close();
    }

    TcpClient::getInstance().show();
    return a.exec();
    // QApplication a(argc, argv);

    // OpeWidget w;
    // w.show();

    // return a.exec();
}
