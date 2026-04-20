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

class Book : public QWidget
{
    Q_OBJECT
public:
    explicit Book(QWidget *parent = nullptr);
    void updateFileList(const PDU* pdu);
    QTimer *file_timer;
    qint64 file_total_size;
    qint64 file_recve_size;
    enum FileDownloadState{
        Idle,
        Preparing,
        Receiving,
        Completed
    };
    // 下载相关
    void handleDownloadRespond(PDU* pdu);
    void handleDownloadComplete();
    void handleDownloadError(PDU* pdu);
    void sendDownloadResponse(const char* status);
    void handleDownloadData(PDU* pdu);

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


    QFile download_file;           // 本地保存的文件
    qint64 download_total;         // 文件总大小
    qint64 download_received;      // 已接收大小
    FileDownloadState download_state;

    ThreadPool* m_threadPool;

};

#endif // BOOK_H
