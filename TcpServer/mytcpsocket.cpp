#include "mytcpsocket.h"
#include "mytcpserver.h"
#include <QDebug>
#include <QMessageBox>
#include "operatedb.h"
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <QThread>
#include <QApplication>

MyTcpSocket::MyTcpSocket(QObject *parent)
    : QTcpSocket{parent}
{
    upload_state = Idle;
    m_isClosing = false;
    m_cancelUpload = false;
    m_cancelDownload = false;
    download_state = d_idle;
    file_recve = 0;
    file_recve_total = 0;
    download_file = NULL;
    download_total = 0;
    download_sent = 0;
    m_threadPool = nullptr;
    download_file = nullptr;

    //connect(this, SIGNAL(readyRead()), this, SLOT(receiveMsg()));
    connect(this, &QTcpSocket::disconnected, this, &MyTcpSocket::clientOffline);
    connect(this, &QTcpSocket::readyRead, this, &MyTcpSocket::onReadyRead);


    //上传信号槽
    connect(this, &MyTcpSocket::uploadComplete, this, &MyTcpSocket::handleUploadComplete, Qt::QueuedConnection);
    connect(this, &MyTcpSocket::uploadError, this, &MyTcpSocket::handleUploadError, Qt::QueuedConnection);

    // 创建下载定时器
    m_downloadTimer = new QTimer(this);
    m_downloadTimer->setSingleShot(false);  // 不单次触发
    connect(m_downloadTimer, &QTimer::timeout, this, &MyTcpSocket::sendNextChunk);
}

MyTcpSocket::~MyTcpSocket()
{
    qDebug() << "***** MyTcpSocket 析构 *****";

    // 如果还没有下线，强制下线
    if(!loginName.isEmpty()) {

        OperateDB::getInstance().handleOffline(loginName.toStdString().c_str());
        emit offline(this);
    }

    // 停止定时器
    if(m_downloadTimer) {

        m_downloadTimer->stop();
    }

    // 关闭文件
    if(download_file) {

        if(download_file->isOpen()) {

            download_file->close();
        }

        delete download_file;
        download_file = nullptr;
    }

    if(q_file.isOpen()) {

        q_file.close();
    }
}


QString MyTcpSocket::getName()
{
    return loginName;
}

void MyTcpSocket::setThreadPool(ThreadPool *pool)
{
    m_threadPool = pool;
}

void MyTcpSocket::onReadyRead()
{
    if (m_isClosing) return;

    // 外层循环：因为清理残留数据后可能暴露出合法PDU，需要循环处理
    // 最多迭代3次避免死循环（PDU→文件数据→清理→PDU 各一次）
    for (int loop = 0; loop < 3 && bytesAvailable() > 0; loop++) {
        bool didWork = false;

        // 第一步：解析协议消息（包括取消请求）
        const int MAX_PDU = 10;
        int processed = 0;

        while (bytesAvailable() >= sizeof(unsigned int) && processed < MAX_PDU) {
            unsigned int testLen = 0;
            peek((char*)&testLen, sizeof(unsigned int));

            if (testLen >= sizeof(PDU) && testLen <= 10 * 1024 * 1024) {
                if (!tryParsePDU()) break;
                processed++;
                didWork = true;
            } else {
                break;
            }
        }

        // 第二步：处理文件数据上传
        // ★ 关键：只读取预期剩余字节数，绝不多读！
        //    因为缓冲区中可能已经包含下一个PDU（如新UPLOAD_REQUEST），
        //    如果 readAll() 会把它当文件数据吃掉，导致客户端永远等待。
        if (upload_state == Receiving && bytesAvailable() > 0) {
            const qint64 MAX_CHUNK = 64 * 1024;
            qint64 remaining = file_recve_total - file_recve;
            qint64 toRead = qMin(bytesAvailable(), qMin(MAX_CHUNK, remaining));

            if (toRead > 0) {
                QByteArray data = read(toRead);
                if (!data.isEmpty()) {
                    q_file.write(data);
                    file_recve += data.size();
                    didWork = true;

                    if (file_recve >= file_recve_total) {
                        handleUploadComplete();
                    }
                }
            }
        }

        // 第三步：清理非上传状态下缓冲区中的残留文件数据
        // ★ 只丢弃前端不构成有效PDU的字节，不能 readAll()！
        //    因为残留文件数据后面可能紧跟着合法PDU（例如：中断后重传时，
        //    旧上传的尾部数据和新UPLOAD_REQUEST PDU可能在同一个TCP包中）
        else if (upload_state != Receiving && bytesAvailable() >= sizeof(unsigned int)) {
            qint64 discarded = 0;
            while (bytesAvailable() >= sizeof(unsigned int)) {
                unsigned int testLen = 0;
                peek((char*)&testLen, sizeof(unsigned int));
                if (testLen >= sizeof(PDU) && testLen <= 10 * 1024 * 1024) {
                    // 找到有效PDU头，停止丢弃，外层循环会回来解析它
                    break;
                }
                char c;
                read(&c, 1);
                discarded++;
                didWork = true;
            }
            if (discarded > 0) {
                qDebug() << "丢弃非PDU残留数据:" << discarded << "bytes";
            }
            // 如果剩余数据不够sizeof(unsigned int)且非空，全部丢弃
            if (bytesAvailable() < sizeof(unsigned int) && bytesAvailable() > 0) {
                qint64 remaining = bytesAvailable();
                readAll();
                qDebug() << "丢弃尾部残留数据:" << remaining << "bytes";
                didWork = true;
            }
        }

        if (!didWork) break;  // 没有进展，避免死循环
    }
}


