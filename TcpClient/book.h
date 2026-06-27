#ifndef BOOK_H
#define BOOK_H

#include <QWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "protocol.h"
#include <QTimer>
#include <QFile>
#include "threadpool.h"
#include "progressdialog.h"
#include <QPointer>
#include "uploadworker.h"
#include "downloadworker.h"


class Book : public QWidget
{
    Q_OBJECT

public:
    explicit Book(QWidget *parent = nullptr);
    ~Book();

    void updateFileList(const PDU* pdu);

    void handleUploadRespond(PDU* pdu);


    // 下载相关
    enum FileDownloadState{
        Idle,
        Preparing,
        Receiving,
        Completed
    };
    FileDownloadState download_state;
    void handleDownloadRespond(PDU* pdu);           // 解析服务器响应，启动 Worker
    void handleDownloadProcess(PDU* pdu);           // 处理服务器发出的文件
    DownloadWorker* getDownloadWorker() const { return m_downloadWorker; }


    //分享
    void handleShareResponse(PDU* pdu);
    void handleShareReceive();
    void handleShareComplete();
    QString shareFileName;

    void setThreadPool(ThreadPool* pool);
    void onServerUploadFinish();    // 服务端确认上传完成，重置上传状态
signals:

public slots:
    void createDir();
    void flushFile();
    void deleteDirFile();
    void renameDirFile();
    void backDir();
    void uploadFile();
    void downloadFile();
    void shareFile();
    void enterDir(QTreeWidgetItem* item, int column = 0);


private slots:

    //上传
    void onUploadProgress(int percent);
    void onUploadFinished(bool success, const QString& message);
    void onUploadError(const QString& error);

    //下载
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(bool success, const QString &message);
    void onDownloadError(const QString &error);

private:
    QTreeWidget* bookList;
    QPushButton *backPB, *createPB, *renamePB, *flushPB,
        *uploadPB, *downloadPB, *removeDirFilePB, *sharePB;

    // 上传/下载独立状态
    struct UploadTransferInfo {
        QString localFilePath;
        qint64 totalSize = 0;
        int progressItemId = -1;
        QString fileName;
    };
    UploadTransferInfo m_uploadInfo;

    struct DownloadTransferInfo {
        QString saveFilePath;
        qint64 totalSize = 0;
        int progressItemId = -1;
        QString fileName;
    };
    DownloadTransferInfo m_downloadInfo;

    //上传
    enum FileUploadState{

        uploadIdle,
        Uploading
    };
    FileUploadState upload_state;
    qint64 upload_sent;
    qint64 upload_total;
    int m_currentUploadId;               // 当前上传的序号，匹配 Worker 发出的信号
    void cancelUpload();
    QThread* m_uploadThread;
    UploadWorker* m_uploadWorker;

    //下载
    QThread* m_downloadThread;
    DownloadWorker* m_downloadWorker;
    qint64 m_downloadTotal;      // 文件总大小
    void cancelDownload();
    QString getUniqueName(const QString& file_path);        //重复命名

    ThreadPool* m_threadPool;

    //进度条（多项目并发支持）
    ProgressDialog* m_progressDialog = nullptr;
    void ensureProgressDialog();

    //中断
    void sendCancelUploadRequest();   //通知服务器取消上传
    void sendCancelDownloadRequest();   //通知取消下载


};

#endif // BOOK_H
