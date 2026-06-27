#ifndef UPLOADWORKER_H
#define UPLOADWORKER_H

#include <QObject>
#include <QFile>
#include <QDebug>
#include <QTimer>

class UploadWorker : public QObject
{
    Q_OBJECT

public:

    explicit UploadWorker(QObject *parent = nullptr);

public slots:
    void setFile(const QString &filePath);   // 设置要上传的本地文件路径
    void startUpload();                      // 开始上传
    void cancelUpload();                    // 取消上传

signals:
    // uploadId 用于匹配上传请求，防止上一次上传的延迟信号干扰当前上传
    void dataBlockReady(const QByteArray &data, int uploadId);
    void progressUpdated(int percent, int uploadId);
    void uploadFinished(bool success, const QString &message, int uploadId);
    void errorOccurred(const QString &error, int uploadId);

private slots:
    void sendNextBlock();                   // 定时发送下一块

private:
    QFile m_file;
    qint64 m_fileSize;
    qint64 m_sent;
    bool m_cancel;
    int m_uploadId;                        // 每次 setFile 递增，防止旧定时器发送错误信号
    QTimer* m_timer;                       // 成员定时器替代 QTimer::singleShot，支持 stop()
    static const int CHUNK_SIZE = 64 * 1024;   // 64KB
};

#endif // UPLOADWORKER_H
