#include "mainwindow.h"
#include "connectiondialog.h"
#include "dbmanager.h"
#include "tabletab.h"
#include "querytab.h"

#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QStyle>
#include <QMessageBox>
#include <QApplication>
#include <QCloseEvent>
#include <QFont>
#include <QSqlDatabase>

static const int kRoleConn   = Qt::UserRole;
static const int kRoleKind   = Qt::UserRole + 1;
static const int kRoleObject = Qt::UserRole + 2;
static const int kRoleDatabase = Qt::UserRole + 3; // MySQL 数据库（schema）名，空表示根/单库
static const int kRoleLoaded   = Qt::UserRole + 4; // 数据库节点是否已加载表清单

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_tree(0),
      m_tabs(0),
      m_center(0),
      m_welcomeLabel(0),
      m_connectAct(0),
      m_disconnectAct(0),
      m_editAct(0),
      m_deleteAct(0),
      m_refreshAct(0),
      m_queryAct(0),
      m_queryCounter(0)
{
    buildUi();
    loadConnections();
    updateCenter();
    resize(1180, 760);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("DbGuiTool - 数据库连接工具"));

    // ---- 中央：欢迎页 / 标签页 堆叠 ----
    m_center = new QStackedWidget(this);
    m_welcomeLabel = new QLabel(m_center);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setTextFormat(Qt::RichText);
    m_welcomeLabel->setText(QStringLiteral(
        "<h2>DbGuiTool 数据库连接工具</h2>"
        "<p style='font-size:14px;'>1. 点击工具栏 <b>“新建连接”</b>（SQLite 可点“创建示例库”一键体验）<br>"
        "2. 在左侧 <b>双击连接</b> 打开数据库，展开表/视图<br>"
        "3. <b>双击表</b> 浏览并直接编辑数据（新增 / 删除 / 保存）<br>"
        "4. 点击 <b>“新建查询”</b> 编写并执行 SQL（F5 运行）</p>"
        "<p style='color:#888;'>连接配置保存在程序目录的 connections.ini 中</p>"));

    m_tabs = new QTabWidget(m_center);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);

    m_center->addWidget(m_welcomeLabel);
    m_center->addWidget(m_tabs);
    setCentralWidget(m_center);

    // ---- 左侧连接树 ----
    QDockWidget *dock = new QDockWidget(QStringLiteral("连接"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(260);
    m_tree = new QTreeWidget(dock);
    m_tree->setHeaderLabel(QStringLiteral("数据库连接"));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setUniformRowHeights(true);
    dock->setWidget(m_tree);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    // ---- 动作 ----
    QStyle *st = style();
    QAction *newConnAct = new QAction(
                st->standardIcon(QStyle::SP_FileDialogNewFolder),
                QStringLiteral("新建连接"), this);
    newConnAct->setShortcut(QKeySequence::New);
    m_connectAct = new QAction(
                st->standardIcon(QStyle::SP_DialogYesButton),
                QStringLiteral("连接"), this);
    m_disconnectAct = new QAction(
                st->standardIcon(QStyle::SP_DialogCancelButton),
                QStringLiteral("断开"), this);
    m_refreshAct = new QAction(
                st->standardIcon(QStyle::SP_BrowserReload),
                QStringLiteral("刷新表列表"), this);
    m_queryAct = new QAction(
                st->standardIcon(QStyle::SP_FileIcon),
                QStringLiteral("新建查询"), this);
    m_editAct = new QAction(QStringLiteral("编辑连接"), this);
    m_deleteAct = new QAction(
                st->standardIcon(QStyle::SP_TrashIcon),
                QStringLiteral("删除连接"), this);
    QAction *aboutAct = new QAction(
                st->standardIcon(QStyle::SP_MessageBoxInformation),
                QStringLiteral("关于"), this);
    QAction *aboutQtAct = new QAction(QStringLiteral("关于 Qt"), this);
    QAction *quitAct = new QAction(QStringLiteral("退出"), this);
    quitAct->setShortcut(QKeySequence::Quit);

    connect(newConnAct, &QAction::triggered, this, &MainWindow::newConnection);
    connect(m_connectAct, &QAction::triggered, this, &MainWindow::connectSelected);
    connect(m_disconnectAct, &QAction::triggered, this, &MainWindow::disconnectSelected);
    connect(m_refreshAct, &QAction::triggered, this, &MainWindow::refreshSelected);
    connect(m_queryAct, &QAction::triggered, this, &MainWindow::newQuery);
    connect(m_editAct, &QAction::triggered, this, &MainWindow::editConnection);
    connect(m_deleteAct, &QAction::triggered, this, &MainWindow::deleteConnection);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    // ---- 菜单 ----
    QMenu *connMenu = menuBar()->addMenu(QStringLiteral("连接(&C)"));
    connMenu->addAction(newConnAct);
    connMenu->addAction(m_editAct);
    connMenu->addAction(m_deleteAct);
    connMenu->addSeparator();
    connMenu->addAction(m_connectAct);
    connMenu->addAction(m_disconnectAct);
    connMenu->addAction(m_refreshAct);
    connMenu->addSeparator();
    connMenu->addAction(quitAct);

    QMenu *queryMenu = menuBar()->addMenu(QStringLiteral("查询(&Q)"));
    queryMenu->addAction(m_queryAct);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);

    // ---- 工具栏 ----
    QToolBar *bar = addToolBar(QStringLiteral("主工具栏"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bar->addAction(newConnAct);
    bar->addAction(m_connectAct);
    bar->addAction(m_disconnectAct);
    bar->addAction(m_refreshAct);
    bar->addSeparator();
    bar->addAction(m_queryAct);
    bar->addSeparator();
    bar->addAction(m_editAct);
    bar->addAction(m_deleteAct);

    // ---- 树信号 ----
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onTreeDoubleClick);
    connect(m_tree, &QTreeWidget::itemExpanded,
            this, &MainWindow::onItemExpanded);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);

    statusBar()->showMessage(QStringLiteral("就绪。点击“新建连接”开始。"));
}

