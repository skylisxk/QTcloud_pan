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


class Book : public QWidget
{
    Q_OBJECT

public:
    explicit Book(QWidget *parent = nullptr);
    ~Book();

    void updateFileList(const PDU* pdu);

    QTimer *file_timer;
    qint64 file_total_size;
    qint64 file_recve_size;

    // 下载相关
    enum FileDownloadState{
        Idle,
        Preparing,
        Receiving,
        Completed
    };
    void handleDownloadRespond(PDU* pdu);
    void handleDownloadComplete();
    void handleDownloadError(PDU* pdu);
    void sendDownloadResponse(const char* status);
    void handleDownloadData(PDU* pdu);

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
    void uploadFileData();
    void downloadFile();
    void shareFile();
    void enterDir(const QModelIndex &index);


private:
    QListWidget* bookList;
    QPushButton *backPB, *createPB, *renamePB, *flushPB,
        *uploadPB, *downloadPB, *removeDirFilePB, *sharePB;

    QString file_save_path;         //保存上传下载的路径

    QFile upload_file;             // 上传文件
    qint64 upload_sent;
    qint64 upload_total;

    QFile download_file;           // 下载文件
    qint64 download_total;         // 文件总大小
    qint64 download_received;      // 已接收大小
    FileDownloadState download_state;

    ThreadPool* m_threadPool;

    //进度条
    QPointer<ProgressDialog> m_progressDialog;
    bool m_cancelUpload;   // 取消上传标志
    bool m_cancelDownload; // 取消下载标志
    void showProgress(const QString& title, const QString& file_name);
    void updateProgress(qint64 current, qint64 total);
    void hideProgress();
    void cancelUpload();
    void cancelDownload();
    void sendCancelUploadRequest();   //通知服务器取消上传
    void sendCancelDownloadRequest();   //通知取消下载
};

#endif // BOOK_H
