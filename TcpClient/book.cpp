#include "book.h"
#include "tcpclient.h"
#include <qinputdialog.h>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include "opewidget.h"
#include "sharefile.h"
#include <QCoreApplication>

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
    file_timer = new QTimer(this);
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
    connect(file_timer, &QTimer::timeout, this, &Book::uploadFileData);

}

Book::~Book()
{

    qDebug() << "Book 析构开始";

    if(file_timer && file_timer->isActive()) {
        file_timer->stop();
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
    //上传路径和文件
    QString cur_path = TcpClient::getInstance().curPath;
    //要上传文件的路径,获取弹窗选中的路径
    file_save_path = QFileDialog::getOpenFileName();

    if(file_save_path.isEmpty()){

        return;
    }

    //将路径的文件名提取出来
    int lastSlash = file_save_path.lastIndexOf('/');
    QString file_name = file_save_path.mid(lastSlash+1);

    //上传文件的大小
    upload_file.setFileName(file_save_path);

    if(!upload_file.open(QIODevice::ReadOnly)){

        QMessageBox::warning(this, "上传文件", "打开失败");
        return;
    }

    upload_total = upload_file.size();
    upload_sent = 0;
    upload_state = Uploading;

    qDebug() << "upload_state 设置为:" << upload_state;


    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_REQUEST;
    //发送当前路径
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);
    //发送大小和文件名
    QString dataStr = QString("%1|%2").arg(file_name).arg(upload_total);
    qstrncpy(pdu->caData, dataStr.toUtf8().constData(), 64);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);
    delete pdu;


    //显示进度条
    showProgress("正在上传", file_name);

    //设置定时器，避免内容黏连
    //file_timer->start(1000);
}

void Book::uploadFileData()
{
    //上传过程中会反复调用这个函数

    if(m_cancelUpload){

        qDebug() << "上传已被取消，终止";
        return;
    }

    if(upload_state != Uploading){

        qDebug() << "上传已被取消，终止";
        return;
    }



    file_timer->stop();

    char buffer[64 * 1024];  // 使用栈数组，避免手动内存管理
    const int BATCH_SIZE = 5;  // 一次定时器发送5块
    int sentCount = 0;
    bool hasError = false;


    qDebug() << "开始上传文件，大小:" << upload_total << "字节";

    while(!upload_file.atEnd() && sentCount < BATCH_SIZE) {
        // 读取数据块
        qint64 bytesRead = upload_file.read(buffer, sizeof(buffer));

        if(bytesRead > 0) {
            // 发送数据块
            qint64 bytesSent = TcpClient::getInstance().getTcpSocket().write(buffer, bytesRead);

            if(bytesSent != bytesRead) {
                qDebug() << "发送不完整:" << bytesSent << "/" << bytesRead;
                QMessageBox::warning(this, "上传文件", "网络发送失败");
                hasError = true;
                break;
            }

            upload_sent += bytesSent;
            sentCount++;

            // 等待数据发送完成，避免TCP缓冲区溢出
            TcpClient::getInstance().getTcpSocket().flush();

            //更新进度
            updateProgress(upload_sent, upload_total);
            int progress = (upload_sent * 100) / upload_total;
            qDebug() << "上传进度:" << progress << "%";

        }

        else if(bytesRead == 0) {

            // 正常结束

            break;
        }

        else {

            hasError = true;
            qDebug() << "读文件失败，错误:" << upload_file.errorString();
            QMessageBox::warning(this, "上传文件", "读文件失败: " + upload_file.errorString());
            break;
        }
    }

    //上传完成，或者发生错误
    if(upload_file.atEnd() || hasError) {

        upload_state = uploadIdle;
        upload_file.close();

        // ✅ 上传完成后，停止定时器并清理
        if(file_timer && file_timer->isActive()) {
            file_timer->stop();
        }

        if(!hasError) {
            updateProgress(upload_sent, upload_total);

            // ✅ 延迟隐藏进度条，避免在事件循环中删除
            QTimer::singleShot(500, this, [this]() {
                hideProgress();
            });
            qDebug() << "上传完成";
        }
    }

    else {
        // 继续上传
        file_timer->start(50);
    }

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

void Book::cancelUpload()
{
    m_cancelUpload = true;

    //停止定时器
    if(file_timer && file_timer->isActive()){

        file_timer->stop();
    }

    //关闭文件
    if(upload_file.isOpen()){

        upload_file.close();
    }

    //通知服务器取消上传
    sendCancelUploadRequest();

    //重置状态
    upload_state = uploadIdle;

    QTimer::singleShot(1000, this, [this]() {

        hideProgress();

        //重置状态
        m_cancelUpload = false;
        QMessageBox::information(this, "上传", "上传已取消");
        qDebug() << "上传已取消完成";
    });

    qDebug() << "上传已取消";

}

void Book::handleUploadRespond(PDU *pdu)
{
    QString response = QString::fromUtf8(pdu->caData);
    if (response.startsWith(FILE_UPLOAD_PROCESS) || response.startsWith(FILE_UPLOAD_RENAME)) {
        // 服务器已准备好，开始发送数据
        file_timer->start(1000);
    } else if (response.startsWith(FILE_UPLOAD_FAIL)) {
        QMessageBox::warning(this, "上传", "服务器准备失败");
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