void MainWindow::loadConnections()
{
    m_connections = ConnectionStore::loadAll();
    foreach (const ConnectionInfo &info, m_connections)
        addConnectionNode(info);
}

QTreeWidgetItem *MainWindow::addConnectionNode(const ConnectionInfo &info)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, info.name);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    item->setData(0, kRoleConn, info.name);
    item->setData(0, kRoleKind, static_cast<int>(KindConn));
    item->setToolTip(0, info.summary());
    m_tree->addTopLevelItem(item);
    return item;
}

void MainWindow::setConnItemState(QTreeWidgetItem *item, bool connected)
{
    if (!item)
        return;
    item->setIcon(0, style()->standardIcon(
                      connected ? QStyle::SP_DialogYesButton : QStyle::SP_DirIcon));
    QFont f = item->font(0);
    f.setBold(connected);
    item->setFont(0, f);
}

void MainWindow::clearChildren(QTreeWidgetItem *item)
{
    const QList<QTreeWidgetItem *> kids = item->takeChildren();
    qDeleteAll(kids);
}

ConnectionInfo *MainWindow::findConnectionInfo(const QString &name)
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].name == name)
            return &m_connections[i];
    }
    return 0;
}

QTreeWidgetItem *MainWindow::findConnectionItem(const QString &name) const
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->data(0, kRoleConn).toString() == name)
            return it;
    }
    return 0;
}

QTreeWidgetItem *MainWindow::currentConnectionItem() const
{
    QTreeWidgetItem *it = m_tree->currentItem();
    while (it && it->data(0, kRoleKind).toInt() != static_cast<int>(KindConn))
        it = it->parent();
    return it;
}

