#include "mytcpsocket.h"
#include "mytcpserver.h"
#include <QDebug>
#include <QMessageBox>
#include "operatedb.h"
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QMessageBox>

#define REGIST_DONE "reigst success"
#define REGIST_FAIL "regist fail"
#define LOGIN_DONE "login success"
#define LOGIN_FAIL "login fail"
#define SEARCH_USR_NO "no such user"
#define SEARCH_USR_ONLINE "online"
#define SEARCH_USR_OFFLINE "offline"

#define FRIEND_EXIST "friend exist"
#define DELETE_FINISH "delete finish"

#define DIR_NOT_EXIST "dir not exist"
#define FILE_EXIST "file exist"
#define FILE_CREATE_DONE "file create success"
#define DIR_FILE_DELETE_DONE "delete success"
#define DIR_FILE_DELETE_FAIL "delete fail"
#define DIR_FILE_RENAME_DONE "rename success"
#define DIR_FILE_RENAME_FAIL "rename fail"
#define FILE_UPLOAD_DONE "file upload success"
#define FILE_UPLOAD_PROCESS "file uploading"
#define FILE_UPLOAD_FAIL "file upload fail"

#define UNKNOWN "unknown error"


MyTcpSocket::MyTcpSocket(QObject *parent)
    : QTcpSocket{parent}
{
    upload_state = Idle;
    download_state = d_idle;
    file_recve = 0;
    file_recve_total = 0;
    download_file = NULL;
    download_total = 0;
    download_sent = 0;
    //connect(this, SIGNAL(readyRead()), this, SLOT(receiveMsg()));
    connect(this, SIGNAL(disconnected()), this, SLOT(clientOffline()));
    connect(this, &QTcpSocket::readyRead, this, &MyTcpSocket::onReadyRead);

}

QString MyTcpSocket::getName()
{
    return strName;
}

