#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QTcpSocket>
#include <QByteArray>
#include <QDir>
#include <QFileInfoList>
#include "protocol.h"
#include <QFile>
#include "threadpool.h"
#include <QDateTime>

class MyTcpSocket : public QTcpSocket
{
    Q_OBJECT

public:
    explicit MyTcpSocket(QObject *parent = nullptr);
    ~MyTcpSocket();


    QString getName();

    enum FileUploadState {
        Idle,           // 空闲，没有文件上传
        Preparing,      // 准备接收文件
        Receiving,      // 正在接收文件数据
        Completed       // 接收完成
    };
    enum FileDownloadState {

        d_idle,
        d_receiving
    };

    void setThreadPool(ThreadPool* pool);
    //判断是否需要关闭
    bool m_isClosing;

public slots:
    void receiveMsg();
    void clientOffline();                                           //处理下线功能
    void onReadyRead();                                             //读文件

    //下载
    void sendNextChunk();
    void finishDownload();
    void handleDownloadError(const QString& error);


private:



    QString loginName;
    QString current_file_name;

    void addHelper(PDU* &pdu, const char* str, const int type);                //减少代码冗余

    void flushFileHelper(QDir &dir, QFileInfoList &file_list, PDU* &res_pdu, const int type);

    ThreadPool* m_threadPool;

    //上传
    QFile q_file;
    bool m_cancelUpload;
    //文件接收量，需要接收的总大小
    qint64 file_recve, file_recve_total;
    //判断文件状态
    FileUploadState upload_state;
    void handleUploadRequest(PDU* pdu);
    void handleUploadData();
    bool tryParsePDU();
    void handleUploadComplete();
    void handleUploadError();
    void sendUploadResponse(const char* status);
    void handleUploadCancelRequest(PDU* pdu);


    //下载
    QFile* download_file;
    bool m_cancelDownload;
    FileDownloadState download_state;
    qint64 download_sent{0};                            //已发送大小
    qint64 download_total{0};                           //文件总大小
    void handleDownloadRequest(PDU* pdu);
    QTimer* m_downloadTimer;                            //添加下载定时器
    void handleCancelDownloadRequest(PDU* pdu);         //取消下载


    //分享
    void handleShareFile(PDU* pdu);
    void handleShareRespond(PDU* pdu);
    void handleShareConfirm(PDU* pdu);
    void handleShareDirCopy(QString dir_src, QString dir_des);

signals:

    void offline(MyTcpSocket* mysocket);                            //下线信号

    void uploadComplete();                                            // 新增：上传完成信号
    void uploadError(const QString& error);                         // 新增：上传错误信号


};

#endif // MYTCPSOCKET_H