bool MainWindow::openConnection(QTreeWidgetItem *connItem)
{
    if (!connItem)
        return false;
    const QString name = connItem->data(0, kRoleConn).toString();
    ConnectionInfo *info = findConnectionInfo(name);
    if (!info)
        return false;

    QString err;
    if (!DbManager::instance().open(*info, &err)) {
        QMessageBox::critical(this, QStringLiteral("连接失败"),
                              QStringLiteral("无法连接到“%1”：\n%2").arg(name, err));
        return false;
    }

    populateConnectionNode(connItem);
    setConnItemState(connItem, true);
    statusBar()->showMessage(QStringLiteral("已连接：%1").arg(info->summary()), 6000);
    return true;
}

void MainWindow::populateConnectionNode(QTreeWidgetItem *connItem)
{
    const QString name = connItem->data(0, kRoleConn).toString();
    clearChildren(connItem);

    ConnectionInfo *info = findConnectionInfo(name);

    if (info && info->hasDatabaseList()) {
        // MySQL：一个连接（实例）下挂多个数据库节点；
        // 每个库里的表/视图在首次展开时再懒加载，避免一次性打开大量连接。
        const QStringList dbs = DbManager::instance().databases(name);
        QTreeWidgetItem *defaultNode = 0;
        foreach (const QString &db, dbs) {
            QTreeWidgetItem *dbNode = new QTreeWidgetItem();
            dbNode->setText(0, db);
            dbNode->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));
            dbNode->setData(0, kRoleConn, name);
            dbNode->setData(0, kRoleKind, static_cast<int>(KindDatabase));
            dbNode->setData(0, kRoleDatabase, db);
            dbNode->setData(0, kRoleLoaded, false);
            // 占位子节点，仅用于显示展开箭头，首次加载时会被真实分组替换
            QTreeWidgetItem *placeholder = new QTreeWidgetItem();
            placeholder->setText(0, QStringLiteral("加载中..."));
            dbNode->addChild(placeholder);
            connItem->addChild(dbNode);

            if (!info->database.isEmpty() && db == info->database)
                defaultNode = dbNode;
        }
        connItem->setExpanded(true);

        // 连接时若指定了默认数据库，则自动展开并定位到它
        if (defaultNode) {
            if (ensureDatabaseLoaded(defaultNode))
                defaultNode->setExpanded(true);
            m_tree->setCurrentItem(defaultNode);
        }
    } else {
        // SQLite / ODBC / PostgreSQL：连接即对应单个库，直接挂表/视图分组
        populateTablesInto(connItem, name, QString());
        connItem->setExpanded(true);
    }
}

void MainWindow::populateTablesInto(QTreeWidgetItem *parent,
                                    const QString &connName,
                                    const QString &dbName)
{
    const QStringList tables = DbManager::instance().tables(connName, dbName);
    const QStringList views  = DbManager::instance().views(connName, dbName);

    QTreeWidgetItem *tableGroup = new QTreeWidgetItem();
    tableGroup->setText(0, QStringLiteral("表 (%1)").arg(tables.size()));
    tableGroup->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    tableGroup->setData(0, kRoleConn, connName);
    tableGroup->setData(0, kRoleDatabase, dbName);
    tableGroup->setData(0, kRoleKind, static_cast<int>(KindGroup));
    parent->addChild(tableGroup);

    foreach (const QString &t, tables) {
        QTreeWidgetItem *leaf = new QTreeWidgetItem();
        leaf->setText(0, t);
        leaf->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        leaf->setData(0, kRoleConn, connName);
        leaf->setData(0, kRoleDatabase, dbName);
        leaf->setData(0, kRoleKind, static_cast<int>(KindTable));
        leaf->setData(0, kRoleObject, t);
        tableGroup->addChild(leaf);
    }
    tableGroup->setExpanded(true);

    if (!views.isEmpty()) {
        QTreeWidgetItem *viewGroup = new QTreeWidgetItem();
        viewGroup->setText(0, QStringLiteral("视图 (%1)").arg(views.size()));
        viewGroup->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        viewGroup->setData(0, kRoleConn, connName);
        viewGroup->setData(0, kRoleDatabase, dbName);
        viewGroup->setData(0, kRoleKind, static_cast<int>(KindGroup));
        parent->addChild(viewGroup);
        foreach (const QString &v, views) {
            QTreeWidgetItem *leaf = new QTreeWidgetItem();
            leaf->setText(0, v);
            leaf->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            leaf->setData(0, kRoleConn, connName);
            leaf->setData(0, kRoleDatabase, dbName);
            leaf->setData(0, kRoleKind, static_cast<int>(KindView));
            leaf->setData(0, kRoleObject, v);
            viewGroup->addChild(leaf);
        }
        viewGroup->setExpanded(true);
    }
}

