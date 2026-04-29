#include "book.h"
#include "tcpclient.h"
#include <qinputdialog.h>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include "opewidget.h"
#include "sharefile.h"
#include <QCoreApplication>
#include <QThread>

Book::Book(QWidget *parent)
    : QWidget{parent}
{
    bookList = new QListWidget;


    backPB = new QPushButton("返回");
    createPB = new QPushButton("创建文件夹");
    removeDirFilePB = new QPushButton("删除文件");
    sharePB = new QPushButton("分享文件");
    renamePB = new QPushButton("重命名文件夹");
    flushPB = new QPushButton("刷新文件");
    uploadPB = new QPushButton("上传文件");
    downloadPB = new QPushButton("下载文件");

    QVBoxLayout* dirVBL = new QVBoxLayout;
    dirVBL->addWidget(backPB);
    dirVBL->addWidget(createPB);
    dirVBL->addWidget(renamePB);
    dirVBL->addWidget(flushPB);

    QVBoxLayout* fileVBL = new QVBoxLayout;
    fileVBL->addWidget(uploadPB);
    fileVBL->addWidget(downloadPB);
    fileVBL->addWidget(sharePB);
    fileVBL->addWidget(removeDirFilePB);

    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->addWidget(bookList);
    mainLayout->addLayout(dirVBL);
    mainLayout->addLayout(fileVBL);

    setLayout(mainLayout);
    file_recve_size = 0;
    file_total_size = 0;
    m_cancelUpload = false;
    upload_state = uploadIdle;
    m_cancelDownload = false;
    download_state = Idle;
    download_total = 0;
    download_received = 0;

    m_threadPool = nullptr;
    m_progressDialog = nullptr;

    //关联信号槽
    connect(createPB, &QAbstractButton::clicked, this, &Book::createDir);
    connect(flushPB, &QAbstractButton::clicked, this, &Book::flushFile);
    connect(removeDirFilePB, &QAbstractButton::clicked, this, &Book::deleteDirFile);
    connect(renamePB, &QAbstractButton::clicked, this, &Book::renameDirFile);
    connect(backPB, &QAbstractButton::clicked, this, &Book::backDir);
    connect(uploadPB, &QAbstractButton::clicked, this, &Book::uploadFile);
    connect(bookList, &QListWidget::doubleClicked, this, &Book::enterDir);
    connect(downloadPB, &QAbstractButton::clicked, this, &Book::downloadFile);
    connect(sharePB, &QAbstractButton::clicked, this, &Book::shareFile);

    // 初始化上传线程
    m_uploadThread = new QThread(this);
    m_uploadWorker = new UploadWorker();
    m_uploadWorker->moveToThread(m_uploadThread);

    // 连接信号槽
    connect(m_uploadThread, &QThread::finished, m_uploadWorker, &QObject::deleteLater);
    connect(m_uploadWorker, &UploadWorker::progressUpdated, this, &Book::onUploadProgress);
    connect(m_uploadWorker, &UploadWorker::uploadFinished, this, &Book::onUploadFinished);
    connect(m_uploadWorker, &UploadWorker::errorOccurred, this, &Book::onUploadError);

    connect(m_uploadWorker, &UploadWorker::dataBlockReady,
            this, [this](const QByteArray &data) {

                // 如果上传状态不是 Uploading，说明已取消，忽略剩余数据块
                if (upload_state != Uploading) {

                    qDebug() << "线程中断";
                    return;
                }
                // 确保在主线程中执行
                QTcpSocket &socket = TcpClient::getInstance().getTcpSocket();
                qint64 sent = socket.write(data);
                if (sent != data.size()) {
                    qDebug() << "socket write error, sent:" << sent;
                    m_uploadWorker->cancelUpload(); // 可取消上传
                    upload_state = uploadIdle;
                }
            }, Qt::QueuedConnection);

    m_uploadThread->start();
}

