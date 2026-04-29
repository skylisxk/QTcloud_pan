#include "uploadworker.h"
#include <QTimer>

UploadWorker::UploadWorker(QObject *parent)
    : QObject(parent), m_fileSize(0), m_sent(0), m_cancel(false)
{
}

void UploadWorker::setFile(const QString &filePath)
{
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法打开文件: " + m_file.errorString());
        return;
    }
    m_fileSize = m_file.size();
    m_sent = 0;
}

void UploadWorker::startUpload()
{
    if (!m_file.isOpen()) {
        emit errorOccurred("文件未打开，请先调用 setFile");
        return;
    }
    m_cancel = false;
    // 开始发送第一块
    sendNextBlock();
}

void UploadWorker::cancelUpload()
{
    m_cancel = true;
}

void UploadWorker::sendNextBlock()
{
    if (m_cancel) {
        m_file.close();
        emit uploadFinished(false, "上传已取消");
        return;
    }

    if (m_file.atEnd()) {
        m_file.close();
        emit uploadFinished(true, "上传完成");
        return;
    }

    QByteArray buffer = m_file.read(CHUNK_SIZE);
    if (buffer.isEmpty()) {
        emit errorOccurred("读取文件失败");
        return;
    }

    m_sent += buffer.size();
    int percent = static_cast<int>(m_sent * 100 / m_fileSize);
    emit progressUpdated(percent);
    emit dataBlockReady(buffer);   // 将数据传给主线程

    // 继续下一块，短暂延时让出 CPU 并允许事件循环处理
    QTimer::singleShot(1, this, &UploadWorker::sendNextBlock);
}