bool MyTcpSocket::tryParsePDU()
{
    if(m_isClosing) {
        return false;
    }

    if(bytesAvailable() < sizeof(unsigned int)) {
        return false;
    }

    unsigned int uiPDUlen = 0;
    peek((char*)&uiPDUlen, sizeof(unsigned int));

    if(uiPDUlen < sizeof(PDU) || uiPDUlen > 10 * 1024 * 1024) {
        qDebug() << "非法PDU长度:" << uiPDUlen;
        readAll();
        return false;
    }

    if(bytesAvailable() < uiPDUlen) {
        return false;
    }

    // 调用原有的 receiveMsg()
    receiveMsg();

    return true;
}

void MyTcpSocket::receiveMsg()
{

    // 读取PDU长度
    unsigned int uiPDUlen = 0;
    qint64 readLen = read((char*)&uiPDUlen, sizeof(unsigned int));

    if(readLen != sizeof(unsigned int)) {
        qDebug() << "读取PDU长度失败";
        return;
    }

    if(uiPDUlen < sizeof(PDU) || uiPDUlen > 10 * 1024 * 1024) {
        qDebug() << "非法PDU长度:" << uiPDUlen;
        readAll();
        return;
    }

    unsigned int uiMsgLen = uiPDUlen - sizeof(PDU);
    PDU* pdu = makePDU(uiMsgLen);
    if(!pdu) {
        qDebug() << "内存分配失败";
        return;
    }

    readLen = read((char*)pdu + sizeof(unsigned int), uiPDUlen - sizeof(unsigned int));
    if(readLen != uiPDUlen - sizeof(unsigned int)) {
        qDebug() << "读取PDU数据不完整";
        free(pdu);
        return;
    }

    qDebug() << "收到消息类型:" << pdu->uiMsgType;

    handlePDU(pdu);
    free(pdu);
    pdu = nullptr;

}