Book::~Book()
{

    qDebug() << "Book 析构开始";

    // 1. 停止 Worker 的上传任务
    if (m_uploadWorker) {
        m_uploadWorker->cancelUpload();      // 设置取消标志，使其停止发送新块
    }

    // 2. 退出线程的事件循环并等待线程结束
    if (m_uploadThread) {
        m_uploadThread->quit();              // 通知线程退出事件循环
        m_uploadThread->wait();              // 等待线程真正结束（避免资源未释放）
    }

    // 3. 断开所有信号连接，防止在析构过程中触发槽
    if (m_uploadWorker) {
        disconnect(m_uploadWorker, nullptr, this, nullptr);
        // 也可以调用 deleteLater，但线程已停止，可直接删除
        delete m_uploadWorker;
        m_uploadWorker = nullptr;
    }


    if(upload_file.isOpen()) {
        upload_file.close();
    }
    if(download_file.isOpen()) {
        download_file.close();
    }



    qDebug() << "Book 析构完成";
}

void Book::setThreadPool(ThreadPool *pool)
{
    m_threadPool = pool;
}

void Book::updateFileList(const PDU *pdu)
{
    if(!pdu){
        return;
    }

    //调用线程池刷新
    int file_count = pdu->uiMsglen / sizeof(FileInfo);

    //立即拷贝数据，不依赖 pdu 后续是否存活
    QByteArray rawData((const char*)pdu->caMsg, pdu->uiMsglen);

    // 判断是否使用线程池
    bool use_threadPool = (m_threadPool != nullptr && file_count > 3);

    if(use_threadPool){

        m_threadPool->enqueue([this, rawData, file_count]() {

            FileInfo* file_list = (FileInfo*)rawData.data();

            struct ItemData{
                QString fileName;
                int fileType;
            };

            QList<ItemData> items;
            //开辟内存
            items.reserve(file_count);

            //耗时操作
            for(int i = 0; i < file_count; i++){

                FileInfo* file_info = file_list + i;
                ItemData item_data;
                item_data.fileName = QString::fromUtf8(file_info->fileName);
                item_data.fileType = file_info->fileType;
                items.append(item_data);

            }

            //主线程更新,必须通过 invokeMethod 回到主线程更新UI
            QMetaObject::invokeMethod(this, [this, items](){

                bookList->clear();

                for(const auto& item_data : items){

                    QListWidgetItem* item = new QListWidgetItem;

                    QString iconPath = (item_data.fileType == 0) ? ":/icon/dir.jpg" : ":/icon/file.jpg";
                    item->setIcon(QIcon(iconPath));
                    item->setText(item_data.fileName);
                    // 存储文件类型，方便后续操作（如双击时判断是文件还是文件夹）
                    item->setData(Qt::UserRole, item_data.fileType);

                    bookList->addItem(item);
                }
            }, Qt::QueuedConnection);

        });

    }

    else{
        // 文件较少，直接在主线程处理（原有逻辑）
        bookList->clear();

        for(int i = 0; i < file_count; i++) {
            FileInfo* file_info = (FileInfo*)pdu->caMsg + i;

            QListWidgetItem* item = new QListWidgetItem;
            QString iconPath = (file_info->fileType == 0) ? ":/icon/dir.jpg" : ":/icon/file.jpg";
            item->setIcon(QIcon(iconPath));
            item->setText(QString::fromUtf8(file_info->fileName));
            item->setData(Qt::UserRole, file_info->fileType);
            bookList->addItem(item);
        }

    }


}

void Book::createDir()
{
    //输入新建文件夹名
    QString dir_name = QInputDialog::getText(this, "新建文件夹", "新建文件夹名");

    //为空
    if(dir_name.isEmpty()){
        return;
    }

    //在列表添加新的项
    QListWidgetItem* item = new QListWidgetItem(QIcon(":/icon/dir.jpg"), dir_name);
    bookList->addItem(item);

    //发送用户名，新建文件夹名，目录信息
    QString login_name = TcpClient::getInstance().loginName;
    QString path = TcpClient::getInstance().curPath;
    PDU* pdu = makePDU(path.toUtf8().size()+1);

    //用户名和文件夹名caData, 路径caMsg
    pdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_REQUEST;
    qstrncpy(pdu->caData, login_name.toUtf8().constData(), 32);
    qstrncpy(pdu->caData+32, dir_name.toUtf8().constData(), 32);
    memcpy(pdu->caMsg, path.toStdString().c_str(), path.size());

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

}

