#include "operatedb.h"
#include <QMessageBox>
#include <QDebug>
#include <QSqlError>

OperateDB::OperateDB(QObject *parent)
    : QObject{parent}
{

    dataBase = QSqlDatabase::addDatabase("QSQLITE");
}

OperateDB &OperateDB::getInstance()
{
    static OperateDB instance;
    return instance;
}

void OperateDB::init()
{
    dataBase.setHostName("localhost");
    dataBase.setDatabaseName("D:\\CodeStore\\cloudPan\\TcpServer\\cloud.db");

    if(!dataBase.open()){

        QMessageBox::critical(NULL, "open db", "fail");
        return;
    }

    //测试输出数据库
    QSqlQuery sqlQuery;
    sqlQuery.exec("select * from usrInfo");
    while(sqlQuery.next()){

        QString data = QString("%1,%2,%3").arg(sqlQuery.value(0).toString()).arg(sqlQuery.value(1).toString()).arg(sqlQuery.value(2).toString());
        qDebug() << data;
    }
}

OperateDB::~OperateDB()
{
    dataBase.close();
}

bool OperateDB::handleRegist(const char *name, const char *pwd)
{

    if(name == NULL || pwd == NULL){

        return false;
    }

    QString data = QString("insert into usrInfo(name, pwd) values(\'%1\',\'%2\')").arg(name).arg(pwd);
    QSqlQuery query;
    return query.exec(data);

}

bool OperateDB::handleLogin(const char *name, const char *pwd)
{
    if(name == NULL || pwd == NULL){

        return false;
    }

    // 使用参数化查询，防止SQL注入
    QString sql = "UPDATE usrInfo SET online = 1 WHERE name = ? AND pwd = ? AND online = 0";           //预编译SQL语句，? 是参数占位符
    QSqlQuery query;
    query.prepare(sql);                                 //准备sql模板
    query.addBindValue(name);                           //第一个参数
    query.addBindValue(pwd);                            //第二个参数

    if (!query.exec()) {
        qDebug() << "查询失败:" << query.lastError().text();
        return false;
    }

    // 检查是否有行被更新
    bool loginSuccess = (query.numRowsAffected() > 0);
    if (loginSuccess) {
        qDebug() << "用户" << name << "登录成功，在线状态已更新";
    } else {
        qDebug() << "登录失败：用户名或密码错误，或用户已在线";
    }

    return loginSuccess;

}

bool OperateDB::handleLoginOut(const char *name)
{
    if(!name){

        return 0;
    }

    //考虑外键约束
    QString sql1 = "DELETE FROM friendInfo "
                   "WHERE id = (SELECT id FROM usrInfo WHERE name = ?) "
                   "OR friendId = (SELECT id FROM usrInfo WHERE name = ?)";

    QString sql2 = "delete from usrInfo where name = ?";
    QSqlQuery query1, query2;

    query1.prepare(sql1);
    query1.addBindValue(name);
    query1.addBindValue(name);

    query2.prepare(sql2);
    query2.addBindValue(name);

    return query1.exec() && query2.exec();

}

void OperateDB::handleOffline(const char *name)
{
    if(name == nullptr){

        qDebug() << "name is null";
        return ;
    }

    QString sql = "update usrInfo set online = 0 where name = ?";
    QSqlQuery query;
    query.prepare(sql);
    query.addBindValue(name);

    query.exec();

    // 检查是否有行被更新
    bool loginSuccess = (query.numRowsAffected() > 0);
    if (loginSuccess) {
        qDebug() << "用户" << name << "在线状态已更新";
    } else {
        qDebug() << "操作失败";
    }
}

QStringList OperateDB::handleAllOnline()
{
    QString sql = "select name from usrInfo where online = 1";

    QSqlQuery query;
    query.exec(sql);

    QStringList res;
    res.clear();

    while(query.next()){

        res.append(query.value(0).toString());                          //value0就是表格name属性
    }

    return res;

}

int OperateDB::handleSearchUsr(const char *name)            //-1找不到，0表示不在线，1在线
{
    if(!name || !*name){

        return -1;
    }

    QString sql = "select online from usrInfo where name = ?";
    QSqlQuery query;
    query.prepare(sql);
    query.addBindValue(name);

    if (!query.exec()) {
        return -1;
    }

    if(!query.next()){

       return -1;
    }

    return query.value(0).toInt();

}