bool MainWindow::ensureDatabaseLoaded(QTreeWidgetItem *dbItem)
{
    if (!dbItem)
        return false;
    if (dbItem->data(0, kRoleKind).toInt() != static_cast<int>(KindDatabase))
        return false;
    if (dbItem->data(0, kRoleLoaded).toBool())
        return true;

    const QString root = dbItem->data(0, kRoleConn).toString();
    const QString db   = dbItem->data(0, kRoleDatabase).toString();

    QString err;
    if (!DbManager::instance().openDatabase(root, db, &err)) {
        QMessageBox::critical(this, QStringLiteral("无法打开数据库"),
                              QStringLiteral("打开数据库“%1”失败：\n%2").arg(db, err));
        return false;
    }

    clearChildren(dbItem); // 移除“加载中...”占位
    populateTablesInto(dbItem, root, db);
    dbItem->setData(0, kRoleLoaded, true);
    return true;
}

void MainWindow::onItemExpanded(QTreeWidgetItem *item)
{
    if (item
            && item->data(0, kRoleKind).toInt() == static_cast<int>(KindDatabase))
        ensureDatabaseLoaded(item);
}


void MainWindow::closeConnectionNode(QTreeWidgetItem *connItem)
{
    if (!connItem)
        return;
    const QString name = connItem->data(0, kRoleConn).toString();
    if (!closeTabsForConnection(name))
        return;
    DbManager::instance().close(name);
    clearChildren(connItem);
    setConnItemState(connItem, false);
    statusBar()->showMessage(QStringLiteral("已断开：%1").arg(name), 4000);
}

void MainWindow::connectSelected()
{
    QTreeWidgetItem *item = currentConnectionItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在左侧选择一个连接。"));
        return;
    }
    const QString name = item->data(0, kRoleConn).toString();
    if (DbManager::instance().isOpen(name)) {
        statusBar()->showMessage(QStringLiteral("“%1”已处于连接状态").arg(name), 3000);
        return;
    }
    openConnection(item);
}

void MainWindow::disconnectSelected()
{
    QTreeWidgetItem *item = currentConnectionItem();
    if (!item)
        return;
    const QString name = item->data(0, kRoleConn).toString();
    if (DbManager::instance().isOpen(name))
        closeConnectionNode(item);
}

void MainWindow::refreshSelected()
{
    QTreeWidgetItem *item = currentConnectionItem();
    if (!item)
        return;
    const QString name = item->data(0, kRoleConn).toString();
    if (!DbManager::instance().isOpen(name)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先连接该数据库。"));
        return;
    }
    populateConnectionNode(item);
    statusBar()->showMessage(QStringLiteral("已刷新表列表：%1").arg(name), 3000);
}

