#ifndef OPEWIDGET_H
#define OPEWIDGET_H

#include <QWidget>
#include <QListWidget>
#include "friend.h"
#include "book.h"
#include <QStackedWidget>
#include <QTimer>

class OpeWidget : public QWidget
{
    Q_OBJECT
public:

    explicit OpeWidget(QWidget *parent = nullptr);
    ~OpeWidget();

    static OpeWidget& getInstance();                        //单例模式
    Friend* getFriend();
    Book* getBook();
    QStackedWidget* getSW();

signals:

public slots:
    void onTimerTimeout();
    void onTabChanged(int index);

private:

    QListWidget* listWidget;
    Friend* pFriend;
    Book* pBook;

    // 页面容器
    QStackedWidget* pSW;

    QTimer* refreshTimer;

};

#endif // OPEWIDGET_H
