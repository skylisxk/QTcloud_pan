#include "sharefile.h"
#include <QCheckBox>
#include "tcpclient.h"
#include "opewidget.h"

ShareFile::ShareFile(QWidget *parent)
    : QWidget{parent}
{

    selectAllPB = new QPushButton("全选");
    selectCancelPB = new QPushButton("取消");
    selectInversePB = new QPushButton("取消全选");
    selectConfirmPB = new QPushButton("确定");

    friendWidget = new QWidget;
    friendArea = new QScrollArea;
    friendVBL = new QVBoxLayout(friendWidget);
    friendButtonGroup = new QButtonGroup(friendWidget);
    //多选
    friendButtonGroup->setExclusive(false);

    QHBoxLayout *top_hbl = new QHBoxLayout;
    top_hbl->addWidget(selectAllPB);
    top_hbl->addWidget(selectInversePB);

    //下拉槽
    top_hbl->addStretch();

    QHBoxLayout *down_hbl = new QHBoxLayout;
    down_hbl->addWidget(selectConfirmPB);
    down_hbl->addWidget(selectCancelPB);

    QVBoxLayout *main_vbl = new QVBoxLayout;
    main_vbl->addLayout(top_hbl);
    main_vbl->addWidget(friendArea);
    main_vbl->addLayout(down_hbl);
    setLayout(main_vbl);

    connect(selectInversePB, &QAbstractButton::clicked, this, &ShareFile::selectInverse);
    connect(selectCancelPB, &QAbstractButton::clicked, this, &ShareFile::selectCancel);
    connect(selectConfirmPB, &QAbstractButton::clicked, this, &ShareFile::selectConfirm);
    connect(selectAllPB, &QAbstractButton::clicked, this, &ShareFile::selectAll);


}

ShareFile &ShareFile::getInstance()
{
    static ShareFile instance;
    return instance;
}

void ShareFile::updateFriendList(QListWidget *listWidget)
{
    if(!listWidget){

        return;
    }

    //先前的好友列表，需要清除
    QList<QAbstractButton*>pre_friend_list = friendButtonGroup->buttons();

    for(auto &ele : pre_friend_list){

        friendVBL->removeWidget(ele);
        friendButtonGroup->removeButton(ele);
        //删除列表
        pre_friend_list.removeOne(ele);
        delete ele;
    }

    //获取新的列表
    QCheckBox* box = nullptr;
    for(int i = 0; i < listWidget->count(); i++){

        box = new QCheckBox(listWidget->item(i)->text());
        friendVBL->addWidget(box);
        friendButtonGroup->addButton(box);
    }

    //打印到屏幕上
    friendArea->setWidget(friendWidget);
}

void ShareFile::selectCancel()
{
    hide();
}

void ShareFile::selectAll()
{
    //获得box
    QList<QAbstractButton*> box_list = friendButtonGroup->buttons();

    //如果没有被选中
    for(auto &ele : box_list){

        if(!ele->isChecked()){

            ele->setChecked(true);
        }
    }
}

void ShareFile::selectConfirm()
{
    TcpClient& tcp_instance = TcpClient::getInstance();
    QString sender = tcp_instance.loginName;
    QString share_file_name = OpeWidget::getInstance().getBook()->shareFileName;
    QString cur_path = tcp_instance.curPath + '/' + share_file_name;
    //获取接受者
    QList<QAbstractButton*> box_list = friendButtonGroup->buttons();
    int recipient_num = 0;
    for(auto &ele : box_list){

        if(ele->isChecked()){

            recipient_num++;
        }
    }

    int recipient_bytes = recipient_num * 32;
    int path_bytes = cur_path.toUtf8().size() + 1;  // +1 for '\0'
    int total_msg_len = recipient_bytes + path_bytes;

    //产生pdu,一个名字占32byte
    PDU* pdu = makePDU(total_msg_len);
    pdu->uiMsgType = ENUM_MSG_TYPE_FILE_SHARE_REQUEST;

    //分享者，接收者数量caData,路径+文件名,接受者msg
    sprintf(pdu->caData, "%s|%d", sender.toStdString().c_str(), recipient_num);
    //循环放置接收者
    int offset = 0;
    for(int i = 0; i < box_list.size(); i++){

        if(!box_list[i]->isChecked()){

            continue;
        }

        qstrncpy((char*)pdu->caMsg + offset*32, box_list[i]->text().toUtf8().constData(), 32);
        offset++;
    }

    // 在接收者列表后面放置文件路径
    char* path_pos = (char*)pdu->caMsg + recipient_bytes;
    qstrncpy(path_pos, cur_path.toUtf8().constData(), path_bytes);

    tcp_instance.getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

    hide();
}

void ShareFile::selectInverse()
{
    //获得box
    QList<QAbstractButton*> box_list = friendButtonGroup->buttons();

    //反选
    for(auto &ele : box_list){

        if(ele->isChecked()){

            ele->setChecked(false);
        }
    }
}
