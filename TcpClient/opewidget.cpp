#include "opewidget.h"

OpeWidget::OpeWidget(QWidget *parent)
{
    // ========== 设置整体样式 ==========
    this->setMinimumSize(960, 520);
    this->setStyleSheet("QWidget { background-color: #f0f2f5; }");

    // 侧边栏
    listWidget = new QListWidget(this);
    listWidget->setFixedWidth(150);
    listWidget->setStyleSheet(
        "QListWidget {"
        "   background-color: #ffffff;"
        "   border: none;"
        "   border-right: 1px solid #e4e7ed;"
        "   font-size: 14px;"
        "   padding: 8px 0px;"
        "}"
        "QListWidget::item {"
        "   padding: 14px 20px;"
        "   color: #606266;"
        "   border-left: 3px solid transparent;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #ecf5ff;"
        "   color: #409eff;"
        "   border-left: 3px solid #409eff;"
        "   font-weight: bold;"
        "}"
        "QListWidget::item:hover:!selected {"
        "   background-color: #f5f7fa;"
        "   color: #409eff;"
        "}"
    );
    listWidget->addItem("👥  好友");
    listWidget->addItem("📁  图书");

    pFriend = new Friend(this);
    pBook = new Book(this);

    //创建一个堆叠容器，可以管理多个页面
    pSW = new QStackedWidget;
    pSW->setStyleSheet(
        "QStackedWidget {"
        "   background-color: #ffffff;"
        "   border-radius: 8px;"
        "   margin: 8px;"
        "}"
    );
    //设置显示的窗口
    pSW->addWidget(pFriend);
    pSW->addWidget(pBook);

    //创建一个水平布局
    QHBoxLayout* pMain= new QHBoxLayout;
    pMain->setSpacing(0);
    pMain->setContentsMargins(0, 0, 0, 0);
    pMain->addWidget(listWidget);       // 左边：列表控件
    pMain->addWidget(pSW, 1);           // 右边：堆叠容器

    // 将布局应用到当前窗口
    setLayout(pMain);

    //添加信号,设置窗口
    // 当 listWidget 当前选中的行改变时，自动切换 pSW显示的页面索引
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
    // 不delete pSW，可能是其他对象的父对象
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
