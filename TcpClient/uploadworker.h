#ifndef UPLOADWORKER_H
#define UPLOADWORKER_H

#include <QObject>
#include <QFile>
#include <QDebug>

class UploadWorker : public QObject
{
    Q_OBJECT
public:
    explicit UploadWorker(QObject *parent = nullptr);

    void setFile(const QString &filePath);   // 设置要上传的本地文件路径
    void startUpload();                      // 开始上传
    void cancelUpload();                    // 取消上传

signals:
    void dataBlockReady(const QByteArray &data);   // 数据块已读取，请主线程发送
    void progressUpdated(int percent);             // 进度百分比 (0-100)
    void uploadFinished(bool success, const QString &message);
    void errorOccurred(const QString &error);

private slots:
    void sendNextBlock();                   // 递归或定时发送下一块

private:
    QFile m_file;
    qint64 m_fileSize;
    qint64 m_sent;
    bool m_cancel;
    static const int CHUNK_SIZE = 64 * 1024;   // 64KB
};

#endif // UPLOADWORKER_H
