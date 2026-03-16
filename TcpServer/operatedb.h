#ifndef OPERATEDB_H
#define OPERATEDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>

class OperateDB : public QObject
{
    Q_OBJECT
public:
    explicit OperateDB(QObject *parent = nullptr);

    static OperateDB& getInstance();                        //单例模式，使用静态方法
    void init();

    ~OperateDB();

    bool handleRegist(const char* name, const char* pwd);   //处理注册部分数据库
    bool handleLogin(const char* name, const char* pwd);
    bool handleLoginOut(const char* name);
    void handleOffline(const char* name);
    QStringList handleAllOnline();                          //处理查看在线用户请求，返回一个列表
    int handleSearchUsr(const char* name);                  //按名字查询用户
    int handleAddFriend(const char* des_name, const char* login_name);    //添加好友
    void addFriendDB(const char* des_name, const char* login_name);       //添加进数据库
    QStringList handleFlushFriend(const char* login_name);                //刷新好友
    void deleteFriend(const char* des_name, const char* login_name);      //删除好友

signals:

public slots:

private:

    QSqlDatabase dataBase;                                  //连接数据库

};

#endif // OPERATEDB_H