void MyTcpSocket::handlePDU(PDU* pdu){

    switch(pdu->uiMsgType){

    case ENUM_MSG_TYPE_REGIST_REQUEST:{                                     //如果是注册请求

        QString caName = QString::fromUtf8(pdu->caData);
        QString caPwd  = QString::fromUtf8(pdu->caData+32);
        PDU* resPdu = makePDU();                                    //响应

        //跟数据库的对比，如果输入的账户密码处理失败
        if(!OperateDB::getInstance().handleRegist(caName.toUtf8().constData(), caPwd.toUtf8().constData())){

            qstrcpy(resPdu->caData, REGIST_FAIL);
            break;
        }

        //用户名作为文件夹创建
        QDir dir;
        dir.mkpath(QString("./%1").arg(caName));

        resPdu->uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;

        qstrcpy(resPdu->caData, REGIST_DONE);

        write((char*)resPdu, resPdu->uiPDUlen);                     //将成功的消息结果返回给客户端

        free(resPdu);
        resPdu = nullptr;

        break;

    }

    case ENUM_MSG_TYPE_LOGIN_REQUEST:{                                      //登录请求

        QString caName = QString::fromUtf8(pdu->caData);
        QString caPwd  = QString::fromUtf8(pdu->caData+32);
        PDU* resPdu = makePDU();                                    //响应

        //如果输入的账户密码处理失败

        if(!OperateDB::getInstance().handleLogin(caName.toUtf8().constData(), caPwd.toUtf8().constData())){

            qstrcpy(resPdu->caData, LOGIN_FAIL);
            QMessageBox::information(nullptr, "Login", LOGIN_FAIL);
            break;
        }

        resPdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;

        qstrcpy(resPdu->caData, LOGIN_DONE);

        write((char*)resPdu, resPdu->uiPDUlen);                     //将结果返回给客户端

        loginName = caName;                                           //将name拷贝

        free(resPdu);
        resPdu = nullptr;

        break;

    }

    case ENUM_MSG_TYPE_LOGIN_OUT_REQUEST:{                                  //注销请求

        QString name = QString::fromUtf8(pdu->caData);

        //如果删除失败
        if(!OperateDB::getInstance().handleLoginOut(name.toStdString().c_str())){

            QMessageBox::information(nullptr, "Login Out", "login out fail");
            break;
        }

        PDU* res_pdu = makePDU();
        addHelper(res_pdu, "login out done", ENUM_MSG_TYPE_LOGIN_OUT_RESPOND);
        delete res_pdu;

        break;
    }

    case ENUM_MSG_TYPE_ALL_ONLINE_REQUEST:{                                 //查询在线用户

        QStringList usrList = OperateDB::getInstance().handleAllOnline();   //将查询到的用户放入usrList

        unsigned int uiMsgLen = usrList.size() * 32;                        //一个名字占32字节
        PDU* resPdu = makePDU(uiMsgLen);                                    //将usrlist里面的名字放入到pdu的camsg里面
        resPdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_RESPOND;

        for(int i = 0; i < usrList.size(); i++){

            //拷贝内存地址到casmsg，每拷贝一次偏移32个字节
            qstrncpy((char*)(resPdu->caMsg) + i * 32, usrList.at(i).toUtf8().constData(), 32);
        }

        write((char*)resPdu, resPdu->uiPDUlen);                             //发送到客户端

        free(resPdu);
        resPdu = nullptr;

        break;
    }

    case ENUM_MSG_TYPE_SEARCH_USR_REQUEST:{                                 //按用户名查询用户

        int state = OperateDB::getInstance().handleSearchUsr(pdu->caData);
        PDU* resPdu = makePDU();
        resPdu->uiMsgType = ENUM_MSG_TYPE_SEARCH_USR_RESPOND;

        if(-1 == state){

            qstrcpy(resPdu->caData, SEARCH_USR_NO);
            delete resPdu;
            break;
        }else if(state == 1){

            qstrcpy(resPdu->caData, SEARCH_USR_ONLINE);
        }else{

            qstrcpy(resPdu->caData, SEARCH_USR_OFFLINE);
        }

        qDebug() << resPdu->caData;

        write((char*)resPdu, resPdu->uiPDUlen);
        free(resPdu);
        resPdu = nullptr;

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:{                                 //添加好友

        QString login_name = QString::fromUtf8(pdu->caData+32);
        QString des_name   = QString::fromUtf8(pdu->caData);

        int state = OperateDB::getInstance().handleAddFriend(des_name.toUtf8().constData(), login_name.toUtf8().constData());
        PDU* res_pdu = makePDU();

        if(state == -1){
            //如果发生错误
            addHelper(res_pdu, UNKNOWN, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);

        }else if(state == 0){
            //0表示已经是好友
            addHelper(res_pdu, FRIEND_EXIST, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);

        }else if(state == 1){
            //1表示在线
            MyTcpServer::getInstance().transcation(des_name.toUtf8().constData(), pdu);


        }else if(state == 2){
            //2表示不在线
            addHelper(res_pdu, SEARCH_USR_OFFLINE, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);

        }else{
            //3表示找不到des_name
            addHelper(res_pdu, SEARCH_USR_NO, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);
        }

        delete res_pdu;

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE:{                                   //同意请求

        QString login_name = QString::fromUtf8(pdu->caData+32);
        QString des_name   = QString::fromUtf8(pdu->caData);

        OperateDB::getInstance().addFriendDB(des_name.toUtf8().constData(), login_name.toUtf8().constData());

        QMessageBox::information(nullptr, "添加好友", "添加成功");

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:{                                  //拒绝请求

        QMessageBox::information(nullptr, "添加好友", "添加失败");
        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:{                               //刷新好友列表
        //获取当前用户名称
        QString login_name = QString::fromUtf8(pdu->caData);
        //获取好友列表，调用数据库
        QStringList list = OperateDB::getInstance().handleFlushFriend(login_name.toUtf8().constData());

        if(list.isEmpty()) {
            // 没有好友，发送空列表
            PDU* res_pdu = makePDU();
            res_pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
            write((char*)res_pdu, res_pdu->uiPDUlen);
            delete res_pdu;
            break;
        }

        unsigned int uiMsgLen = list.size() * 32;
        //pdu发送这个列表
        PDU* res_pdu = makePDU(uiMsgLen);
        res_pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;

        for(int i = 0; i < list.size(); i++){

            //把名字拷贝进去 确保填满32字节，不足的用\0补齐
            QByteArray nameData = list.at(i).toUtf8();
            int copyLen = qMin(nameData.size(), 31);        // 最多31字节，留1字节给\0
            memcpy((char*)(res_pdu->caMsg) + i * 32, nameData.constData(), copyLen);
        }

        write((char*)res_pdu, res_pdu->uiPDUlen);

        delete res_pdu;

        break;
    }

    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:{                              //删除好友请求

        QString login_name = QString::fromUtf8(pdu->caData);
        QString des_name   = QString::fromUtf8(pdu->caData+32);

        OperateDB::getInstance().deleteFriend(des_name.toUtf8().constData(), login_name.toUtf8().constData());

        PDU* res_pdu = makePDU();
        res_pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;

        qstrcpy(res_pdu->caData, DELETE_FINISH);

        write((char*)res_pdu, res_pdu->uiPDUlen);

        delete res_pdu;

        //给删除方的回复
        MyTcpServer::getInstance().transcation(des_name.toUtf8().constData(), pdu);

        break;
    }

    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:{                               //私聊请求

        QString des_name = QString::fromUtf8(pdu->caData+32);

        MyTcpServer::getInstance().transcation(des_name.toUtf8().constData(), pdu);

        break;
    }

    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:{                                 //群聊请求

        QStringList onlineList = OperateDB::getInstance().handleAllOnline();

        for(int i = 0; i < onlineList.size(); i++){
            //转发功能
            MyTcpServer::getInstance().transcation(onlineList.at(i).toStdString().c_str(), pdu);
        }

        break;
    }

    case ENUM_MSG_TYPE_CREATE_DIR_REQUEST:{                                 //创建文件夹请求

        QDir dir;
        QString path = QString((char*)pdu->caData);
        PDU* res_pdu = makePDU();

        if(!dir.exists(path)){

            addHelper(res_pdu, DIR_NOT_EXIST, ENUM_MSG_TYPE_CREATE_DIR_RESPOND);
            break;
        }

        QString dir_name = QString::fromUtf8(pdu->caData+32);
        //在当前目录下创建文件夹
        QString newPath = path + "/" + dir_name;

        //如果存在同名文件夹
        if(dir.exists(newPath)){

            addHelper(res_pdu, FILE_EXIST, ENUM_MSG_TYPE_CREATE_DIR_RESPOND);
            break;
        }

        dir.mkdir(newPath);
        addHelper(res_pdu, FILE_CREATE_DONE, ENUM_MSG_TYPE_CREATE_DIR_RESPOND);

        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:{                                 //刷新文件请求

        QDir dir(QString::fromUtf8((char*)pdu->caMsg));
        //获取目录信息, flielist是一个列表，每一项代表一个文件,并且忽略.和..文件夹
        QFileInfoList file_list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        PDU* res_pdu = NULL;
        flushFileHelper(dir, file_list, res_pdu, ENUM_MSG_TYPE_FLUSH_FILE_RESPOND);
        //发送res_pdu,之前的file_info相当于对respdu内存区域进行了操作
        write((char*)res_pdu, res_pdu->uiPDUlen);
        delete res_pdu;
        break;
    }

    case ENUM_MSG_TYPE_DELETE_DIR_FILE_REQUEST:{                            //删除文件夹请求

        //使用QString接收
        QString dir_name = QString::fromUtf8(pdu->caData);
        QString path = QString::fromUtf8((char*)pdu->caMsg) + "/" + dir_name;

        /**********************多线程**************************/
        if(m_threadPool){

            m_threadPool->enqueue([this, dir_name, path](){

                QFileInfo file_info(path);

                bool is_delete = false;
                QDir dir;

                if(file_info.isDir()){

                    dir.setPath(path);
                    // 耗时操作在线程池执行
                    is_delete = dir.removeRecursively();
                }
                else if(file_info.isFile()) {
                    dir.setPath(QFileInfo(path).path());
                    is_delete = dir.remove(file_info.fileName());
                }

                // 回到主线程发送响应
                QMetaObject::invokeMethod(this, [this, is_delete](){

                    PDU* res_pdu = makePDU();
                    addHelper(res_pdu, is_delete ? DIR_FILE_DELETE_DONE : DIR_FILE_DELETE_FAIL, ENUM_MSG_TYPE_DELETE_DIR_FILE_RESPOND);

                }, Qt::QueuedConnection);

            });

        }

        else{

            //使用QFileInfo
            QFileInfo file_info(path);

            //判断是否删除成功
            bool is_delete = false;
            QDir dir;

            if(file_info.isDir()){

                dir.setPath(path);
                //删除文件夹里面所有的文件
                is_delete = dir.removeRecursively();
            }
            else if(file_info.isFile()){

                //删除单个文件
                dir.setPath(QString::fromUtf8((char*)pdu->caMsg));
                is_delete = dir.remove(dir_name);
            }

            PDU* res_pdu = makePDU();
            addHelper(res_pdu, is_delete ? DIR_FILE_DELETE_DONE : DIR_FILE_DELETE_FAIL, ENUM_MSG_TYPE_DELETE_DIR_FILE_RESPOND);

        }
        break;
    }

    case ENUM_MSG_TYPE_RENAME_DIR_FILE_REQUEST:{                            //重命名文件夹请求

        QString path = QString::fromUtf8((char*)pdu->caMsg);
        QString old_name = QString::fromUtf8(pdu->caData);
        QString new_name = QString::fromUtf8(pdu->caData + 32);


        // 使用 QDir 拼接路径,而不是直接拼接/
        QDir dir(path);
        QString dir_old_path = dir.filePath(old_name);
        QString dir_new_path = dir.filePath(new_name);
        // 转换为绝对路径,保留原变量名
        // 将拼接后的相对路径转换为绝对路径，覆盖原变量值
        QFileInfo old_file_info(dir_old_path);
        dir_old_path = old_file_info.absoluteFilePath();    // 覆盖为绝对路径

        //如果是文件
        if (old_file_info.isFile()) {
            // 文件保留原扩展名
            QString suffix = old_file_info.suffix();
            QFileInfo newFileInfo(new_name);

            if (!suffix.isEmpty() && newFileInfo.suffix().isEmpty()) {
                // 自动加上扩展名
                QString final_new = new_name + "." + suffix;
                dir_new_path = dir.filePath(final_new);

            }
        }

        QFileInfo new_file_info(dir_new_path);
        dir_new_path = new_file_info.absoluteFilePath(); // 覆盖为绝对路径

        bool is_rename = false;

        // 执行重命名
        is_rename = dir.rename(dir_old_path, dir_new_path);

        // 返回响应
        PDU* res_pdu = makePDU();
        addHelper(res_pdu,
                  is_rename ? DIR_FILE_RENAME_DONE : DIR_FILE_RENAME_FAIL,
                  ENUM_MSG_TYPE_RENAME_DIR_FILE_RESPOND);

        delete res_pdu;
        break;
    }

    case ENUM_MSG_TYPE_ENTER_DIR_REQUEST:{                                  //进入子目录请求

        QString old_path = QString::fromUtf8((char*)pdu->caMsg);
        QString dir_name = QString::fromUtf8(pdu->caData);

        //判断是否为文件夹，如果不是则退出
        QString new_path = old_path + "/" + dir_name;
        QFileInfo file_info(new_path);

        if(file_info.isFile()){

            //如果是文件,直接打开
            // 转换为 QUrl 并打开
            QUrl fileUrl = QUrl::fromLocalFile(new_path);
            QDesktopServices::openUrl(fileUrl);
            break;
        }

        //跟刷新文件夹同理操作
        QDir dir(new_path);
        QFileInfoList file_list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        PDU* res_pdu = NULL;

        flushFileHelper(dir, file_list, res_pdu, ENUM_MSG_TYPE_ENTER_DIR_RESPOND);
        //将新的路径发送过去
        qstrncpy(res_pdu->caData, pdu->caData, 64);
        //发送res_pdu,之前的file_info相当于对respdu内存区域进行了操作
        write((char*)res_pdu, res_pdu->uiPDUlen);
        delete res_pdu;
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_REQUEST:{                                     //上传文件请求

        handleUploadRequest(pdu);

        break;
    }

    case ENUM_MSG_TYPE_DOWNLOAD_REQUEST:{                                   //下载文件请求

        handleDownloadRequest(pdu);

        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_REQUEST:{                                 //分享请求

        handleShareFile(pdu);
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_CONFIRM:{                                 //确认接收分享

        handleShareConfirm(pdu);
        break;
    }

    case ENUM_MSG_TYPE_UPLOAD_CANCEL_REQUEST:{                              //取消上传

        handleUploadCancelRequest(pdu);
    }

    case ENUM_MSG_TYPE_DOWNLOAD_CANCEL_REQUEST:{                            //取消下载

        handleCancelDownloadRequest(pdu);
    }

    default:

        break;

    }
}

/****************************************************
*****************************************************
*****************************************************
*****************************************************
*****************************************************
****************************************************/

void MyTcpSocket::addHelper(PDU* &pdu, const char* str, const int type){

    pdu->uiMsgType = type;
    qstrncpy(pdu->caData, str, 64);
    write((char*)pdu, pdu->uiPDUlen);

}

void MyTcpSocket::flushFileHelper(QDir &dir, QFileInfoList &file_list, PDU* &res_pdu, const int type)
{
    //用结构体表示file文件
    FileInfo* file_info = NULL;
    res_pdu = makePDU(sizeof(FileInfo)*file_list.size());
    res_pdu->uiMsgType = type;

    for(int i = 0; i < file_list.size(); i++){

        //把respdu转换为结构体指针类型，加i表示偏移到下一个位置
        file_info = (FileInfo*)res_pdu->caMsg + i;

        //判断类型
        file_info->fileType = (file_list[i].isFile() ? 1 : 0);

        // 文件大小
        file_info->fileSize = file_info->fileType ? file_list[i].size() : 0;

        // 修改日期
        QString dateStr = file_list[i].lastModified().toString("yyyy-MM-dd HH:mm");
        QByteArray utf8Date = dateStr.toUtf8();
        qstrncpy(file_info->lastModified, utf8Date.constData(), sizeof(file_info->lastModified));

        //把名字拷贝到结构体
        QString file_name;
        //如果是文件
        if(file_info->fileType){
            // 文件：确保包含完整扩展名
            QString base = file_list[i].baseName();
            QString suffix = file_list[i].completeSuffix();
            file_name = suffix.isEmpty() ? base : base + "." + suffix;

        }
        else{
            //如果是文件夹则直接拷贝
            file_name = file_list[i].fileName();
        }
        // 转换为 UTF-8 字节数组
        QByteArray utf8Name = file_name.toUtf8();

        // 使用 qstrncpy,保证结尾的 '\0'
        qstrncpy(file_info->fileName, utf8Name.constData(), sizeof(file_info->fileName));

    }

}

//下线功能
void MyTcpSocket::clientOffline()
{
    if (m_isClosing) return;
    m_isClosing = true;

    qDebug() << "***** clientOffline 被调用 *****";

    // 断开 readyRead 连接，避免后续信号干扰
    disconnect(this, &QTcpSocket::readyRead, this, &MyTcpSocket::onReadyRead);

    if (QCoreApplication::instance() && !loginName.isEmpty()) {
        OperateDB::getInstance().handleOffline(loginName.toStdString().c_str());
    }

    emit offline(this);
}

/*----------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------*/


void MyTcpSocket::handleUploadRequest(PDU *pdu)
{

    // 如果正在上传，拒绝新请求（发送失败响应，客户端可据此恢复状态）
    if(upload_state != Idle) {

        qDebug() << "已有文件正在上传，拒绝新请求";
        // 使用 sendUploadResponse 确保格式 "status|fileName" 与客户端解析一致
        sendUploadResponse(FILE_UPLOAD_FAIL, QString());
        return;
    }

    // 解析请求
    QString path = QString::fromUtf8((char*)pdu->caMsg);
    QStringList parts = QString::fromUtf8(pdu->caData).split('|');

    if(parts.size() < 2) {

        qDebug() << "请求格式错误";
        return;
    }

    //原来的名称，要和后续的名字对比
    QString original_file_name = parts[0];
    qint64 file_size = parts[1].toLongLong();

    qDebug() << "文件名:" << original_file_name << "大小:" << file_size;

    // 准备文件
    QString full_path = path + '/' + original_file_name;

    // 文件名相同的时候添加(1)
    QString unique_path = getUniqueName(full_path);
    q_file.setFileName(unique_path);

    // 提取实际文件名（用于响应）
    QString actual_file_name = QFileInfo(unique_path).fileName();


    if(!q_file.open(QIODevice::WriteOnly)) {

        qDebug() << "无法创建文件";
        sendUploadResponse(FILE_UPLOAD_FAIL, actual_file_name);
        upload_state = Idle;
        return;
    }

    // 设置为接收状态
    upload_state = Receiving;
    file_recve_total = file_size;
    file_recve = 0;

    // 发送准备就绪响应，并告知实际文件名
    if (actual_file_name != original_file_name) {

        // 文件被重命名了，发送新文件名
        sendUploadResponse(FILE_UPLOAD_RENAME, actual_file_name);
    }

    else {

        sendUploadResponse(FILE_UPLOAD_PROCESS, actual_file_name);

    }

    qDebug() << "开始接收文件:" << full_path;


    // 立即检查是否有数据（可能请求和数据一起到达）
    if(bytesAvailable() > 0) {

        qDebug() << "立即处理已有数据";
        handleUploadData();
    }
}


// 处理文件上传数据
void MyTcpSocket::handleUploadData()
{
    qDebug() << "handleUploadData调用";

    if (upload_state != Receiving) return;

    // 限制单次读取大小，避免阻塞
    const qint64 MAX_CHUNK = 64 * 1024;  // 64KB
    QByteArray buffer;

    // 读取缓冲区数据块
    if (bytesAvailable() > MAX_CHUNK) {

        buffer = read(MAX_CHUNK);
    }
    else {

        buffer = readAll();
    }

    if (buffer.isEmpty()) return;

    // 写入服务器文件，并返回字节大小
    qint64 written = q_file.write(buffer);

    // 大小不同说明写入失败
    if (written != buffer.size()) {
        handleUploadError();
        return;
    }

    file_recve += written;

    qDebug() << file_recve << " total: " << file_recve_total;

    // 写入完成
    if (file_recve >= file_recve_total) {
        handleUploadComplete();
    }
}


void MyTcpSocket::handleUploadComplete()
{
    qDebug() << "=== handleUploadComplete called ===";

    q_file.close();

    // 发送完成响应
    PDU* pdu = makePDU();
    addHelper(pdu, FILE_UPLOAD_DONE, ENUM_MSG_TYPE_UPLOAD_FINISH);
    delete pdu;


    // 重置文件对象,清空文件名
    q_file.setFileName("");

    // 重置所有状态变量
    upload_state = Idle;
    file_recve_total = 0;
    file_recve = 0;
    m_cancelUpload = false;

    // 注意：不在这里清空缓冲区！
    // onReadyRead() 会在文件数据处理之后、PDU解析之前统一清理非PDU残留数据。
    // 如果这里 readAll()，会吞掉紧随文件数据到达的下一个UPLOAD_REQUEST PDU。

    qDebug() << "=== handleUploadComplete called ===";


}

void MyTcpSocket::handleUploadError()
{
    q_file.close();
    upload_state = Idle;
    m_cancelUpload = false;


    // 发送失败响应
    PDU* pdu = makePDU();
    addHelper(pdu, FILE_UPLOAD_FAIL, ENUM_MSG_TYPE_UPLOAD_PROCESS);
    delete pdu;

}

void MyTcpSocket::sendUploadResponse(const char* status, const QString& fileName)
{
    PDU* pdu = makePDU();
    pdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_PROCESS;

    // 把状态和文件名组合起来发送
    QString response = QString("%1|%2").arg(status).arg(fileName);
    qstrncpy(pdu->caData, response.toUtf8().constData(), 64);

    write((char*)pdu, pdu->uiPDUlen);
    delete pdu;
}

void MyTcpSocket::handleUploadCancelRequest(PDU *pdu)
{
    qDebug() << "收到上传取消请求";

    // 修改状态
    m_cancelUpload = true;

    // 关闭正在接收的文件
    if(q_file.isOpen()) {
        q_file.close();
    }

    QString file_path = q_file.fileName();

    //删除文件
    if(!file_path.isEmpty() && QFile::exists(file_path)){

        QFile::remove(file_path);
        qDebug() << "删除未完成的临时文件:" << file_path;

    }

    //重置
    upload_state = Idle;
    file_recve = 0;
    file_recve_total = 0;
    m_cancelUpload = false;
}

QString MyTcpSocket::getUniqueName(const QString &file_path)
{
    qDebug() << "getUniqueName called with:" << file_path;

    QFileInfo file_info(file_path);
    QString base_name = file_info.completeBaseName();   // 前缀名
    QString suffix = file_info.suffix();        // 后缀名
    QString path = file_info.path();    // 绝对路径

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



/*--------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------*/

void MyTcpSocket::handleDownloadRequest(PDU *pdu)
{
    if(download_state == d_receiving) {
        qDebug() << "已有文件正在下载，拒绝新请求";
        return;
    }

    // 获取路径和名称
    QString file_name = QString::fromUtf8(pdu->caData);
    QString path = QString::fromUtf8((char*)pdu->caMsg) + '/' + file_name;

    QFileInfo fileInfo(path);
    if(!fileInfo.exists() || !fileInfo.isFile()) {
        handleDownloadError("file not exist");
        return;
    }

    download_file = new QFile(path);
    if(!download_file->open(QIODevice::ReadOnly)) {
        handleDownloadError("open failed");
        delete download_file;
        download_file = nullptr;
        return;
    }

    // 获取文件大小
    qint64 file_size = fileInfo.size();

    // 修改状态
    download_state = d_receiving;
    download_total = file_size;
    download_sent = 0;

    // 发送文件名和大小
    PDU* res_pdu = makePDU(0);
    QString dataStr = QString("%1|%2").arg(file_name).arg(file_size);
    addHelper(res_pdu, dataStr.toUtf8().constData(), ENUM_MSG_TYPE_DOWNLOAD_RESPOND);
    delete res_pdu;

    // 启动定时器，每 10ms 发送一块
    m_downloadTimer->start(10);

    qDebug() << "开始下载，文件大小:" << file_size;
}

void MyTcpSocket::sendNextChunk()
{
    if (m_cancelDownload) {
        qDebug() << "下载已取消，忽略后续数据";
        return;
    }
    if(!download_file || download_state != d_receiving) {
        return;
    }
    if(download_file->atEnd()) {
        finishDownload();
        return;
    }


    char buffer[64 * 1024];         // 一次读取64KB
    qint64 bytes_read = download_file->read(buffer, sizeof(buffer));       //实际读取文件的字节数

    if(bytes_read <= 0) {
        finishDownload();
        return;
    }

    PDU* data_pdu = makePDU(bytes_read);
    data_pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_PROCESS;
    memcpy(data_pdu->caMsg, buffer, bytes_read);                    // 将读取到的文件块放到pdu

    qint64 total_len = data_pdu->uiPDUlen;                          // 实际pdu长度
    qint64 bytes_sent = write((char*)data_pdu, total_len);          // 发送pdu,返回实际发送的字节

    // 相同说明发送成功
    if(bytes_sent == total_len) {

        download_sent += bytes_read;

        // 每发送 1MB 打印一次进度
        if(download_sent % (1024 * 1024) == 0 || download_sent == download_total) {

            qDebug() << "下载进度:" << download_sent << "/" << download_total;
        }
    }

    else {

        qDebug() << "发送失败，期望:" << total_len << "实际:" << bytes_sent;
        delete data_pdu;
        handleDownloadError("send failed");
        return;
    }

    delete data_pdu;
}

void MyTcpSocket::finishDownload()
{
    qDebug() << "下载完成，总发送:" << download_sent;

    // 停止定时器
    m_downloadTimer->stop();

    if(download_file) {
        download_file->close();
        delete download_file;
        download_file = nullptr;
    }

    // 发送完成通知
    PDU* finish_pdu = makePDU();
    addHelper(finish_pdu, "download finish", ENUM_MSG_TYPE_DOWNLOAD_FINISH);

    download_state = d_idle;
    download_sent = 0;
    download_total = 0;
    m_cancelDownload = false;  // 重置取消标志
}

void MyTcpSocket::handleDownloadError(const QString& error)
{
    qDebug() << "下载错误:" << error;

    // 停止定时器
    m_downloadTimer->stop();

    if(download_file) {
        download_file->close();
        delete download_file;
        download_file = nullptr;
    }

    PDU* err_pdu = makePDU();
    addHelper(err_pdu, error.toStdString().c_str(), ENUM_MSG_TYPE_DOWNLOAD_ERROR);

    download_state = d_idle;
    download_sent = 0;
    download_total = 0;
    m_cancelDownload = false;  // 重置取消标志
}

void MyTcpSocket::handleCancelDownloadRequest(PDU *pdu)
{
    qDebug() << "取消下载";

    //修改状态
    m_cancelDownload = true;

    //关闭文件
    if(download_file){

        if(download_file->isOpen()){

            download_file->close();
            delete download_file;
            download_file = nullptr;
        }
    }

    download_state = d_idle;
    download_sent = 0;
    download_total = 0;
    m_cancelDownload = false;
}



/*--------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------*/
void MyTcpSocket::handleShareFile(PDU *pdu)
{

    //解析caData，存放的是分享者，接收者数量
    QStringList parts = QString::fromUtf8(pdu->caData).split('|');

    if(parts.size() < 2){

        return;
    }

    QString sender = parts[0];
    int recipient_num = parts[1].toInt();
    int offset = recipient_num * 32;

    //通知接收者的pdu,空间用来存放路径和接收者名字
    PDU* inform_pdu = makePDU(pdu->uiMsglen - offset);
    inform_pdu->uiMsgType = ENUM_MSG_TYPE_FILE_SHARE_INFORM;

    //将sender和文件路径发送
    qstrncpy(inform_pdu->caData, sender.toUtf8().constData(), 64);
    memcpy((char*)inform_pdu->caMsg, (char*)pdu->caMsg + offset, pdu->uiMsglen - offset);

    //解析caMsg，获得接受者名字并转发出去
    for(int i = 0; i < recipient_num; i++){

        QString recipient_name = QString::fromUtf8((char*)pdu->caMsg + i*32);
        //转发名称
        MyTcpServer::getInstance().transcation(recipient_name.toUtf8().constData(), inform_pdu);
    }

    delete inform_pdu;

    PDU* res_pdu = makePDU();
    addHelper(res_pdu, "share done", ENUM_MSG_TYPE_FILE_SHARE_RESPOND);
    delete res_pdu;

}

void MyTcpSocket::handleShareConfirm(PDU *pdu)
{
    //客户端确认接收分享
    QString share_path = QString::fromUtf8((char*)pdu->caMsg);

    //获得文件名
    int last_slash = share_path.lastIndexOf('/');
    QString file_name = share_path.right(share_path.size()-last_slash-1);

    //拷贝名字。因为名字就是根目录名，相当于拷贝根目录
    QString recipient_name = QString::fromUtf8(pdu->caData);
    QString recipient_path = QString("./%1").arg(recipient_name) + '/' + file_name;

    /************************************多线程********************************/

    if(m_threadPool){

        //避免share_path被销毁，因为lambda表达式可能在函数结束后调用
        QString src_path = share_path;
        QString dst_path = recipient_path;

        m_threadPool->enqueue([this, src_path, dst_path](){

            QFileInfo file_info(src_path);

            if(file_info.isFile()){

                QFile::copy(src_path, dst_path);

            }

            else if(file_info.isDir()){

                handleShareDirCopy(src_path, dst_path);
            }

            //主线发送
            QMetaObject::invokeMethod(this, [this](){

                PDU* res_pdu = makePDU();
                addHelper(res_pdu, "share complete", ENUM_MSG_TYPE_FILE_SHARE_DONE);
                delete res_pdu;

            }, Qt::QueuedConnection);

        });

    }

    else{

        //判断是否为文件
        QFileInfo file_info(share_path);
        if(file_info.isFile()){

            //使用静态函数拷贝,把share的拷贝到recipient
            QFile::copy(share_path, recipient_path);
        }
        //如果是文件夹
        else if(file_info.isDir()){

            handleShareDirCopy(share_path, recipient_path);
        }
        //拷贝完成，发送给服务器
        PDU* res_pdu = makePDU();
        addHelper(res_pdu, "share complete", ENUM_MSG_TYPE_FILE_SHARE_DONE);
        delete res_pdu;
    }

}

void MyTcpSocket::handleShareDirCopy(QString dir_src, QString dir_des)
{
    QDir dir;

    // 1. 创建目标目录,使用 mkpath 而不是 mkdir
    if(!dir.mkpath(dir_des)) {
        qDebug() << "创建目录失败:" << dir_des;
        return;
    }

    // 2. 获取源目录内容（排除 . 和 ..）
    dir.setPath(dir_src);
    QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    // 3. 逐个拷贝
    for(const auto& ele : file_list) {

        QString srcPath = dir_src + '/' + ele.fileName();
        QString dstPath = dir_des + '/' + ele.fileName();

        if(ele.isFile()) {

            if(!QFile::copy(srcPath, dstPath)) {

                qDebug() << "拷贝文件失败:" << srcPath;
                // 继续拷贝其他文件，不中断
            }
        }
        else if(ele.isDir()) {
            // 递归拷贝子目录
            handleShareDirCopy(srcPath, dstPath);
        }
    }
}


