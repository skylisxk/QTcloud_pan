#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QMainWindow>
#include <QFile>
#include <QTcpSocket>
#include <QIODevice>
#include "protocol.h"
#include "opewidget.h"
#include "book.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class TcpClient;
}
QT_END_NAMESPACE

class TcpClient : public QMainWindow
{
    Q_OBJECT

public:
    TcpClient(QWidget *parent = nullptr);
    ~TcpClient();
    void loadConfig();

    //单例模式
    static TcpClient& getInstance();
    QTcpSocket& getTcpSocket();
    //记录当前登录的用户名
    QString loginName;
    //记录文件夹路径
    QString curPath;
    QString rootPath;
    Book* book;
    Friend* pFriend;

public slots:

    void testConnect();
    void receiveMsg();
    void onReadyRead();                                                 //读文件
    void onConnected();                                                 // 连接成功时触发
    void onDisconnected();                                              // 断开连接时触发
    void onError(QAbstractSocket::SocketError error);                   // 错误处理

private slots:
    //测试用void on_sendpb_clicked();

    void on_regist_pb_clicked();

    void on_login_clicked();

private:
    Ui::TcpClient *ui;
    QString ip;
    quint16 port;

    //连接服务器
    QTcpSocket tcpSocket;

};
#endif // TCPCLIENT_H