void MyTcpSocket::receiveMsg()
{
    // ✅ 安全检查：正在上传文件时不处理协议消息
    if(upload_state != Idle) {
        qDebug() << "错误：非空闲状态进入 receiveMsg";
        return;
    }

    // 读取PDU长度
    unsigned int uiPDUlen = 0;
    read((char*)&uiPDUlen, sizeof(unsigned int));

    // 计算消息长度并分配PDU
    unsigned int uiMsgLen = uiPDUlen - sizeof(PDU);
    PDU* pdu = makePDU(uiMsgLen);
    if(!pdu) {
        qDebug() << "内存分配失败";
        return;
    }

    // 读取剩余数据
    qint64 readLen = read((char*)pdu + sizeof(unsigned int), uiPDUlen - sizeof(unsigned int));

    if(readLen != uiPDUlen - sizeof(unsigned int)) {
        qDebug() << "读取PDU数据不完整";
        free(pdu);
        return;
    }

    switch(pdu->uiMsgType){

    case ENUM_MSG_TYPE_REGIST_REQUEST:{                                     //如果是注册请求

        char caName[32] = {'\0'};
        char caPwd[32] = {'\0'};
        PDU* resPdu = makePDU();                                    //响应

        qstrncpy(caName, pdu->caData, 32);                          // Qt的安全版本，自动处理终止符
        qstrncpy(caPwd, pdu->caData + 32, 32);                      // 将pdu的name和pwd输入

        //跟数据库的对比，如果输入的账户密码处理失败
        if(!OperateDB::getInstance().handleRegist(caName, caPwd)){

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

        char caName[32] = {'\0'};
        char caPwd[32] = {'\0'};
        PDU* resPdu = makePDU();                                    //响应

        qstrncpy(caName, pdu->caData, 32);                          // Qt的安全版本，自动处理终止符
        qstrncpy(caPwd, pdu->caData + 32, 32);

        //如果输入的账户密码处理失败

        if(!OperateDB::getInstance().handleLogin(caName, caPwd)){

            qstrcpy(resPdu->caData, LOGIN_FAIL);
            QMessageBox::information(nullptr, "Login", LOGIN_FAIL);
            break;
        }

        resPdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;

        qstrcpy(resPdu->caData, LOGIN_DONE);

        write((char*)resPdu, resPdu->uiPDUlen);                     //将结果返回给客户端

        strName = caName;                                           //将name拷贝

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
            memcpy((char*)(resPdu->caMsg) + i * 32, usrList.at(i).toStdString().c_str(), usrList.at(i).size());
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

        char login_name[32] = {'\0'};
        char des_name[32] = {'\0'};

        qstrncpy(login_name, pdu->caData+32, 32);
        qstrncpy(des_name, pdu->caData, 32);

        int state = OperateDB::getInstance().handleAddFriend(des_name, login_name);
        PDU* res_pdu = makePDU();

        if(state == -1){
            //如果发生错误
            addHelper(res_pdu, UNKNOWN, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);

        }else if(state == 0){
            //0表示已经是好友
            addHelper(res_pdu, FRIEND_EXIST, ENUM_MSG_TYPE_ADD_FRIEND_RESPOND);

        }else if(state == 1){
            //1表示在线
            MyTcpServer::getInstance().transcation(des_name, pdu);


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

        char login_name[32] = {'\0'};
        char des_name[32] = {'\0'};

        qstrncpy(login_name, pdu->caData+32, 32);
        qstrncpy(des_name, pdu->caData, 32);

        OperateDB::getInstance().addFriendDB(des_name, login_name);

        QMessageBox::information(nullptr, "添加好友", "添加成功");

        break;
    }

    case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE:{                                  //拒绝请求


        QMessageBox::information(nullptr, "添加好友", "添加失败");

        break;
    }

    case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:{                               //刷新好友列表

        char login_name[32] = {'\0'};
        qstrncpy(login_name, pdu->caData, 32);

        QStringList list = OperateDB::getInstance().handleFlushFriend(login_name);
        unsigned int uiMsgLen = list.size() * 32;

        PDU* res_pdu = makePDU(uiMsgLen);
        res_pdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;

        for(int i = 0; i < list.size(); i++){

            //把名字拷贝进去
            memcpy((char*)(res_pdu->caMsg) + i * 32, list.at(i).toStdString().c_str(), list.at(i).size());
        }

        write((char*)res_pdu, res_pdu->uiPDUlen);

        delete res_pdu;

        break;
    }

    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:{                              //删除好友请求

        char login_name[32] = {'\0'};
        char des_name[32] = {'\0'};

        qstrncpy(login_name, pdu->caData, 32);
        qstrncpy(des_name, pdu->caData+32, 32);

        OperateDB::getInstance().deleteFriend(des_name, login_name);

        PDU* res_pdu = makePDU();
        res_pdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;

        qstrcpy(res_pdu->caData, DELETE_FINISH);

        write((char*)res_pdu, res_pdu->uiPDUlen);

        delete res_pdu;

        //给删除方的回复
        MyTcpServer::getInstance().transcation(des_name, pdu);

        break;
    }

    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:{                               //私聊请求

        char des_name[32] = {'\0'};
        memcpy(des_name, pdu->caData+32, 32);

        MyTcpServer::getInstance().transcation(des_name, pdu);

        break;
    }

    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:{                                 //群聊请求


        // char login_name[32] = {'\0'};
        // qstrncpy(login_name, pdu->caData, 32);

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

        char dir_name[32] = {'\0'};
        memcpy(dir_name, pdu->caData+32, 32);
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
        // ========== 核心修改：转换为绝对路径（保留原变量名） ==========
        // 将拼接后的相对路径转换为绝对路径，覆盖原变量值
        QFileInfo old_file_info(dir_old_path);
        dir_old_path = old_file_info.absoluteFilePath(); // 覆盖为绝对路径

        //如果是文件
        if (old_file_info.isFile()) {
            // 文件：保留原扩展名
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

    case ENUM_MSG_TYPE_FILE_SHARE_REQUEST:{

        handleShareFile(pdu);
        break;
    }

    case ENUM_MSG_TYPE_FILE_SHARE_CONFIRM:{

        handleShareConfirm(pdu);
        break;
    }


    default:

        break;

    }

    free(pdu);
    pdu = nullptr;

}

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

        //把名字拷贝到结构体
        QString file_name;
        //如果是文件
        if(file_info->fileType){
            // 文件：确保包含完整扩展名
            QString base = file_list[i].baseName();
            QString suffix = file_list[i].completeSuffix();
            file_name = suffix.isEmpty() ? base : base + "." + suffix;

        }else{
            //如果是文件夹则直接拷贝
            file_name = file_list[i].fileName();
        }
        // 转换为 UTF-8 字节数组
        QByteArray utf8Name = file_name.toUtf8();

        // 使用 qstrncpy - 它已经保证了结尾的 '\0'！
        qstrncpy(file_info->fileName, utf8Name.constData(), sizeof(file_info->fileName));

    }

}


void MyTcpSocket::clientOffline()
{
    OperateDB::getInstance().handleOffline(strName.toStdString().c_str());              //将online变成0

    //删除tcpSocketList里面的socket

    emit offline(this);                                                                 //发送下线信号
}

/*-------------------------------------------------------------------------------------------------------*/
void MyTcpSocket::handleUploadRequest(PDU *pdu)
{
    // 如果正在上传，拒绝新请求
    if(upload_state != Idle) {
        qDebug() << "已有文件正在上传，拒绝新请求";
        return;
    }

    // 解析请求
    QString path = QString::fromUtf8((char*)pdu->caMsg);
    QStringList parts = QString::fromUtf8(pdu->caData).split('|');

    if(parts.size() < 2) {
        qDebug() << "请求格式错误";
        return;
    }

    QString file_name = parts[0];
    qint64 file_size = parts[1].toLongLong();

    qDebug() << "文件名:" << file_name << "大小:" << file_size;

    // 准备文件
    QString fullPath = path + '/' + file_name;
    q_file.setFileName(fullPath);

    if(!q_file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建文件";
        sendUploadResponse(FILE_UPLOAD_FAIL);
        return;
    }

    // 设置为接收状态
    upload_state = Receiving;
    file_recve_total = file_size;
    file_recve = 0;
    current_file_name = file_name;

    qDebug() << "开始接收文件:" << fullPath;

    // 发送准备就绪响应
    sendUploadResponse(FILE_UPLOAD_PROCESS);

    // 重要：立即检查是否有数据（可能请求和数据一起到达）
    if(bytesAvailable() > 0) {
        qDebug() << "立即处理已有数据";
        handleUploadData();
    }
}

void MyTcpSocket::onReadyRead()
{
    // 只要还有数据，就循环处理
    while(bytesAvailable() > 0) {
        // 情况1：正在接收文件数据
        if(upload_state == Receiving) {
            handleUploadData();
            // 处理完可能的数据后继续循环，因为可能有多个数据包
            continue;
        }

        // 情况2：空闲状态，处理协议消息
        if(upload_state == Idle) {
            // 尝试解析PDU
            if(!tryParsePDU()) {
                // 无法解析PDU，可能是数据不完整，等待下一次
                break;
            }
            // 解析成功，继续循环处理可能的下一个PDU
            continue;
        }

        // 情况3：其他状态（Preparing等），不应该有数据
        break;
    }
}

// 处理文件上传数据
void MyTcpSocket::handleUploadData()
{
    if(upload_state != Receiving) {
        qDebug() << "错误：非接收状态进入handleUploadData";
        return;
    }

    QByteArray buffer = readAll();
    if(buffer.isEmpty()) return;

    qDebug() << "收到文件数据:" << buffer.size() << "字节";

    qint64 written = q_file.write(buffer);
    if(written != buffer.size()) {
        qDebug() << "写入文件失败";
        handleUploadError();
        return;
    }

    file_recve += written;
    qDebug() << "进度:" << file_recve << "/" << file_recve_total;

    // 检查是否完成
    if(file_recve >= file_recve_total) {
        qDebug() << "文件接收完成，准备调用handleUploadComplete";

        // ✅ 使用定时器延迟执行，避免在当前调用栈中处理
        QTimer::singleShot(0, this, &MyTcpSocket::handleUploadComplete);
    }
}

bool MyTcpSocket::tryParsePDU()
{
    // 检查是否有足够的字节读取PDU长度
    if(bytesAvailable() < sizeof(unsigned int)) {
        return false;
    }

    // 读取PDU长度（但不从缓冲区移除，先peek）
    unsigned int uiPDUlen = 0;
    peek((char*)&uiPDUlen, sizeof(unsigned int));

    // 验证PDU长度的合法性
    if(uiPDUlen < sizeof(PDU) || uiPDUlen > 10 * 1024 * 1024) {
        qDebug() << "非法PDU长度:" << uiPDUlen << "，可能是文件数据？";
        readAll();  // 清空缓冲区
        return false;
    }

    // 检查是否有完整的数据
    if(bytesAvailable() < uiPDUlen) {
        return false;  // 数据不完整，等待更多
    }

    receiveMsg();

    return true;
}

void MyTcpSocket::handleUploadComplete()
{
    q_file.close();

    // 发送完成响应
    PDU* pdu = makePDU();
    addHelper(pdu, FILE_UPLOAD_DONE, ENUM_MSG_TYPE_UPLOAD_FINISH);
    delete pdu;


    // 2. 重置文件对象（重要！）
    q_file.setFileName("");  // 清空文件名

    // 3. 重置所有状态变量
    upload_state = Idle;
    file_recve_total = 0;
    file_recve = 0;

    // 4. 清空可能残留的缓冲区
    while(bytesAvailable() > 0) {
        readAll();
    }

}

void MyTcpSocket::handleUploadError()
{
    q_file.close();
    upload_state = Idle;

    // 发送失败响应
    PDU* pdu = makePDU();
    addHelper(pdu, FILE_UPLOAD_FAIL, ENUM_MSG_TYPE_UPLOAD_PROCESS);
    delete pdu;

}

void MyTcpSocket::sendUploadResponse(const char* status)
{
    PDU* pdu = makePDU();
    addHelper(pdu, status, ENUM_MSG_TYPE_UPLOAD_PROCESS);
    delete pdu;
}

/*-----------------------------------------------------------------------------------------------*/
void MyTcpSocket::handleDownloadRequest(PDU *pdu)
{
    if(download_state == d_receiving){

        qDebug() << "已有文件正在下载，拒绝新请求";
        return;
    }

    //获取路径和名称
    QString file_name = QString::fromUtf8(pdu->caData);
    QString path = QString::fromUtf8((char*)pdu->caMsg) + '/' + file_name;

    QFileInfo fileInfo(path);

    if(!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "文件不存在";

        // 发送错误响应
        handleDownloadError("file not exist");
        return;
    }

    download_file = new QFile(path);

    if(!download_file->open(QIODevice::ReadOnly)){

        handleDownloadError("open failed");
        delete download_file;
        return;
    }

    //获取文件大小
    QFileInfo file_info(path);
    qint64 file_size = file_info.size();

    //修改状态
    download_state = d_receiving;
    download_total = fileInfo.size();
    download_sent = 0;

    //发送文件名和大小
    PDU* res_pdu = makePDU();
    QString dataStr = QString("%1|%2").arg(file_name).arg(file_size);
    addHelper(res_pdu, dataStr.toUtf8().constData(), ENUM_MSG_TYPE_DOWNLOAD_RESPOND);

    delete res_pdu;

    //服务器分块发送文件数据
    QTimer::singleShot(10, this, &MyTcpSocket::sendNextChunk);
}

void MyTcpSocket::sendNextChunk()
{
    if(!download_file) {
        qDebug() << "文件指针为空";
        handleDownloadError("file not open");
        return;
    }

    if(download_state != d_receiving){

        return;
    }

    if(download_file->atEnd()) {
        finishDownload();
        delete download_file;
        return;
    }

    char buffer[4096];
    qint64 bytes_read = download_file->read(buffer, sizeof(buffer));

    if(bytes_read > 0){

        // 创建数据PDU,将文件数据考进去
        PDU* data_pdu = makePDU(bytes_read);
        data_pdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_PROCESS;
        memcpy(data_pdu->caMsg, buffer, bytes_read);

        qint64 bytes_sent = write((char*)data_pdu, data_pdu->uiPDUlen);

        //发送成功时
        if(bytes_sent == data_pdu->uiPDUlen){

            download_sent += bytes_read;
            flush();

            //发送下一块
            QTimer::singleShot(10, this, &MyTcpSocket::sendNextChunk);
        }
        else {
            qDebug() << "发送失败";
            handleDownloadError("send failed");
        }

        delete data_pdu;
    }
    else {
        qDebug() << "读取文件失败";
        handleDownloadError("read failed");
    }

}

void MyTcpSocket::finishDownload()
{
    if(download_file){
    download_file->close();
    download_file = NULL;
    }

    PDU* finish_pdu = makePDU();
    addHelper(finish_pdu, "download finish", ENUM_MSG_TYPE_DOWNLOAD_FINISH);
    delete finish_pdu;
    download_state = d_idle;
}

void MyTcpSocket::handleDownloadError(const QString &error)
{
    //文件关闭
    if(download_file){

        download_file->close();
        download_file = NULL;
    }
    PDU* err_pdu = makePDU();
    addHelper(err_pdu, error.toStdString().c_str(), ENUM_MSG_TYPE_DOWNLOAD_ERROR);
    delete err_pdu;
    download_state = d_idle;
}

/*-----------------------------------------------------------------------------------------------*/
void MyTcpSocket::handleShareFile(PDU *pdu)
{

    //解析caData
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

    //解析caMsg
    char recipient_name[32] = {'\0'};
    for(int i = 0; i < recipient_num; i++){

        memcpy(recipient_name, (char*)pdu->caMsg + i*32, 32);
        //转发名称
        MyTcpServer::getInstance().transcation(recipient_name, inform_pdu);
    }

    delete inform_pdu;

    PDU* res_pdu = makePDU();
    addHelper(res_pdu, "share done", ENUM_MSG_TYPE_FILE_SHARE_RESPOND);
    delete res_pdu;

}



void MyTcpSocket::handleShareConfirm(PDU *pdu)
{

    QString share_path = QString::fromUtf8((char*)pdu->caMsg);

    //获得文件名
    int last_slash = share_path.lastIndexOf('/');
    QString file_name = share_path.right(share_path.size()-last_slash-1);

    //拷贝名字。因为名字就是根目录名，相当于拷贝根目录
    QString recipient_name = QString::fromUtf8(pdu->caData);
    QString recipient_path = QString("./%1").arg(recipient_name) + '/' + file_name;

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

    PDU* res_pdu = makePDU();
    addHelper(res_pdu, "share complete", ENUM_MSG_TYPE_FILE_SHARE_DONE);
    delete res_pdu;
}

void MyTcpSocket::handleShareDirCopy(QString dir_src, QString dir_des)
{
    //将src拷贝到des去
    QDir dir;
    dir.mkdir(dir_des);
    dir.setPath(dir_src);

    //获取目录文件信息
    QFileInfoList file_list = dir.entryInfoList();
    for(auto &ele : file_list){

        //如果是文件
        if(ele.isFile()){

            //获取文件名,拷贝到des目录
            QFile::copy(dir_src + '/' + ele.fileName(),
                        dir_des + '/' + ele.fileName());
        }

        //如果是文件夹
        else if(ele.isDir()){

            //如果是.目录就跳过
            if(QString(".") == ele.fileName()
                || QString("..") == ele.fileName()){

                continue;
            }

            //递归调用该函数
            handleShareDirCopy(dir_src + '/' + ele.fileName(),
                                dir_des + '/' + ele.fileName());
        }
    }
}

