#include "mainwindow.h"

#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QVariant>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDebug>

// 无界面自检：验证 SQLite 驱动可用、内存库增删改查正确、主窗口可构造。
// 用法：dbtool --selftest   成功返回 0，失败返回非 0。
static int runSelfTest()
{
    qInfo("[selftest] available drivers: %s",
          qPrintable(QSqlDatabase::drivers().join(QLatin1Char(','))));

    // 逐个实例化已注册驱动：这会真正加载插件 dll 及其第三方依赖
    // （例如 QMYSQL 依赖 libmysql.dll、libssl/libcrypto）。只实例化、不连接服务器，
    // isValid()==true 即代表插件与依赖 dll 均加载成功。
    // GUI 子系统程序的 qInfo 不进控制台，故把结果写到 exe 同目录的报告文件。
    QStringList report;
    report << QStringLiteral("DbGuiTool selftest - SQL driver probe");
    report << QStringLiteral("drivers: ") + QSqlDatabase::drivers().join(QLatin1Char(','));
    bool mysqlListed = false, mysqlLoadable = false;
    foreach (const QString &drv, QSqlDatabase::drivers()) {
        const QString cnx = QLatin1String("probe_") + drv;
        bool loadable = false;
        {
            QSqlDatabase probe = QSqlDatabase::addDatabase(drv, cnx);
            loadable = probe.isValid();
            probe.close();
        }
        QSqlDatabase::removeDatabase(cnx);
        report << QStringLiteral("  %1 = %2").arg(drv, loadable ? QLatin1String("YES") : QLatin1String("NO"));
        if (drv == QLatin1String("QMYSQL")) {
            mysqlListed = true;
            mysqlLoadable = loadable;
        }
    }

    QFile rf(QCoreApplication::applicationDirPath()
             + QLatin1String("/selftest_report.txt"));
    if (rf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream ts(&rf);
        ts.setCodec("UTF-8");
        ts << report.join(QLatin1Char('\n')) << QLatin1Char('\n');
        rf.close();
    }

    if (!mysqlListed || !mysqlLoadable) {
        qWarning("[selftest] FAIL: QMYSQL not listed/loadable (listed=%d loadable=%d)",
                 mysqlListed, mysqlLoadable);
        return 10;
    }
    qInfo("[selftest] QMYSQL plugin loadable.");

    if (!QSqlDatabase::drivers().contains(QLatin1String("QSQLITE"))) {
        qWarning("[selftest] FAIL: QSQLITE driver not found");
        return 1;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"),
                                                 QLatin1String("selftest"));
    db.setDatabaseName(QLatin1String(":memory:"));
    if (!db.open()) {
        qWarning("[selftest] FAIL: open :memory: -> %s",
                 qPrintable(db.lastError().text()));
        return 1;
    }

    QSqlQuery q(db);
    auto fail = [&](const char *step) {
        qWarning("[selftest] FAIL at %s: %s", step,
                 qPrintable(q.lastError().text()));
        return 2;
    };

    if (!q.exec(QLatin1String(
            "CREATE TABLE t(id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT, age INTEGER)")))
        return fail("create");

    if (!q.exec(QLatin1String("INSERT INTO t(name,age) VALUES "
                              "('alice',30),('bob',25)")))
        return fail("insert");

    if (!q.exec(QLatin1String("UPDATE t SET age=31 WHERE name='alice'")))
        return fail("update");

    if (!q.exec(QLatin1String("SELECT COUNT(*) FROM t")) || !q.next()
            || q.value(0).toInt() != 2) {
        qWarning("[selftest] FAIL: count after insert should be 2");
        return 3;
    }

    if (!q.exec(QLatin1String("SELECT age FROM t WHERE name='alice'"))
            || !q.next() || q.value(0).toInt() != 31) {
        qWarning("[selftest] FAIL: update not effective");
        return 4;
    }

    if (!q.exec(QLatin1String("DELETE FROM t WHERE name='bob'")))
        return fail("delete");

    if (!q.exec(QLatin1String("SELECT COUNT(*) FROM t")) || !q.next()
            || q.value(0).toInt() != 1) {
        qWarning("[selftest] FAIL: count after delete should be 1");
        return 5;
    }

    // QSqlTableModel 路径（TableTab 的增/改/删依赖该机制）
    {
        QSqlTableModel model(0, db);
        model.setTable(QLatin1String("t"));
        model.setEditStrategy(QSqlTableModel::OnManualSubmit);
        if (!model.select()) {
            qWarning("[selftest] FAIL: model.select -> %s",
                     qPrintable(model.lastError().text()));
            return 6;
        }
        const int before = model.rowCount();

        // 新增一行
        if (!model.insertRow(before)) {
            qWarning("[selftest] FAIL: model.insertRow");
            return 7;
        }
        model.setData(model.index(before, 1), QVariant(QLatin1String("carol")));
        model.setData(model.index(before, 2), QVariant(40));
        if (!model.submitAll()) {
            qWarning("[selftest] FAIL: model.submitAll(insert) -> %s",
                     qPrintable(model.lastError().text()));
            return 8;
        }
        if (!model.select() || model.rowCount() != before + 1) {
            qWarning("[selftest] FAIL: row count should be %d after insert",
                     before + 1);
            return 9;
        }

        // 修改 carol 的 age
        int carolRow = -1;
        for (int r = 0; r < model.rowCount(); ++r) {
            if (model.data(model.index(r, 1)).toString() == QLatin1String("carol")) {
                carolRow = r;
                break;
            }
        }
        if (carolRow < 0 || !model.setData(model.index(carolRow, 2), QVariant(41))
                || !model.submitAll()) {
            qWarning("[selftest] FAIL: model update carol -> %s",
                     qPrintable(model.lastError().text()));
            return 10;
        }

        // 删除 carol
        if (!model.removeRow(carolRow) || !model.submitAll()
                || !model.select() || model.rowCount() != before) {
            qWarning("[selftest] FAIL: model delete should restore count to %d",
                     before);
            return 11;
        }
    }

    // 验证主窗口（含连接树、标签页）可以正常构造
    MainWindow w;
    w.show();
    QCoreApplication::processEvents();

    qInfo("[selftest] PASS: SQLite CRUD OK, MainWindow constructed.");
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QLatin1String("DbGuiTool"));
    QCoreApplication::setOrganizationName(QLatin1String("QtLearning"));
    QCoreApplication::setApplicationVersion(QLatin1String("0.1"));

    if (app.arguments().contains(QLatin1String("--selftest")))
        return runSelfTest();

    MainWindow w;
    w.show();
    return app.exec();
}