void MainWindow::newConnection()
{
    QStringList names;
    foreach (const ConnectionInfo &c, m_connections)
        names << c.name;

    ConnectionDialog dlg(names, QString(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const ConnectionInfo info = dlg.connectionInfo();
    ConnectionStore::upsert(info);
    m_connections.append(info);
    QTreeWidgetItem *item = addConnectionNode(info);
    m_tree->setCurrentItem(item);

    if (QMessageBox::question(this, QStringLiteral("连接"),
            QStringLiteral("连接“%1”已保存，是否立即连接？").arg(info.name))
            == QMessageBox::Yes) {
        openConnection(item);
    }
}

void MainWindow::editConnection()
{
    QTreeWidgetItem *item = currentConnectionItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择要编辑的连接。"));
        return;
    }
    const QString oldName = item->data(0, kRoleConn).toString();
    int infoIndex = -1;
    ConnectionInfo oldInfo;
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].name == oldName) {
            infoIndex = i;
            oldInfo = m_connections[i];
            break;
        }
    }
    if (infoIndex < 0)
        return;

    QStringList names;
    foreach (const ConnectionInfo &c, m_connections)
        names << c.name;

    ConnectionDialog dlg(names, oldName, this);
    dlg.setConnection(oldInfo);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const ConnectionInfo newInfo = dlg.connectionInfo();
    const bool wasOpen = DbManager::instance().isOpen(oldName);
    if (wasOpen) {
        if (!closeTabsForConnection(oldName))
            return;
        DbManager::instance().close(oldName);
    }
    if (newInfo.name != oldName)
        ConnectionStore::remove(oldName);
    ConnectionStore::upsert(newInfo);
    m_connections[infoIndex] = newInfo;

    // 重建树节点
    const int top = m_tree->indexOfTopLevelItem(item);
    delete m_tree->takeTopLevelItem(top);
    QTreeWidgetItem *newItem = addConnectionNode(newInfo);
    m_tree->setCurrentItem(newItem);

    if (wasOpen)
        openConnection(newItem);
}

void MainWindow::deleteConnection()
{
    QTreeWidgetItem *item = currentConnectionItem();
    if (!item)
        return;
    const QString name = item->data(0, kRoleConn).toString();
    if (QMessageBox::question(this, QStringLiteral("删除连接"),
            QStringLiteral("确定删除连接“%1”吗？（不会删除数据库中的数据）").arg(name))
            != QMessageBox::Yes)
        return;

    if (DbManager::instance().isOpen(name)) {
        if (!closeTabsForConnection(name))
            return;
        DbManager::instance().close(name);
    }
    ConnectionStore::remove(name);
    for (int i = m_connections.size() - 1; i >= 0; --i) {
        if (m_connections[i].name == name)
            m_connections.removeAt(i);
    }
    const int top = m_tree->indexOfTopLevelItem(item);
    delete m_tree->takeTopLevelItem(top);
    updateCenter();
}

QString MainWindow::quoteIdentifier(const QString &connName, const QString &obj) const
{
    QSqlDatabase db = DbManager::instance().database(connName);
    if (db.driverName() == QLatin1String("QODBC"))
        return QLatin1Char('[') + obj + QLatin1Char(']');
    return QLatin1Char('"') + obj + QLatin1Char('"');
}

// 生成可直接用于 SQL 的“限定表名”。
// MySQL/MariaDB：用反引号并加“库名.表名”前缀（连接时已指定库，这里显式限定，
//                可避免当前库不一致或跨库重名；反引号才是 MySQL 的标准标识符引号，
//                原来的双引号在默认 sql_mode 下会被当成字符串）。
// 其他数据库沿用原有引号规则且不做跨库限定：PostgreSQL 连接的是 database（不能作为
// 前缀，schema 才可以），SQLite 没有库级限定，ODBC 的库限定语法各异。
QString MainWindow::qualifiedTableName(const QString &connName,
                                       const QString &dbName,
                                       const QString &obj) const
{
    const QString drv = DbManager::instance().database(connName).driverName();

    if (drv == QLatin1String("QMYSQL") || drv == QLatin1String("QMYSQL3")) {
        auto backtick = [](const QString &id) {
            QString escaped = id;
            escaped.replace(QLatin1Char('`'), QStringLiteral("``"));
            return QLatin1Char('`') + escaped + QLatin1Char('`');
        };
        const QString table = backtick(obj);
        if (dbName.isEmpty())
            return table;
        return backtick(dbName) + QLatin1Char('.') + table;
    }

    return quoteIdentifier(connName, obj);
}