void Book::flushFile()
{
    //将路径信息发送出去
    QString cur_path = TcpClient::getInstance().curPath;
    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_REQUEST;
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

void Book::deleteDirFile()
{
    //获取文件夹名和目录名
    QListWidgetItem* item = bookList->currentItem();
    if(!item){

        return;
    }

    QString file_name = item->text();

    // 弹出确认对话框
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
                                  QString("确定要删除 \"%1\" 吗？").arg(file_name),
                                  QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes){

        return;
    }

    QString cur_path = TcpClient::getInstance().curPath;
    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);

    pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_DIR_FILE_REQUEST;
    qstrncpy(pdu->caData, file_name.toUtf8().constData(), 64);
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);


    //直接移除该条目
    delete item;
    delete pdu;
}

void Book::renameDirFile()
{
    //发送目录信息，要修改的名字以及新文件名
    QListWidgetItem* item = bookList->currentItem();
    if(!item){

        return;
    }

    QString new_name = QInputDialog::getText(this, "重命名", "输入新的名字");
    if(new_name.isEmpty()){

        return;
    }

    QString old_name = item->text();
    QString cur_path = TcpClient::getInstance().curPath;
    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_RENAME_DIR_FILE_REQUEST;

    QByteArray copy_name = old_name.toUtf8();
    qstrncpy(pdu->caData, copy_name.constData(), 32);
    copy_name = new_name.toUtf8();
    qstrncpy(pdu->caData+32, copy_name.constData(), 32);

    QByteArray copy_path = cur_path.toUtf8();
    qstrncpy((char*)pdu->caMsg, copy_path.constData(), pdu->uiMsglen);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;
}

void Book::backDir()
{
    //检查是否为根目录
    if(TcpClient::getInstance().curPath == TcpClient::getInstance().rootPath){

        return;
    }

    //发送包含上级目录的信息
    QString cur_path = TcpClient::getInstance().curPath;
    int lastSlash = cur_path.lastIndexOf('/');
    if(lastSlash > 0){
        TcpClient::getInstance().curPath = cur_path.left(lastSlash);
    }
    flushFile();
}

void Book::enterDir(const QModelIndex &index)
{
    //获取目录信息以及要进入的文件夹
    QString cur_path = TcpClient::getInstance().curPath;
    QString dir_name = index.data().toString();

    if(dir_name.isEmpty()){

        return;
    }

    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_ENTER_DIR_REQUEST;

    QByteArray utf_name = dir_name.toUtf8();
    qstrncpy(pdu->caData, utf_name.constData(), 64);
    QByteArray utf_path = cur_path.toUtf8();
    qstrncpy((char*)pdu->caMsg, utf_path.constData(), pdu->uiMsglen);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

}




/**********************************************************************************
 * ********************************************************************************
 * ********************************************************************************
 * ********************************************************************************
 * ********************************************************************************
 * ********************************************************************************
 * ********************************************************************************
 * ********************************************************************************/

void Book::uploadFile()
{

    QString cur_path = TcpClient::getInstance().curPath;
    QString localFilePath = QFileDialog::getOpenFileName(this, "选择要上传的文件");
    if (localFilePath.isEmpty()) return;

    QFileInfo info(localFilePath);
    QString fileName = info.fileName();
    qint64 fileSize = info.size();

    // 保存文件路径和大小，供 Worker 使用
    file_save_path = localFilePath;
    upload_total = fileSize;

    // 发送上传请求
    PDU* pdu = makePDU(cur_path.toUtf8().size() + 1);
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_REQUEST;
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);
    QString dataStr = QString("%1|%2").arg(fileName).arg(fileSize);
    qstrncpy(pdu->caData, dataStr.toUtf8().constData(), 64);
    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    delete pdu;

    upload_state = Uploading;

    // 显示进度条，等待服务器确认
    showProgress("正在上传", fileName);
    // 注意：不要启动任何定时器
}