/***********************************


好友还未添加进数据库 todo



***********************************/



int OperateDB::handleAddFriend(const char *des_name, const char *login_name)        //-1失败,0表示已经是好友,1表示在线，2表示不在线,3表示找不到des_name
{
    if(!des_name || !login_name || !*des_name || !*login_name){

        return -1;
    }

    QString sql = "select * from friendInfo where (id = (select id from usrInfo where name = ?) "
                  "and friendId = (select id from usrInfo where name = ?) ) or "
                  "( friendId = (select id from usrInfo where name = ?) and "
                  "id = (select id from usrInfo where name = ?) )";

    QSqlQuery query;
    query.prepare(sql);
    query.addBindValue(des_name);
    query.addBindValue(login_name);
    query.addBindValue(des_name);
    query.addBindValue(login_name);

    if(!query.exec()){

        return -1;

    }

    if(query.next()){

        return 0;
    }

    //判断对方是否在线

    return handleSearchUsr(des_name) == 1 ? 1 :
          (handleSearchUsr(des_name) == 0 ? 2 : 3);

}

void OperateDB::addFriendDB(const char *des_name, const char *login_name)
{
    if(!des_name || !login_name){

        return;
    }

    QSqlQuery query;
    QString sql = "INSERT INTO friendInfo (id, friendId) "
                  "SELECT u1.id, u2.id "
                  "FROM usrInfo u1, usrInfo u2 "
                  "WHERE u1.name = ? "
                  "  AND u2.name = ? "
                  "  AND NOT EXISTS ("
                  "    SELECT 1 FROM friendInfo f "
                  "    WHERE (f.id = u1.id AND f.friendId = u2.id) "
                  "       OR (f.id = u2.id AND f.friendId = u1.id)"
                  "  )";

    query.prepare(sql);
    query.addBindValue(login_name);
    query.addBindValue(des_name);

    query.exec();

}

QStringList OperateDB::handleFlushFriend(const char *login_name)                    //查询好友列表
{

    QStringList list;
    if(!login_name) return list;

    // 最佳方案：先找出用户ID，然后查询好友
    QSqlQuery query;

    // 1. 获取登录用户的ID
    int login_id = -1;
    query.prepare("SELECT id FROM usrInfo WHERE name = ?");
    query.addBindValue(login_name);
    if(query.exec() && query.next()){
        login_id = query.value(0).toInt();
    }

    if(login_id == -1){
        qDebug() << "用户不存在:" << login_name;
        return list;
    }

    // 2. 查询所有好友（双向查询）
    QString sql = "SELECT u.name as friendName "
                  "FROM usrInfo u "
                  "WHERE u.id IN ("
                  "    SELECT f.friendId FROM friendInfo f WHERE f.id = ? "
                  "    UNION "
                  "    SELECT f.id FROM friendInfo f WHERE f.friendId = ? "
                  ") "
                  "AND u.online = 1 "
                  "AND u.id != ? "  // 排除自己
                  "ORDER BY u.name";

    query.prepare(sql);
    query.addBindValue(login_id);
    query.addBindValue(login_id);
    query.addBindValue(login_id);

    if(!query.exec()){
        qDebug() << "查询失败:" << query.lastError().text();
        return list;
    }

    while(query.next()){
        QString friend_name = query.value("friendName").toString();
        list.append(friend_name);
    }

    return list;
}

void OperateDB::deleteFriend(const char *des_name, const char *login_name)
{
    if(!des_name || !login_name){

        return;
    }

    QSqlQuery query;

    QString sql = "DELETE FROM friendInfo "
                  "WHERE (id = (SELECT id FROM usrInfo WHERE name = ?) "
                  "       AND friendId = (SELECT id FROM usrInfo WHERE name = ?)) "
                  "   OR (id = (SELECT id FROM usrInfo WHERE name = ?) "
                  "       AND friendId = (SELECT id FROM usrInfo WHERE name = ?))";

    query.prepare(sql);
    query.addBindValue(login_name);
    query.addBindValue(des_name);
    query.addBindValue(des_name);
    query.addBindValue(login_name);

    query.exec();

}