void MainWindow::newQuery()
{
    QTreeWidgetItem *connItem = currentConnectionItem();
    if (!connItem) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个连接。"));
        return;
    }
    const QString name = connItem->data(0, kRoleConn).toString();
    if (!DbManager::instance().isOpen(name)) {
        if (!openConnection(connItem))
            return;
    }

    QString dbName;
    QString initialSql;
    QTreeWidgetItem *cur = m_tree->currentItem();
    if (cur) {
        const int kind = cur->data(0, kRoleKind).toInt();
        if (kind == static_cast<int>(KindTable)
                || kind == static_cast<int>(KindView)) {
            dbName = cur->data(0, kRoleDatabase).toString();
            const QString obj = cur->data(0, kRoleObject).toString();
            initialSql = QStringLiteral("SELECT * FROM %1;")
                    .arg(qualifiedTableName(name, dbName, obj));
        } else if (kind == static_cast<int>(KindDatabase)) {
            dbName = cur->data(0, kRoleDatabase).toString();
            QString err;
            if (DbManager::instance().openDatabase(name, dbName, &err)
                    && ensureDatabaseLoaded(cur)) {
                cur->setExpanded(true);
                initialSql = QStringLiteral("-- 当前数据库：%1\nSHOW TABLES;")
                                     .arg(dbName);
            }
        }
    }
    openQueryTab(name, dbName, initialSql);
}

void MainWindow::onTreeDoubleClick(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;
    const int kind = item->data(0, kRoleKind).toInt();
    const QString conn = item->data(0, kRoleConn).toString();

    if (kind == static_cast<int>(KindConn)) {
        if (!DbManager::instance().isOpen(conn)) {
            openConnection(item);
        } else {
            item->setExpanded(!item->isExpanded());
        }
    } else if (kind == static_cast<int>(KindDatabase)) {
        if (ensureDatabaseLoaded(item))
            item->setExpanded(!item->isExpanded());
    } else if (kind == static_cast<int>(KindTable)) {
        openTableTab(conn, item->data(0, kRoleDatabase).toString(),
                     item->data(0, kRoleObject).toString(), false);
    } else if (kind == static_cast<int>(KindView)) {
        openTableTab(conn, item->data(0, kRoleDatabase).toString(),
                     item->data(0, kRoleObject).toString(), true);
    } else if (kind == static_cast<int>(KindGroup)) {
        item->setExpanded(!item->isExpanded());
    }
}

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item)
        m_tree->setCurrentItem(item);

    QTreeWidgetItem *connItem = currentConnectionItem();
    const bool hasConn = (connItem != 0);
    const bool open = hasConn
            && DbManager::instance().isOpen(
                connItem->data(0, kRoleConn).toString());

    m_connectAct->setEnabled(hasConn && !open);
    m_disconnectAct->setEnabled(open);
    m_refreshAct->setEnabled(open);
    m_editAct->setEnabled(hasConn);
    m_deleteAct->setEnabled(hasConn);
    m_queryAct->setEnabled(hasConn);

    QMenu menu(this);
    menu.addAction(m_connectAct);
    menu.addAction(m_disconnectAct);
    menu.addAction(m_refreshAct);
    menu.addSeparator();
    menu.addAction(m_queryAct);
    menu.addSeparator();
    menu.addAction(m_editAct);
    menu.addAction(m_deleteAct);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

int MainWindow::findTableTab(const QString &connName, const QString &dbName,
                             const QString &tableName)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *w = m_tabs->widget(i);
        if (w->property("kind").toString() == QLatin1String("table")
                && w->property("conn").toString() == connName
                && w->property("db").toString() == dbName
                && w->property("table").toString() == tableName)
            return i;
    }
    return -1;
}

