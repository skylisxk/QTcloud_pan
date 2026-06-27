#include "downloadworker.h"
#include <QFileInfo>

DownloadWorker::DownloadWorker(QObject *parent)
    : QObject(parent), m_totalBytes(0), m_receivedBytes(0), m_startPos(0), m_cancel(false)
{
}

void DownloadWorker::setFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_file.setFileName(filePath);
}

void DownloadWorker::setTotal(qint64 totalBytes)
{
    QMutexLocker locker(&m_mutex);
    m_totalBytes = totalBytes;
}

// ★断点续传：设置已下载字节数。必须在 setFile() 之后、start() 之前调用
void DownloadWorker::setStartPos(qint64 pos)
{
    QMutexLocker locker(&m_mutex);
    m_startPos = pos;
}

void DownloadWorker::start()
{
    QMutexLocker locker(&m_mutex);
    m_receivedBytes = 0;
    m_cancel = false;

    qDebug() << "DownloadWorker::start, file name:" << m_file.fileName()
             << "startPos:" << m_startPos;

    // ★断点续传：如果 startPos>0，以追加模式打开文件（保留已下载部分）
    if (m_startPos > 0 && QFile::exists(m_file.fileName())) {
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QString err = QString("无法以追加模式打开文件: %1, 错误: %2")
                              .arg(m_file.fileName()).arg(m_file.errorString());
            emit errorOccured(err);
            return;
        }
        qDebug() << "断点续传：从偏移" << m_startPos << "继续下载";
    } else {
        // 全新下载：覆盖写入
        if (!m_file.open(QIODevice::WriteOnly)) {
            QString err = QString("无法打开文件: %1, 错误: %2").arg(m_file.fileName()).arg(m_file.errorString());
            emit errorOccured(err);
            return;
        }
    }
}

void DownloadWorker::cancel()
{
    QMutexLocker locker(&m_mutex);

    if (m_cancel) return;

    m_cancel = true;

    if (m_file.isOpen()) {

        QString filePath = m_file.fileName();
        m_file.close();
        if (!filePath.isEmpty() && QFile::exists(filePath)) {

            QFile::remove(filePath);
            qDebug() << "Removed incomplete file:" << filePath;
        }
    }
    emit finished(false, tr("下载已取消"));
}

void DownloadWorker::writeData(const QByteArray &data)
{
    // 分块传输，多次调用

    QMutexLocker locker(&m_mutex);

    if (m_cancel) {

        qDebug() << "Download cancelled, ignoring data";
        return;
    }
    if (!m_file.isOpen()) {

        emit errorOccured("文件未打开，无法写入");
        return;
    }

    // 写入文件，并返回写入字节的大小
    qint64 written = m_file.write(data);

    // 不相同说明写入失败
    if (written != data.size()) {

        emit errorOccured("写入文件失败");
        return;
    }

    m_receivedBytes += written;

    // ★断点续传：进度 = 断点前已下载 + 本次已接收
    qint64 totalReceived = m_startPos + m_receivedBytes;
    emit progressUpdated(totalReceived, m_totalBytes);

    if (totalReceived >= m_totalBytes) {

        m_file.close();
        emit finished(true, "下载完成");
    }
}
