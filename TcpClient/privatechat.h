#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include <QWidget>
#include "protocol.h"

namespace Ui {
class PrivateChat;
}

class PrivateChat : public QWidget
{
    Q_OBJECT

public:
    explicit PrivateChat(QWidget *parent = nullptr);
    ~PrivateChat();
    void setChatName(QString des_name);

    static PrivateChat& getInstance();              //使用单例模式
    void updateMsg(const PDU* pdu);                 //将pdu的内容显示在窗口上

private slots:
    void on_sendMsg_clicked();


private:
    Ui::PrivateChat *ui;
    QString login_name;
    QString des_name;

};

#endif // PRIVATECHAT_H
