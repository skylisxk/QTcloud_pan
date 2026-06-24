#include "progressdialog.h"
#include <QHBoxLayout>


ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent), m_isFinished(false)
{

    setWindowTitle("传输进度");
    setModal(true);
    setFixedSize(420, 200);
    setStyleSheet(
        "QDialog {"
        "   background-color: #ffffff;"
        "   border-radius: 8px;"
        "}"
    );

    // 创建控件
    m_titleLabel = new QLabel("正在传输...");
    m_titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #303133;");

    m_fileLabel = new QLabel("文件名: ");
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setStyleSheet("color: #909399; font-size: 12px;");

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(22);

    m_percentLabel = new QLabel("0%");
    m_percentLabel->setAlignment(Qt::AlignCenter);
    m_percentLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #409eff;");

    m_detailLabel = new QLabel("已传输: 0 / 0 字节");
    m_detailLabel->setStyleSheet("color: #909399; font-size: 12px;");

    m_cancelBtn = new QPushButton("取消传输");
    m_cancelBtn->setFixedWidth(100);

    // 布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(24, 20, 24, 16);
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_fileLabel);

    QHBoxLayout* progressLayout = new QHBoxLayout;
    progressLayout->addWidget(m_progressBar, 1);
    progressLayout->addWidget(m_percentLabel);
    mainLayout->addLayout(progressLayout);

    mainLayout->addWidget(m_detailLabel);
    mainLayout->addWidget(m_cancelBtn, 0, Qt::AlignRight);

    //关联到槽
    connect(m_cancelBtn, &QPushButton::clicked, this, &ProgressDialog::onCancelClicked);
}

static QString formatSize(qint64 size){

    if(size < 1024) return QString("%1 B").arg(size);
    if(size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    if(size < 1024 * 1024 * 1024) return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void ProgressDialog::setTitle(const QString &title)
{
    m_titleLabel->setText(title);

}

void ProgressDialog::setFileName(const QString &file_name)
{
    m_fileLabel->setText(QString("文件名: %1").arg(file_name));

}

void ProgressDialog::setProgress(qint64 current, qint64 total)
{
    if (m_isFinished) return;

    int percent = (current * 100) / total;
    m_progressBar->setValue(percent);
    m_percentLabel->setText(QString("%1%").arg(percent));

    // 特殊处理：当 total == 100 时，认为 current 是百分数，不显示字节
    if (total == 100) {
        m_detailLabel->setText(QString("已传输: %1%").arg(current));
    } else {
        m_detailLabel->setText(QString("已传输: %1 / %2")
                                   .arg(formatSize(current))
                                   .arg(formatSize(total)));
    }
}

void ProgressDialog::setFinished()
{
    m_isFinished = true;
    m_titleLabel->setText("传输完成！");
    m_titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #67c23a;");
    m_progressBar->setValue(100);
    m_percentLabel->setText("100%");
    m_percentLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #67c23a;");
    m_cancelBtn->setText("关闭");

}

void ProgressDialog::setInterrupt()
{
    m_isFinished = true;
    m_cancelBtn->setText("关闭");

}

void ProgressDialog::onCancelClicked()
{
    if(m_isFinished) {
        accept();  // 完成时点击关闭
    }
    else {
        emit cancelled();  // 传输中点击取消
    }

}

