#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

#include "connection.h"

class QTreeWidget;
class QTreeWidgetItem;
class QTabWidget;
class QStackedWidget;
class QAction;
class QLabel;
class TableTab;
class QueryTab;

/**
 * @brief 主窗口：左侧连接树 + 右侧数据/查询标签页。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newConnection();
    void editConnection();
    void deleteConnection();
    void connectSelected();
    void disconnectSelected();
    void refreshSelected();
    void newQuery();
    void onTreeDoubleClick(QTreeWidgetItem *item, int column);
    void onItemExpanded(QTreeWidgetItem *item);
    void onTreeContextMenu(const QPoint &pos);
    void onTabCloseRequested(int index);
    void showAbout();

private:
    enum Kind { KindConn = 1, KindGroup = 2, KindTable = 3, KindView = 4,
                KindDatabase = 5 };

    void buildUi();
    void loadConnections();
    QTreeWidgetItem *addConnectionNode(const ConnectionInfo &info);
    void populateConnectionNode(QTreeWidgetItem *connItem);
    void populateTablesInto(QTreeWidgetItem *parent, const QString &connName,
                            const QString &dbName);
    bool ensureDatabaseLoaded(QTreeWidgetItem *dbItem);
    bool openConnection(QTreeWidgetItem *connItem);
    void closeConnectionNode(QTreeWidgetItem *connItem);
    QTreeWidgetItem *currentConnectionItem() const;
    QTreeWidgetItem *findConnectionItem(const QString &name) const;
    ConnectionInfo *findConnectionInfo(const QString &name);
    void clearChildren(QTreeWidgetItem *item);
    bool closeTabsForConnection(const QString &connName);
    void openTableTab(const QString &connName, const QString &dbName,
                      const QString &tableName, bool isView);
    int  findTableTab(const QString &connName, const QString &dbName,
                      const QString &tableName);
    QueryTab *openQueryTab(const QString &connName, const QString &dbName,
                           const QString &initialSql);
    void updateCenter();
    QString quoteIdentifier(const QString &connName, const QString &obj) const;
    QString qualifiedTableName(const QString &connName, const QString &dbName,
                               const QString &obj) const;
    void setConnItemState(QTreeWidgetItem *item, bool connected);

    QTreeWidget    *m_tree;
    QTabWidget     *m_tabs;
    QStackedWidget *m_center;
    QLabel         *m_welcomeLabel;
    QAction        *m_connectAct;
    QAction        *m_disconnectAct;
    QAction        *m_editAct;
    QAction        *m_deleteAct;
    QAction        *m_refreshAct;
    QAction        *m_queryAct;
    int             m_queryCounter;

    QList<ConnectionInfo> m_connections;
};

#endif // MAINWINDOW_H
