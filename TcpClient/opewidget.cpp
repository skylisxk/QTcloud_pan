#include "opewidget.h"
#include "tcpclient.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QApplication>

OpeWidget::OpeWidget(QWidget *parent)
{
    // ========== 设置整体样式 ==========
    this->setMinimumSize(960, 520);
    this->setStyleSheet("QWidget { background-color: #f0f2f5; }");
    // 关闭窗口时自动销毁，配合 QApplication::quit() 确保进程完全退出
    setAttribute(Qt::WA_DeleteOnClose);

    // ========== 侧边栏整体容器 ==========
    QWidget* sidebarWidget = new QWidget(this);
    sidebarWidget->setFixedWidth(150);
    sidebarWidget->setStyleSheet("background-color: #ffffff; border-right: 1px solid #e4e7ed;");

    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setSpacing(0);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);

    // 用户信息头部区域
    QString loginUserName = TcpClient::getInstance().loginName;
    QWidget* userHeader = new QWidget;
    userHeader->setStyleSheet("background-color: #f5f7fa; border-bottom: 1px solid #e4e7ed;");
    userHeader->setFixedHeight(64);

    QHBoxLayout* headerLayout = new QHBoxLayout(userHeader);
    headerLayout->setSpacing(8);
    headerLayout->setContentsMargins(12, 0, 12, 0);

    // 头像占位圆圈
    QLabel* avatarLabel = new QLabel;
    avatarLabel->setFixedSize(36, 36);
    avatarLabel->setStyleSheet(
        "background-color: #409eff;"
        "border-radius: 18px;"
        "color: #ffffff;"
        "font-size: 15px;"
        "font-weight: bold;"
    );
    avatarLabel->setAlignment(Qt::AlignCenter);
    // 取用户名的第一个字符作为头像文字
    QString avatarText = loginUserName.isEmpty() ? QString("U") : QString(loginUserName.at(0).toUpper());
    avatarLabel->setText(avatarText);

    // 用户名标签
    QLabel* userNameLabel = new QLabel(loginUserName.isEmpty() ? "未登录" : loginUserName);
    userNameLabel->setStyleSheet(
        "font-size: 13px;"
        "font-weight: bold;"
        "color: #303133;"
        "background: transparent;"
    );
    userNameLabel->setWordWrap(false);

    headerLayout->addWidget(avatarLabel);
    headerLayout->addWidget(userNameLabel, 1);

    // 列表
    listWidget = new QListWidget(this);
    listWidget->setStyleSheet(
        "QListWidget {"
        "   background-color: #ffffff;"
        "   border: none;"
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

    sidebarLayout->addWidget(userHeader);
    sidebarLayout->addWidget(listWidget, 1);

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
    pMain->addWidget(sidebarWidget);        // 左边：侧边栏（含用户信息+菜单）
    pMain->addWidget(pSW, 1);               // 右边：堆叠容器

    // 将布局应用到当前窗口
    setLayout(pMain);

    // 设置窗口标题包含当前用户名
    setWindowTitle(QString("云盘 - %1").arg(loginUserName));

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
    // 手动删除子对象（包含线程清理），确保在 ThreadPool 销毁前完成
    delete pFriend;
    pFriend = nullptr;
    delete pBook;
    pBook = nullptr;
    delete listWidget;
    delete refreshTimer;
}

void OpeWidget::closeEvent(QCloseEvent *event)
{
    // 发送下线请求，确保服务器及时更新 online 状态
    QString name = TcpClient::getInstance().loginName;
    QTcpSocket& sock = TcpClient::getInstance().getTcpSocket();
    if (!name.isEmpty() && sock.state() == QAbstractSocket::ConnectedState) {
        TcpClient::getInstance().m_shuttingDown = true;
        PDU* pdu = makePDU();
        pdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_OUT_REQUEST;
        qstrncpy(pdu->caData, name.toUtf8().constData(), 64);
        sock.write((char*)pdu, pdu->uiPDUlen);
        sock.flush();  // 确保数据立即发出
        free(pdu);
    }
    event->accept();
    // 强制退出事件循环，确保进程完全终止
    QApplication::quit();
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
