#ifndef CONNECTION_H
#define CONNECTION_H

#include <QString>
#include <QList>

/**
 * @brief 一条数据库连接的配置信息
 *
 * 对应 Navicat 中的一个“连接”。这些信息会被持久化到 connections.ini。
 * 注意：初始版本密码以明文保存，仅供本机学习使用，后续可改为加密存储。
 */
struct ConnectionInfo
{
    QString name;       // 连接显示名（唯一标识）
    QString driver;     // Qt 驱动名：QSQLITE / QODBC / QPSQL ...
    QString host;       // 主机地址（SQLite 不用）
    int     port;       // 端口，0 表示使用驱动默认端口
    QString userName;   // 用户名
    QString password;   // 密码
    QString database;   // SQLite: 数据库文件路径；QODBC: DSN/连接串；其它: 数据库名

    ConnectionInfo();

    bool isFileBased() const;          // QSQLITE 为 true
    bool hasDatabaseList() const;     // QMYSQL：一个连接（实例）下含多个数据库
    int  defaultPort() const;          // 各数据库的默认端口
    QString summary() const;           // 用于状态栏/提示的简要描述
};

/**
 * @brief 连接配置的持久化管理（基于 INI 文件）
 */
class ConnectionStore
{
public:
    static QList<ConnectionInfo> loadAll();
    static void saveAll(const QList<ConnectionInfo> &list);
    static void upsert(const ConnectionInfo &info);  // 按 name 新增或覆盖
    static void remove(const QString &name);

    // INI 文件位于可执行程序同目录，便于查看与携带
    static QString iniFilePath();
};

#endif // CONNECTION_H
