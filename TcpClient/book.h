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

    //上传
    QTimer *file_timer;
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
    void handleDownloadRespond(PDU* pdu);
    void handleDownloadComplete();
    void handleDownloadError(PDU* pdu);
    void sendDownloadResponse(const char* status);
    void handleDownloadData(PDU* pdu);
    void handleDownloadRawData(const QByteArray& data);

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

    //上传
    enum FileUploadState{

        uploadIdle,
        Uploading
    };
    FileUploadState upload_state;
    QFile upload_file;             // 上传文件
    qint64 upload_sent;
    qint64 upload_total;
    bool m_cancelUpload;            // 取消上传标志
    void cancelUpload();

    //下载
    QFile download_file;           // 下载文件
    qint64 download_total;         // 文件总大小
    qint64 download_received;      // 已接收大小
    void cancelDownload();
    bool m_cancelDownload; // 取消下载标志
    QString getUniqueName(const QString& file_path);    //重复命名

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
