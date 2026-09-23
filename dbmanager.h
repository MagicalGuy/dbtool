#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include <QString>
#include <QHash>

#include "connection.h"

/**
 * @brief 统一管理所有已打开的数据库连接（单例）。
 *
 * Qt 用“连接名”区分不同的 QSqlDatabase，这里在用户定义的连接名前
 * 加固定前缀，避免与临时连接（如测试连接）冲突。
 *
 * 对 MySQL 这类“一个实例下有多个数据库”的驱动：
 *  - 根连接（dbName 为空）用于登录实例并执行 SHOW DATABASES；
 *  - 每个被打开的数据库再建立一条独立连接（内部名带 #db# 后缀），
 *    这样表数据编辑（QSqlTableModel）在各自的默认库内工作，互不干扰。
 */
class DbManager : public QObject
{
    Q_OBJECT
public:
    static DbManager &instance();

    // 打开（或复用）一条“根连接”，成功返回 true，失败把原因写入 error
    bool open(const ConnectionInfo &info, QString *error);

    // 打开（或复用）某根连接下指定数据库的独立连接（仅多库驱动需要）
    bool openDatabase(const QString &rootName, const QString &dbName,
                      QString *error = 0);

    // 关闭一条根连接及其下所有数据库连接。调用前必须确保相关 model/query 已销毁。
    void close(const QString &name);
    void closeAll();

    bool isOpen(const QString &name) const;

    // dbName 为空返回根连接，否则返回该数据库的独立连接（未打开则为无效连接）
    QSqlDatabase database(const QString &name,
                          const QString &dbName = QString()) const;

    QStringList tables(const QString &name,
                       const QString &dbName = QString()) const;  // 用户表
    QStringList views(const QString &name,
                      const QString &dbName = QString()) const;   // 视图

    // 列出某根连接下该账号可访问的所有数据库（MySQL：SHOW DATABASES）
    QStringList databases(const QString &name) const;

    // Qt 内部连接名
    static QString connectionName(const QString &baseName);
    static QString dbConnectionName(const QString &rootName, const QString &dbName);

    // 供“测试连接”使用：按配置直接尝试打开一个一次性连接
    static bool testConnection(const ConnectionInfo &info, QString *error);

private:
    DbManager();
    Q_DISABLE_COPY(DbManager)

    static void applyParams(QSqlDatabase &db, const ConnectionInfo &info);
    bool openWithKey(const ConnectionInfo &info, const QString &key, QString *error);

    // 根连接名 -> 其配置（用于派生各数据库连接）
    QHash<QString, ConnectionInfo> m_rootInfos;
};

#endif // DBMANAGER_H
