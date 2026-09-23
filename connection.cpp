#include "connection.h"

#include <QCoreApplication>
#include <QSettings>
#include <QDir>

ConnectionInfo::ConnectionInfo()
    : port(0)
{
}

bool ConnectionInfo::isFileBased() const
{
    return driver == QLatin1String("QSQLITE");
}

bool ConnectionInfo::hasDatabaseList() const
{
    // 目前仅 MySQL/MariaDB 支持“一个连接下枚举多个数据库”
    return driver == QLatin1String("QMYSQL")
            || driver == QLatin1String("QMYSQL3");
}

int ConnectionInfo::defaultPort() const
{
    if (driver == QLatin1String("QPSQL"))
        return 5432;
    if (driver == QLatin1String("QMYSQL") || driver == QLatin1String("QMYSQL3"))
        return 3306;
    return 0;
}

QString ConnectionInfo::summary() const
{
    if (isFileBased())
        return QStringLiteral("%1 [%2] %3").arg(name, driver, database);
    const QString db = database.isEmpty()
            ? QStringLiteral("(所有数据库)") : database;
    return QStringLiteral("%1 [%2] %3@%4:%5/%6")
            .arg(name, driver, userName, host)
            .arg(port)
            .arg(db);
}

QString ConnectionStore::iniFilePath()
{
    return QCoreApplication::applicationDirPath()
            + QDir::separator() + QLatin1String("connections.ini");
}

QList<ConnectionInfo> ConnectionStore::loadAll()
{
    QList<ConnectionInfo> result;
    QSettings settings(iniFilePath(), QSettings::IniFormat);
    settings.setFallbacksEnabled(false);

    const int count = settings.value(QLatin1String("general/count"), 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString g = QStringLiteral("conn%1").arg(i);
        ConnectionInfo info;
        info.name     = settings.value(g + QLatin1String("/name")).toString();
        info.driver   = settings.value(g + QLatin1String("/driver"), QLatin1String("QSQLITE")).toString();
        info.host     = settings.value(g + QLatin1String("/host")).toString();
        info.port     = settings.value(g + QLatin1String("/port"), 0).toInt();
        info.userName = settings.value(g + QLatin1String("/user")).toString();
        info.password = settings.value(g + QLatin1String("/password")).toString();
        info.database = settings.value(g + QLatin1String("/database")).toString();
        if (!info.name.isEmpty())
            result.append(info);
    }
    return result;
}

static void writeGroup(QSettings &settings, int index, const ConnectionInfo &info)
{
    const QString g = QStringLiteral("conn%1").arg(index);
    settings.setValue(g + QLatin1String("/name"), info.name);
    settings.setValue(g + QLatin1String("/driver"), info.driver);
    settings.setValue(g + QLatin1String("/host"), info.host);
    settings.setValue(g + QLatin1String("/port"), info.port);
    settings.setValue(g + QLatin1String("/user"), info.userName);
    settings.setValue(g + QLatin1String("/password"), info.password);
    settings.setValue(g + QLatin1String("/database"), info.database);
}

void ConnectionStore::saveAll(const QList<ConnectionInfo> &list)
{
    QSettings settings(iniFilePath(), QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.remove(QString()); // 清空旧内容，整体重写

    settings.beginGroup(QLatin1String("general"));
    settings.setValue(QLatin1String("count"), list.size());
    settings.endGroup();

    for (int i = 0; i < list.size(); ++i)
        writeGroup(settings, i, list[i]);
}

void ConnectionStore::upsert(const ConnectionInfo &info)
{
    QList<ConnectionInfo> list = loadAll();
    bool replaced = false;
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].name == info.name) {
            list[i] = info;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        list.append(info);
    saveAll(list);
}

void ConnectionStore::remove(const QString &name)
{
    QList<ConnectionInfo> list = loadAll();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i].name == name)
            list.removeAt(i);
    }
    saveAll(list);
}
