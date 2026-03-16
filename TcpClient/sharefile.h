#ifndef SHAREFILE_H
#define SHAREFILE_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QScrollArea>
#include <QListWidget>

class ShareFile : public QWidget
{
    Q_OBJECT
public:
    explicit ShareFile(QWidget *parent = nullptr);
    static ShareFile &getInstance();
    void updateFriendList(QListWidget* listWidget);

signals:

public slots:

    void selectCancel();
    void selectAll();
    void selectConfirm();
    void selectInverse();

private:

    QPushButton *selectAllPB, *selectCancelPB, *selectConfirmPB,  *selectInversePB;

    //好友展示区域
    QScrollArea *friendArea;
    QWidget *friendWidget;
    QVBoxLayout* friendVBL;

    //管理好友
    QButtonGroup *friendButtonGroup;
};

#endif // SHAREFILE_H