void MainWindow::openTableTab(const QString &connName, const QString &dbName,
                              const QString &tableName, bool isView)
{
    const int existing = findTableTab(connName, dbName, tableName);
    if (existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return;
    }

    TableTab *tab = new TableTab(connName, dbName, tableName, isView, this);
    if (!tab->loadError().isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("无法打开表"),
                              QStringLiteral("打开表“%1”失败：\n%2")
                                  .arg(tableName, tab->loadError()));
        delete tab;
        return;
    }
    tab->setProperty("kind", QLatin1String("table"));
    tab->setProperty("conn", connName);
    tab->setProperty("db", dbName);
    tab->setProperty("table", tableName);

    const QString title = dbName.isEmpty()
            ? QStringLiteral("%1 · %2").arg(connName, tableName)
            : QStringLiteral("%1.%2").arg(dbName, tableName);
    const int idx = m_tabs->addTab(tab, title);
    m_tabs->setCurrentIndex(idx);
    updateCenter();
}

QueryTab *MainWindow::openQueryTab(const QString &connName, const QString &dbName,
                                   const QString &initialSql)
{
    QueryTab *tab = new QueryTab(connName, dbName, this);
    if (!initialSql.isEmpty())
        tab->setSql(initialSql);
    tab->setProperty("kind", QLatin1String("query"));
    tab->setProperty("conn", connName);
    tab->setProperty("db", dbName);

    const QString target = dbName.isEmpty() ? connName
                                            : QStringLiteral("%1/%2").arg(connName, dbName);
    ++m_queryCounter;
    const int idx = m_tabs->addTab(
                tab, QStringLiteral("查询%1 · %2").arg(m_queryCounter).arg(target));
    m_tabs->setCurrentIndex(idx);
    updateCenter();
    return tab;
}

bool MainWindow::closeTabsForConnection(const QString &connName)
{
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        QWidget *w = m_tabs->widget(i);
        if (w->property("conn").toString() != connName)
            continue;
        TableTab *tt = qobject_cast<TableTab *>(w);
        if (tt && !tt->confirmClose())
            return false;
        m_tabs->removeTab(i);
        delete w;
    }
    updateCenter();
    return true;
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *w = m_tabs->widget(index);
    TableTab *tt = qobject_cast<TableTab *>(w);
    if (tt && !tt->confirmClose())
        return;
    m_tabs->removeTab(index);
    delete w;
    updateCenter();
}

void MainWindow::updateCenter()
{
    m_center->setCurrentIndex(m_tabs->count() > 0 ? 1 : 0);
}

void MainWindow::showAbout()
{
    const QString drivers = QSqlDatabase::drivers().join(QStringLiteral(", "));
    QMessageBox::about(this, QStringLiteral("关于 DbGuiTool"),
        QStringLiteral(
            "<h3>DbGuiTool</h3>"
            "<p>一个使用 C++ / Qt 编写的简易数据库 GUI 连接工具（类 Navicat 初始版本）。</p>"
            "<p>功能：连接管理、表数据浏览与直接编辑（增/删/改/查）、"
            "SQL 多语句执行、结果与数据导出 CSV。</p>"
            "<p>当前可用 Qt 数据库驱动：<b>%1</b></p>"
            "<p style='color:#888;'>初始版本密码明文保存于 connections.ini，仅供学习。</p>")
            .arg(drivers));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 先确认所有表编辑页是否需要保存
    for (int i = 0; i < m_tabs->count(); ++i) {
        TableTab *tt = qobject_cast<TableTab *>(m_tabs->widget(i));
        if (tt && !tt->confirmClose()) {
            event->ignore();
            return;
        }
    }
    // 删除所有标签页（释放持有连接的 model/query）后再关闭连接
    while (m_tabs->count() > 0) {
        QWidget *w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        delete w;
    }
    DbManager::instance().closeAll();
    event->accept();
}
