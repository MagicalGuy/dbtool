#include "dbmanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QCoreApplication>
#include <QUuid>

DbManager &DbManager::instance()
{
    static DbManager s_instance;
    return s_instance;
}

DbManager::DbManager()
    : QObject(0)
{
}

QString DbManager::connectionName(const QString &baseName)
{
    return QLatin1String("dbtool_cnx_") + baseName;
}

void DbManager::applyParams(QSqlDatabase &db, const ConnectionInfo &info)
{
    if (info.driver == QLatin1String("QSQLITE")) {
        // SQLite：数据库名就是文件路径（:memory: 表示内存库）
        db.setDatabaseName(info.database);
    } else if (info.driver == QLatin1String("QODBC")) {
        // ODBC：数据库名处填写 DSN 名称或完整 DRIVER=... 连接串
        db.setDatabaseName(info.database);
        if (!info.userName.isEmpty())
            db.setUserName(info.userName);
        if (!info.password.isEmpty())
            db.setPassword(info.password);
    } else {
        db.setHostName(info.host);
        if (info.port > 0)
            db.setPort(info.port);
        db.setDatabaseName(info.database);
        db.setUserName(info.userName);
        db.setPassword(info.password);
    }
}

bool DbManager::openWithKey(const ConnectionInfo &info, const QString &key,
                            QString *error)
{
    QSqlDatabase db;
    if (QSqlDatabase::contains(key)) {
        db = QSqlDatabase::database(key, false);
    } else {
        db = QSqlDatabase::addDatabase(info.driver, key);
    }

    applyParams(db, info);

    if (db.isOpen())
        return true;

    if (!db.open()) {
        if (error) *error = db.lastError().text().trimmed();
        return false;
    }
    return true;
}

bool DbManager::open(const ConnectionInfo &info, QString *error)
{
    if (info.name.isEmpty()) {
        if (error) *error = QStringLiteral("连接名称不能为空");
        return false;
    }
    if (!QSqlDatabase::drivers().contains(info.driver)) {
        if (error)
            *error = QStringLiteral("当前 Qt 未包含数据库驱动：%1").arg(info.driver);
        return false;
    }
    if (info.driver == QLatin1String("QSQLITE") && info.database.isEmpty()) {
        if (error) *error = QStringLiteral("请先选择 SQLite 数据库文件");
        return false;
    }

    const QString cn = connectionName(info.name);
    if (!openWithKey(info, cn, error))
        return false;

    m_rootInfos.insert(info.name, info);
    return true;
}

bool DbManager::openDatabase(const QString &rootName, const QString &dbName,
                             QString *error)
{
    if (!m_rootInfos.contains(rootName)) {
        if (error)
            *error = QStringLiteral("根连接尚未打开：%1").arg(rootName);
        return false;
    }

    const QString key = dbConnectionName(rootName, dbName);
    if (QSqlDatabase::contains(key)) {
        QSqlDatabase existing = QSqlDatabase::database(key, false);
        if (existing.isOpen())
            return true;
    }

    ConnectionInfo info = m_rootInfos.value(rootName);
    info.database = dbName;
    return openWithKey(info, key, error);
}

QString DbManager::dbConnectionName(const QString &rootName, const QString &dbName)
{
    return connectionName(rootName) + QLatin1String("#db#") + dbName;
}

void DbManager::close(const QString &name)
{
    const QString rootKey = connectionName(name);
    const QString dbPrefix = rootKey + QLatin1String("#db#");

    // 先关闭该根连接下派生的所有数据库连接
    foreach (const QString &cn, QSqlDatabase::connectionNames()) {
        if (!cn.startsWith(dbPrefix))
            continue;
        {
            QSqlDatabase db = QSqlDatabase::database(cn, false);
            if (db.isValid() && db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(cn);
    }

    // 再关闭根连接
    if (QSqlDatabase::contains(rootKey)) {
        {
            QSqlDatabase db = QSqlDatabase::database(rootKey, false);
            if (db.isValid() && db.isOpen())
                db.close();
        }
        // 必须在所有 QSqlDatabase/query 句柄离开作用域后再 remove
        QSqlDatabase::removeDatabase(rootKey);
    }
    m_rootInfos.remove(name);
}

void DbManager::closeAll()
{
    const QStringList names = QSqlDatabase::connectionNames();
    foreach (const QString &cn, names) {
        if (!cn.startsWith(QLatin1String("dbtool_cnx_")))
            continue;
        {
            QSqlDatabase db = QSqlDatabase::database(cn, false);
            if (db.isValid() && db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(cn);
    }
    m_rootInfos.clear();
}

bool DbManager::isOpen(const QString &name) const
{
    const QString cn = connectionName(name);
    if (!QSqlDatabase::contains(cn))
        return false;
    QSqlDatabase db = QSqlDatabase::database(cn, false);
    return db.isValid() && db.isOpen();
}

QSqlDatabase DbManager::database(const QString &name, const QString &dbName) const
{
    const QString key = dbName.isEmpty()
            ? connectionName(name)
            : dbConnectionName(name, dbName);
    if (!QSqlDatabase::contains(key))
        return QSqlDatabase();
    return QSqlDatabase::database(key, false);
}

QStringList DbManager::tables(const QString &name, const QString &dbName) const
{
    QSqlDatabase db = database(name, dbName);
    if (!db.isOpen())
        return QStringList();
    return db.tables(QSql::Tables);
}

QStringList DbManager::views(const QString &name, const QString &dbName) const
{
    QSqlDatabase db = database(name, dbName);
    if (!db.isOpen())
        return QStringList();
    return db.tables(QSql::Views);
}

QStringList DbManager::databases(const QString &name) const
{
    QSqlDatabase db = database(name);
    QStringList result;
    if (!db.isOpen())
        return result;
    if (db.driverName() != QLatin1String("QMYSQL")
            && db.driverName() != QLatin1String("QMYSQL3"))
        return result;

    QSqlQuery q(db);
    if (q.exec(QLatin1String("SHOW DATABASES"))) {
        while (q.next())
            result << q.value(0).toString();
    }
    return result;
}

bool DbManager::testConnection(const ConnectionInfo &info, QString *error)
{
    if (!QSqlDatabase::drivers().contains(info.driver)) {
        if (error)
            *error = QStringLiteral("当前 Qt 未包含数据库驱动：%1").arg(info.driver);
        return false;
    }

    const QString tcn = QLatin1String("dbtool_test_")
            + QString::number(QCoreApplication::applicationPid())
            + QLatin1Char('_') + QUuid::createUuid().toString(QUuid::Id128);

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(info.driver, tcn);
        applyParams(db, info);
        if (!db.open()) {
            if (error) *error = db.lastError().text().trimmed();
            return false;
        }
        // 执行一次简单查询，确认连接真正可用（MySQL 未指定库时 SELECT 1 也合法）
        QSqlQuery q(db);
        if (!q.exec(QLatin1String("SELECT 1"))) {
            if (error) *error = q.lastError().text().trimmed();
            return false;
        }
    }
    QSqlDatabase::removeDatabase(tcn);
    return true;
}