void Book::handleUploadRespond(PDU* pdu)
{
    QString response = QString::fromUtf8(pdu->caData);
    QStringList parts = response.split('|');
    if (parts.size() < 2) {
        QMessageBox::warning(this, "上传", "服务器响应格式错误");
        hideProgress();
        return;
    }

    QString status = parts[0];
    QString actualFileName = parts[1];

    if (status == FILE_UPLOAD_PROCESS || status == FILE_UPLOAD_RENAME) {
        if (status == FILE_UPLOAD_RENAME) {
            QMessageBox::information(this, "提示", QString("文件已重命名为: %1").arg(actualFileName));
        }

        // 使用新的 Worker 接口：只设置文件路径
        m_uploadWorker->setFile(file_save_path);
        // 不需要 setSocket，因为数据会通过 dataBlockReady 信号传回主线程发送
        m_uploadWorker->startUpload();

        // 可选：设置一个标志表示正在上传，用于进度条取消等
    } else if (status == FILE_UPLOAD_FAIL) {
        QMessageBox::warning(this, "上传", "服务器准备失败");
        hideProgress();
    }
}

void Book::sendCancelUploadRequest()
{

    qDebug() << "=== sendCancelUploadRequest 被调用 ===";

    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_CANCEL_REQUEST;

    qDebug() << "发送消息类型:" << pdu->uiMsgType;
    qDebug() << "PDU长度:" << pdu->uiPDUlen;

    qint64 sent = TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    qDebug() << "实际发送字节数:" << sent;

    TcpClient::getInstance().getTcpSocket().flush();

    delete pdu;

}

void Book::cancelUpload()
{

    qDebug() << "CancelUpload调用";

    if (upload_state != Uploading) return;

    upload_state = uploadIdle;
    m_cancelUpload = true;

    // 1. 通知服务器取消上传
    sendCancelUploadRequest();

    // 2. 通知 Worker 停止读取文件
    if (m_uploadWorker) {
        m_uploadWorker->cancelUpload();
    }

    // // 3. 将进度条设置为中断状态（不立即隐藏，等待 Worker 响应）
    // if (m_progressDialog) {
    //     m_progressDialog->setInterrupt();  // 显示“已中断”
    // }

}

void Book::onUploadProgress(int percent)
{
    if (m_progressDialog) {

        m_progressDialog->setProgress(percent, 100);
    }
}

void Book::onUploadFinished(bool success, const QString &message)
{
    hideProgress();

    upload_state = uploadIdle;
    m_cancelUpload = false;

    if (success) {

        QMessageBox::information(this, "上传", message);
        QTimer::singleShot(500, this, &Book::flushFile); // 延迟刷新
    }
    else {

        QMessageBox::warning(this, "上传", message);
    }


}

void Book::onUploadError(const QString &error)
{
    hideProgress();

    upload_state = uploadIdle;

    m_cancelUpload = false;

    QMessageBox::warning(this, "上传错误", error);

}



/************************************************************************************
 * **********************************************************************************
 * **********************************************************************************
 * **********************************************************************************
 * **********************************************************************************
 * **********************************************************************************
 * **********************************************************************************/
void Book::downloadFile()
{

    //发送路径和要下载的文件名
    QListWidgetItem* item = bookList->currentItem();
    if(!item){

        return;
    }

    QString file_name = item->text();

    //保存路径
    QString save_path = QFileDialog::getSaveFileName(
        this,
        "保存文件",
        file_name,  // 默认文件名
        "所有文件 (*.*)"
        );

    if(save_path.isEmpty()) {
        qDebug() << "用户取消了保存";
        return;
    }

    file_save_path = save_path;
    download_state = Preparing;
    download_total = 0;

    QString cur_path = TcpClient::getInstance().curPath;
    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);

    pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_REQUEST;
    qstrncpy(pdu->caData, file_name.toUtf8().constData(), 64);
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

}

void Book::handleDownloadRespond(PDU *pdu)
{
    // 打开本地文件准备写入
    download_file.setFileName(file_save_path);
    if(!download_file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建文件:" << download_file.errorString();
        download_state = Idle;
        return;
    }

    //获取文件名和大小
    QString file_str = QString::fromUtf8(pdu->caData);
    QStringList parts = file_str.split('|');

    if(parts.size() < 2) {

        qDebug() << "下载信息格式错误";
        download_state = Idle;
        return;
    }

    QString file_name = parts[0];
    download_total = parts[1].toLongLong();

    //显示进度条
    showProgress("正在下载", file_name);

    //打开本地文件并写入
    download_file.setFileName(file_save_path);
    if(!download_file.open(QIODevice::WriteOnly)){

        download_state = Idle;
        return;
    }

    //设置状态
    download_state = Receiving;
    download_received = 0;

}

