#include "opewidget.h"

OpeWidget::OpeWidget(QWidget *parent)
{
    listWidget = new QListWidget(this);
    listWidget->addItem("好友");
    listWidget->addItem("图书");

    pFriend = new Friend(this);
    pBook = new Book(this);

    //设置显示的窗口
    pSW = new QStackedWidget;
    pSW->addWidget(pFriend);
    pSW->addWidget(pBook);

    QHBoxLayout* pMain= new QHBoxLayout;
    pMain->addWidget(listWidget);
    pMain->addWidget(pSW);

    setLayout(pMain);

    //添加信号,设置窗口
    connect(listWidget, SIGNAL(currentRowChanged(int)), pSW, SLOT(setCurrentIndex(int)));

    // 创建统一定时器
    refreshTimer = new QTimer(this);
    //refreshTimer->start(5000);  // 5秒刷新一次
    connect(refreshTimer, &QTimer::timeout, this, &OpeWidget::onTimerTimeout);

    //切换页的时候刷新
    connect(listWidget, &QListWidget::currentRowChanged,
            this, &OpeWidget::onTabChanged);
}

OpeWidget::~OpeWidget()
{

    qDebug() << "OpeWidget 析构";
    // 断开所有信号，避免在析构中触发
    disconnect();
    // 手动删除子对象，确保它们在 QApplication 还存在时销毁
    delete pFriend;
    delete pBook;
    delete listWidget;
    delete refreshTimer;
    // 注意：不要 delete pSW，因为它可能是其他对象的父对象？确保安全
    pFriend = nullptr;
    pBook = nullptr;

}

OpeWidget &OpeWidget::getInstance()
{
    static OpeWidget instance;
    return instance;
}

Friend *OpeWidget::getFriend()
{
    return pFriend;
}

Book *OpeWidget::getBook()
{
    return pBook;
}

QStackedWidget *OpeWidget::getSW()
{
    return pSW;
}

void OpeWidget::onTimerTimeout()
{
    qDebug() << "定时刷新触发，当前显示的是:" << listWidget->currentItem()->text();

    // 根据当前显示的窗口决定刷新哪个
    int currentIndex = pSW->currentIndex();
    if(currentIndex == 0) {  // 好友窗口
        pFriend->flushFriend();  // 需要在Friend类中实现
    } else if(currentIndex == 1) {  // 图书窗口
        pBook->flushFile();  // 调用Book的刷新函数
    }
}

void OpeWidget::onTabChanged(int index)
{

    if(index == 0) {  // 好友窗口
        if(pFriend) {
            pFriend->flushFriend();  // 刷新好友列表
        }
    } else if(index == 1) {  // 图书窗口
        if(pBook) {
            pBook->flushFile();  // 刷新文件列表
        }
    }
}
