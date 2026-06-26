#include "progressdialog.h"
#include <QCloseEvent>
#include <QStyle>

// ========== 格式化文件大小 ==========
static QString formatSize(qint64 size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    if (size < 1024 * 1024 * 1024) return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

// ========== TransferItem 实现 ==========
TransferItem::TransferItem(int transferId, TransferDirection dir,
                           const QString& fileName, QWidget* parent)
    : QFrame(parent), m_transferId(transferId)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(
        "TransferItem {"
        "   background-color: #fafafa;"
        "   border: 1px solid #e4e7ed;"
        "   border-radius: 6px;"
        "   padding: 8px;"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(12, 8, 12, 8);

    // ---- 第一行：图标 + 标题 + 百分比 + 取消按钮 ----
    QHBoxLayout* topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    // 图标
    m_iconLabel = new QLabel;
    m_iconLabel->setFixedSize(24, 24);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    if (dir == TransferDirection::Upload) {
        m_iconLabel->setText("📤");
    } else {
        m_iconLabel->setText("📥");
    }

    // 标题
    QString directionText = (dir == TransferDirection::Upload) ? "上传" : "下载";
    m_titleLabel = new QLabel(QString("%1: %2").arg(directionText, fileName));
    m_titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #303133; background: transparent;");

    // 百分比
    m_percentLabel = new QLabel("0%");
    m_percentLabel->setFixedWidth(45);
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_percentLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #409eff; background: transparent;");

    // 取消按钮
    m_cancelBtn = new QPushButton("取消传输");
    m_cancelBtn->setFixedWidth(72);
    m_cancelBtn->setFixedHeight(28);
    m_cancelBtn->setStyleSheet(
        "QPushButton { font-size: 11px; padding: 2px 8px; }"
    );

    topRow->addWidget(m_iconLabel);
    topRow->addWidget(m_titleLabel, 1);
    topRow->addWidget(m_percentLabel);
    topRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(topRow);

    // ---- 第二行：进度条 ----
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);

    // ---- 第三行：详情 ----
    m_detailLabel = new QLabel("已传输: 0 / 0 字节");
    m_detailLabel->setStyleSheet("color: #909399; font-size: 11px; background: transparent;");
    mainLayout->addWidget(m_detailLabel);

    // 连接
    connect(m_cancelBtn, &QPushButton::clicked, this, &TransferItem::onCancelClicked);
}

void TransferItem::setProgress(qint64 current, qint64 total)
{
    if (m_isFinished) return;

    int percent = (total > 0) ? (int)(current * 100 / total) : 0;
    m_progressBar->setValue(percent);
    m_percentLabel->setText(QString("%1%").arg(percent));

    // 上传用 percent/100 模式，下载用字节模式
    if (total == 100) {
        m_detailLabel->setText(QString("已传输: %1%").arg(current));
    } else {
        m_detailLabel->setText(QString("已传输: %1 / %2")
                                   .arg(formatSize(current))
                                   .arg(formatSize(total)));
    }
}

void TransferItem::setFinished()
{
    m_isFinished = true;

    m_titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #67c23a; background: transparent;");
    m_percentLabel->setText("100%");
    m_percentLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #67c23a; background: transparent;");
    m_progressBar->setValue(100);
    m_cancelBtn->setText("关闭");
}

void TransferItem::setInterrupt()
{
    m_isFinished = true;

    m_titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #909399; background: transparent;");
    m_percentLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #909399; background: transparent;");
    m_cancelBtn->setText("关闭");
}

void TransferItem::onCancelClicked()
{
    if (m_isFinished) {
        // 已完成/已中断 → 关闭此条
        emit closeRequested(m_transferId);
    } else {
        // 传输中 → 取消传输
        m_isFinished = true;
        m_titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #f56c6c; background: transparent;");
        m_percentLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #f56c6c; background: transparent;");
        m_cancelBtn->setText("关闭");
        m_detailLabel->setText("已取消");
        emit cancelled(m_transferId);
    }
}

// ========== ProgressDialog 实现 ==========
ProgressDialog::ProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("传输进度");
    setModal(false);
    setMinimumWidth(500);
    setMaximumHeight(500);
    setStyleSheet(
        "QDialog {"
        "   background-color: #ffffff;"
        "   border-radius: 8px;"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(
        "QScrollArea {"
        "   border: none;"
        "   background-color: #ffffff;"
        "}"
    );

    m_container = new QWidget;
    m_container->setStyleSheet("background-color: #ffffff;");
    m_itemLayout = new QVBoxLayout(m_container);
    m_itemLayout->setSpacing(6);
    m_itemLayout->setContentsMargins(12, 12, 12, 12);
    m_itemLayout->addStretch();  // 底部弹簧，让项目靠上排列

    m_scrollArea->setWidget(m_container);
    mainLayout->addWidget(m_scrollArea);
}

int ProgressDialog::addItem(TransferDirection dir, const QString& fileName)
{
    int id = m_nextId++;
    TransferItem* item = new TransferItem(id, dir, fileName, m_container);

    // 连接到 cancelled（转发给 Book）
    connect(item, &TransferItem::cancelled, this, [this](int transferId) {
        m_activeCount--;
        emit cancelled(transferId);
    });

    // 连接到 closeRequested（内部移除）
    connect(item, &TransferItem::closeRequested, this, [this](int transferId) {
        removeItem(transferId);
    });

    // 插入到弹簧之前
    m_itemLayout->insertWidget(m_itemLayout->count() - 1, item);
    m_items.insert(id, item);
    m_activeCount++;

    // 调整对话框高度
    int itemCount = m_items.size();
    int newHeight = qMin(120 + itemCount * 90, 500);
    resize(width(), newHeight);

    if (!isVisible()) {
        show();
    }

    return id;
}

void ProgressDialog::removeItem(int transferId)
{
    TransferItem* it = m_items.value(transferId, nullptr);
    if (!it) return;

    m_items.remove(transferId);
    m_itemLayout->removeWidget(it);
    it->hide();
    it->deleteLater();

    // 调整对话框高度
    int itemCount = m_items.size();
    int newHeight = qMin(120 + itemCount * 90, 500);
    resize(width(), newHeight);

    // 没有活跃项了
    if (itemCount == 0) {
        m_activeCount = 0;
        emit allTransfersFinished();
    }
}

TransferItem* ProgressDialog::item(int transferId) const
{
    return m_items.value(transferId, nullptr);
}

void ProgressDialog::closeEvent(QCloseEvent* event)
{
    // 如果有未完成的传输，不允许直接关闭窗口
    // 用户必须通过取消按钮逐个取消，或者等待传输完成
    if (m_activeCount > 0) {
        event->ignore();
        return;
    }
    event->accept();
}
