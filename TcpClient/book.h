#ifndef BOOK_H
#define BOOK_H

#include <QWidget>
#include <QListWidget>
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

    //上传
    qint64 file_total_size;
    qint64 file_recve_size;
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
    void enterDir(const QModelIndex &index);


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
    QListWidget* bookList;
    QPushButton *backPB, *createPB, *renamePB, *flushPB,
        *uploadPB, *downloadPB, *removeDirFilePB, *sharePB;

    QString file_save_path;         //保存上传下载的路径

    //上传
    enum FileUploadState{

        uploadIdle,
        Uploading
    };
    FileUploadState upload_state;
    QFile upload_file;             // 上传文件
    qint64 upload_sent;
    qint64 upload_total;
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

    //进度条
    ProgressDialog* m_progressDialog;
    void showProgress(const QString& title, const QString& file_name);
    void updateProgress(qint64 current, qint64 total);
    void hideProgress();

    //中断
    void sendCancelUploadRequest();   //通知服务器取消上传
    void sendCancelDownloadRequest();   //通知取消下载


};

#endif // BOOK_H
