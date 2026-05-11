#ifndef DOWNLOADWORKER_H
#define DOWNLOADWORKER_H

#include <QObject>
#include <QFile>
#include <QDebug>
#include <QMutex>

class DownloadWorker : public QObject
{
    Q_OBJECT

public:

    explicit DownloadWorker(QObject *parent = nullptr);

public slots:

    void setFile(const QString& filePath);
    void setTotal(qint64 totalBytes);
    void start();
    void cancel();
    void writeData(const QByteArray& data);     //主线程调用

signals:

    void progressUpdated(qint64 received, qint64 total);
    void finished(bool success, const QString& message);
    void errorOccured(const QString& error);

private:

    QFile m_file;
    qint64 m_totalBytes;        // 总大小（由外部设置）
    qint64 m_receivedBytes;     // 已接收字节数
    bool m_cancel;
    mutable QMutex m_mutex;
};

#endif // DOWNLOADWORKER_H
