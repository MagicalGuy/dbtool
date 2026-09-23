#ifndef QUERYTAB_H
#define QUERYTAB_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QPlainTextEdit;
class QTableView;
class QSqlQueryModel;
class QLabel;

/**
 * @brief SQL 查询编辑器标签页。
 *
 *  - 支持按分号拆分多条语句依次执行（能正确跳过引号与注释中的分号）
 *  - 最后一条结果集语句（SELECT / PRAGMA 等）的结果显示在表格中
 *  - 非查询语句（INSERT/UPDATE/DELETE/DDL）统计 affected rows
 *  - 结果导出 CSV、F5 / Ctrl+Enter 快捷运行
 */
class QueryTab : public QWidget
{
    Q_OBJECT
public:
    QueryTab(const QString &connName, const QString &dbName,
             QWidget *parent = 0);
    ~QueryTab();

    QString connectionName() const { return m_connName; }
    QString dbName() const { return m_dbName; }
    void setSql(const QString &sql);

private slots:
    void run();
    void clearEditor();
    void exportCsv();

private:
    void buildUi();
    static QString firstKeyword(const QString &stmt);
    static bool returnsResultSet(const QString &stmt);
    static QStringList splitStatements(const QString &sql);

    QString m_connName;
    QString m_dbName;
    QPlainTextEdit *m_editor;
    QTableView     *m_view;
    QSqlQueryModel *m_model;
    QLabel         *m_msgLabel;
};

#endif // QUERYTAB_H