void Book::handleDownloadData(PDU *pdu)
{
    qDebug() << "=== handleDownloadData 被调用 ===";
    qDebug() << "PDU 数据大小:" << pdu->uiMsglen;
    qDebug() << "download_state:" << download_state;
    //反复调用这个函数

    if(download_state != Receiving) {
        qDebug() << "未在接收状态，忽略数据";
        return;
    }

    //获取数据,这个是data_pdu
    QByteArray data((char*)pdu->caMsg, pdu->uiMsglen);
    //写入文件
    qint64 written = download_file.write(data);

    if(written != data.size()) {

        qDebug() << "写入文件失败";
        handleDownloadError(pdu);
        return;
    }

    download_received += written;

    // 更新进度条
    updateProgress(download_received, download_total);

    qDebug() << "下载进度:" << download_received << "/" << download_total;

    // 检查是否完成
    if(download_received >= download_total) {

        handleDownloadComplete();
    }

}

void Book::handleDownloadRawData(const QByteArray &data)
{
    qDebug() << "=== handleDownloadRawData ===";
    qDebug() << "数据大小:" << data.size();
    qDebug() << "download_state:" << download_state;

    if(download_state != Receiving) {
        qDebug() << "不在接收状态，忽略";
        return;
    }

    qint64 written = download_file.write(data);
    qDebug() << "写入文件:" << written << "/" << data.size();

    if(written == data.size()) {
        download_received += written;
        qDebug() << "下载进度:" << download_received << "/" << download_total;

        if(download_received >= download_total) {
            handleDownloadComplete();
        }
    } else {
        qDebug() << "写入失败";
    }
}


void Book::handleDownloadComplete()
{
    if(download_state == Receiving){

        // ✅ 强制刷新缓冲区，确保数据写入磁盘
        download_file.flush();
        download_file.close();
        download_state = Completed;

        // 重置状态
        download_state = Idle;
        download_received = 0;
        download_total = 0;

        //关闭进度条
        hideProgress();

        QMessageBox::information(this, "下载", "文件下载成功！");
    }
}

void Book::handleDownloadError(PDU* pdu)
{
    QString error = QString::fromUtf8(pdu->caData);
    if(download_file.isOpen()) {
        download_file.close();
    }

    //关闭进度条
    hideProgress();

    download_state = Idle;
    download_received = 0;
    download_total = 0;
    QMessageBox::warning(this, "下载错误", error);

}


QString Book::getUniqueName(const QString &file_path)
{
    qDebug() << "getUniqueName called with:" << file_path;

    QFileInfo file_info(file_path);
    QString base_name = file_info.completeBaseName();   // 前缀名
    QString suffix = file_info.suffix();        // 后缀名
    QString path = file_info.absolutePath();    // 绝对路径

    //后缀添加.
    if(!suffix.isEmpty()){

        suffix = "." + suffix;
    }

    //不存在则退出,就是没有遇到同名的文件
    if(!QFile::exists(file_path)){

        qDebug() << "File does not exist, return original:" << file_path;
        return file_path;
    }

    // 尝试添加 (1), (2), (3)... 直到找到不存在的文件名
    int count = 1;
    QString new_file_path;

    do{

        new_file_path = QString("%1/%2(%3)%4").arg(path).arg(base_name).arg(count).arg(suffix);
        count++;

    }   while(QFile::exists(new_file_path));

    qDebug() << "Returning unique path:" << new_file_path;
    return new_file_path;

}






/****************************************************************************************/
void Book::handleShareReceive()
{
    QMessageBox::information(this, "分享", "文件分享成功！");
}


void Book::handleShareComplete()
{
    flushFile();
}

void Book::shareFile()
{
    //分享者，接收者，文件名，路径
    QListWidgetItem* item = bookList->currentItem();

    if(!item){

        return;
    }
    shareFileName = item->text();
    QString cur_path = TcpClient::getInstance().curPath;
    QString sender = TcpClient::getInstance().loginName;

    //获得好友信息
    Friend* friend_info = OpeWidget::getInstance().getFriend();
    QListWidget* friend_list = friend_info->getListWidget();
    //获得列表信息 将信息添加到sharefile.cpp
    ShareFile &share_file_instance = ShareFile::getInstance();
    share_file_instance.updateFriendList(friend_list);

    if(share_file_instance.isHidden()){

        share_file_instance.show();
    }


}

