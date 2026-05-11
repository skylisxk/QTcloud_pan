#include "tcpclient.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    TcpClient::getInstance().show();
    return a.exec();
    // QApplication a(argc, argv);

    // OpeWidget w;
    // w.show();

    // return a.exec();
}
