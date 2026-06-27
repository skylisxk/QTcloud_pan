#include "uploadworker.h"

UploadWorker::UploadWorker(QObject *parent)
    : QObject(parent), m_fileSize(0), m_sent(0), m_cancel(false), m_uploadId(0)
{
    // 使用成员定时器替代 QTimer::singleShot，便于在 setFile 时停止旧定时器
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &UploadWorker::sendNextBlock);
}

void UploadWorker::setFile(const QString &filePath)
{
    qDebug() << "UploadWorker::setFile in thread:";

    // 停止上一个上传可能残留的定时器，防止旧定时器触发 sendNextBlock
    m_timer->stop();

    // 递增上传序号，使旧上传的延迟信号被 Book 忽略
    m_uploadId++;

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法打开文件: " + m_file.errorString(), m_uploadId);
        return;
    }

    m_fileSize = m_file.size();
    m_sent = 0;
    m_cancel = false;
}

void UploadWorker::startUpload()
{
    qDebug() << "UploadWorker::startUpload in thread, id:" << m_uploadId;

    if (!m_file.isOpen()) {
        emit errorOccurred("文件未打开，请先调用 setFile", m_uploadId);
        return;
    }

    // 拷贝当前 uploadId，防止本地上传过程中 m_uploadId 被 setFile 修改
    // 开始发送第一块
    sendNextBlock();
}

void UploadWorker::cancelUpload()
{
    m_cancel = true;
}

void UploadWorker::sendNextBlock()
{
    // 保存当前上传的 id，整个函数期间不变
    int currentId = m_uploadId;

    if (m_cancel) {
        m_file.close();
        m_timer->stop();
        emit uploadFinished(false, "上传已取消", currentId);
        return;
    }
    if (m_file.atEnd()) {
        m_file.close();
        m_timer->stop();
        emit uploadFinished(true, "上传完成", currentId);
        return;
    }

    // 一次读64kb文件，返回读取的字节数
    QByteArray buffer = m_file.read(CHUNK_SIZE);
    if (buffer.isEmpty()) {
        m_timer->stop();
        emit errorOccurred("读取文件失败", currentId);
        return;
    }

    // 计算百分比
    m_sent += buffer.size();
    int percent = static_cast<int>(m_sent * 100 / m_fileSize);
    emit progressUpdated(percent, currentId);

    emit dataBlockReady(buffer, currentId);            // 将数据传给主线程

    // 继续下一块，使用成员定时器（setFile 会 stop 它，防止旧上传干扰新上传）
    m_timer->start(1);
}