void Book::handleShareResponse(PDU *pdu)
{
    //服务器发送的inform_pdu
    //获取文件路径
    QString path = QString::fromUtf8((char*)pdu->caMsg);
    //获取文件名
    int lastSlash = path.lastIndexOf('/');
    if(lastSlash > 0){
        shareFileName = path.mid(lastSlash+1);
    }
    //通知消息
    QString note = QString("%1 share file->%2 \n是否接收").arg(pdu->caData).arg(shareFileName);
    //是否接收
    int state = QMessageBox::question(this, "共享文件", note);
    if(QMessageBox::Yes != state){

        //不接收直接退出
        return;
    }
    //回复客户端确认接收
    QString recipient_name = TcpClient::getInstance().loginName;
    PDU* res_pdu = makePDU(pdu->uiMsglen);
    res_pdu->uiMsgType = ENUM_MSG_TYPE_FILE_SHARE_CONFIRM;
    //拷贝路径和名字
    memcpy(res_pdu->caMsg, pdu->caMsg, pdu->uiMsglen);
    qstrncpy(res_pdu->caData, recipient_name.toUtf8().constData(), 64);

    TcpClient::getInstance().getTcpSocket().write((char*)res_pdu, res_pdu->uiPDUlen);

    delete res_pdu;
}






/*************************************************************************************/
void Book::showProgress(const QString &title, const QString &file_name)
{
    // 每次都重新创建，确保信号连接有效
    if(m_progressDialog) {

        m_progressDialog->disconnect();
        m_progressDialog->close();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }

    m_progressDialog = new ProgressDialog(this);
    m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);

    //非模态，能同时进行其他操作
    m_progressDialog->setModal(false);
    m_progressDialog->setWindowModality(Qt::NonModal); // 确保不阻塞


    // 连接取消信号
    connect(m_progressDialog, &ProgressDialog::cancelled, this, [this](){

        qDebug() << "=== cancelled 信号被触发 ===";
        qDebug() << "download_state:" << download_state;
        qDebug() << "upload_state:" << upload_state;

        if(download_state == Receiving) {

            qDebug() << "进入取消下载分支";
            cancelDownload();

        }

        else if(upload_state == Uploading) {

            qDebug() << "进入取消上传分支";
            cancelUpload();
        }

        else {

            qDebug() << "没有匹配的状态！";
        }

    }, Qt::QueuedConnection);

    // 连接销毁信号，清空指针
    connect(m_progressDialog, &QObject::destroyed, this, [this]() {

        m_progressDialog = nullptr;
        qDebug() << "进度条已销毁";
    });

    m_progressDialog->setTitle(title);
    m_progressDialog->setFileName(file_name);
    m_progressDialog->show();
}

void Book::updateProgress(qint64 current, qint64 total)
{
    if(m_progressDialog && m_progressDialog->isVisible()){

        m_progressDialog->setProgress(current, total);
    }
}

void Book::hideProgress()
{
    if(!m_progressDialog) return;

    qDebug() << "hideProgress 被调用";

    if(m_cancelUpload || m_cancelDownload){

        qDebug() << "中断操作";
        m_progressDialog->setInterrupt();
        return;
    }

    // 只是通知完成，让用户手动关闭
    m_progressDialog->setFinished();

}


void Book::cancelDownload(){

    if(m_cancelDownload)    return;

    m_cancelDownload = true;

    if(download_file.isOpen()){

        download_file.close();
    }

    if(!file_save_path.isEmpty() && QFile::exists(file_save_path)){

        QFile::remove(file_save_path);
    }

    sendCancelDownloadRequest();

    download_state = Idle;

    hideProgress();

    m_cancelDownload = false;

}

void Book::sendCancelDownloadRequest()
{
    qDebug() << "=== sendCancelDownloadRequest 被调用 ===";

    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_CANCEL_REQUEST;

    qDebug() << "发送消息类型:" << pdu->uiMsgType;
    qDebug() << "PDU长度:" << pdu->uiPDUlen;

    qint64 sent = TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    qDebug() << "实际发送字节数:" << sent;

    TcpClient::getInstance().getTcpSocket().flush();

    delete pdu;
}

