#ifndef FRIEND_H
#define FRIEND_H

#include <QWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "online.h"
#include "protocol.h"

class Friend : public QWidget
{
    Q_OBJECT
public:

    QString searchName;

    explicit Friend(QWidget *parent = nullptr);
    void showAllOnlineUsr(PDU* pdu);
    void updateFriendList(PDU* pdu);
    void updateGroupMsg(PDU* pdu);
    QListWidget* getListWidget();

signals:

public slots:

    void showOnline();
    void searchUsr();
    void flushFriend();
    void deleteFriend();
    void chatFriend();
    void groupChat();

private:

    QTextEdit* textEdit;
    QLineEdit* lineEdit;
    QListWidget* listWidget;            //好友列表

    QPushButton* delFriendButton, *flushFriendButton, *showOnlineUsrButton, *searchUsrButton,
               * msgSendButton, *privateChatButton;

    Online* pOnline;                                            //需要一个Online的对象来操作，显示输出框

};

#endif // FRIEND_H
