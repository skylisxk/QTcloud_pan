#include "tcpserver.h"
#include <QApplication>
#include "operatedb.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    OperateDB::getInstance().init();
    TcpServer w;
    w.show();
    return a.exec();
}
