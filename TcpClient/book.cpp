#include "book.h"
#include "tcpclient.h"
#include <qinputdialog.h>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include "opewidget.h"
#include "sharefile.h"

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
    download_state = Idle;
    download_total = 0;
    download_received = 0;

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

void Book::updateFileList(const PDU *pdu)
{
    if(!pdu){
        return;
    }

    FileInfo* file_info = NULL;

    //清楚原有的数据
    bookList->clear();
    //将客户端发送的pdu传到结构体上
    for(int i = 0; i < pdu->uiMsglen/sizeof(FileInfo); i++){

        file_info = (FileInfo*)pdu->caMsg + i;

        //产生item对象
        QListWidgetItem* item = new QListWidgetItem;
        //设置图标
        item->setIcon(QIcon(QPixmap(0 == file_info->fileType ? QString(":/icon/dir.jpg") : QString(":/icon/file.jpg"))));
        item->setText(file_info->fileName);
        //item输入到bookList上面
        bookList->addItem(item);

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
    QFile file(file_save_path);
    qint64 file_size = file.size();

    PDU* pdu = makePDU(cur_path.toUtf8().size()+1);
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_REQUEST;
    //发送当前路径
    qstrncpy((char*)pdu->caMsg, cur_path.toUtf8().constData(), pdu->uiMsglen);
    //发送大小和文件名
    QString dataStr = QString("%1|%2").arg(file_name).arg(file_size);
    qstrncpy(pdu->caData, dataStr.toUtf8().constData(), 64);

    TcpClient::getInstance().getTcpSocket().write((char*)pdu, pdu->uiPDUlen);

    delete pdu;

    //设置定时器，避免内容黏连
    file_timer->start(1000);
}

void Book::uploadFileData()
{
    file_timer->stop();
    //再把文件打开上传内容
    QFile file(file_save_path);

    if(!file.open(QIODevice::ReadOnly)){

        QMessageBox::warning(this, "上传文件", "打开失败");
        return;
    }

    qint64 fileSize = file.size();
    qint64 totalSent = 0;
    char buffer[4096];  // 使用栈数组，避免手动内存管理

    qDebug() << "开始上传文件，大小:" << fileSize << "字节";

    while(!file.atEnd()) {
        // 读取数据块
        qint64 bytesRead = file.read(buffer, sizeof(buffer));

        if(bytesRead > 0) {
            // 发送数据块
            qint64 bytesSent = TcpClient::getInstance().getTcpSocket().write(buffer, bytesRead);

            if(bytesSent != bytesRead) {
                qDebug() << "发送不完整:" << bytesSent << "/" << bytesRead;
                QMessageBox::warning(this, "上传文件", "网络发送失败");
                break;
            }

            totalSent += bytesSent;

            // 等待数据发送完成，避免TCP缓冲区溢出
            TcpClient::getInstance().getTcpSocket().flush();

            // 可选：显示进度
            int progress = (totalSent * 100) / fileSize;
            qDebug() << "上传进度:" << progress << "%";
        } else if(bytesRead == 0) {
            // 正常结束
            break;
        } else {
            qDebug() << "读文件失败，错误:" << file.errorString();
            QMessageBox::warning(this, "上传文件", "读文件失败: " + file.errorString());
            break;
        }
    }

    file.close();

    qDebug() << "文件上传完成，总发送:" << totalSent << "字节";

}

void Book::downloadFile()
{
    //发送路径和要下载的文件名
    QListWidgetItem* item = bookList->currentItem();
    if(!item){

        return;
    }

    QString file_name = item->text();

    // ✅ 添加 DontUseNativeDialog 选项
    QFileDialog::Options options;
    options |= QFileDialog::DontUseNativeDialog;  // 强制使用Qt对话框，不使用系统原生对话框

    QString save_path = QFileDialog::getSaveFileName(
        this,
        "保存文件",
        file_name,  // 默认文件名
        "所有文件 (*.*)",
        nullptr,
        options    // 添加选项
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

void Book::handleDownloadComplete()
{
    if(download_state == Receiving){

        download_file.close();
        download_state = Completed;
        QMessageBox::information(this, "下载", "文件下载成功！");

        // 重置状态
        download_state = Idle;
        download_received = 0;
        download_total = 0;
    }
}

void Book::handleDownloadError(PDU* pdu)
{
    QString error = QString::fromUtf8(pdu->caData);
    if(download_file.isOpen()) {
        download_file.close();
    }

    download_state = Idle;
    download_received = 0;
    download_total = 0;
    QMessageBox::warning(this, "下载错误", error);

}

void Book::handleDownloadData(PDU *pdu)
{
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

}

void Book::handleShareResponse(PDU *pdu)
{
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
