#ifndef TABLETAB_H
#define TABLETAB_H

#include <QWidget>
#include <QString>

class QSqlTableModel;
class QTableView;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QSqlDatabase;

/**
 * @brief 单张表（或视图）的数据浏览与编辑标签页。
 *
 *  使用 QSqlTableModel + OnManualSubmit：
 *  - 双击单元格直接编辑
 *  - 新增 / 删除行，最后统一“保存修改”或“撤销”
 *  - 单列模糊过滤、点击表头排序、导出 CSV
 */
class TableTab : public QWidget
{
    Q_OBJECT
public:
    TableTab(const QString &connName, const QString &dbName,
             const QString &tableName,
             bool readOnly, QWidget *parent = 0);
    ~TableTab();

    QString connectionName() const { return m_connName; }
    QString dbName() const { return m_dbName; }
    QString tableName() const { return m_tableName; }
    QString loadError() const { return m_loadError; }

    // 关闭/切换连接前调用：若有未保存修改提示保存，返回 false 表示用户取消
    bool confirmClose();

private slots:
    void addRow();
    void deleteRows();
    void saveChanges();
    void revertChanges();
    void refresh();
    void applyFilter();
    void clearFilter();
    void exportCsv();
    void updateStatus();

private:
    void buildUi();
    void populateFilterColumns();
    QString quotedIdentifier(const QString &field) const;

    QString m_connName;
    QString m_dbName;
    QString m_tableName;
    bool    m_readOnly;
    QString m_loadError;

    QSqlTableModel *m_model;
    QTableView     *m_view;
    QLineEdit      *m_filterEdit;
    QComboBox      *m_filterColumn;
    QLabel         *m_statusLabel;

    QPushButton *m_addBtn;
    QPushButton *m_delBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_revertBtn;
};

#endif // TABLETAB_H
