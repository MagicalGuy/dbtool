#include "tabletab.h"
#include "dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlRecord>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QModelIndexList>

#include <algorithm>

TableTab::TableTab(const QString &connName, const QString &dbName,
                   const QString &tableName,
                   bool readOnly, QWidget *parent)
    : QWidget(parent),
      m_connName(connName),
      m_dbName(dbName),
      m_tableName(tableName),
      m_readOnly(readOnly),
      m_model(0),
      m_view(0),
      m_filterEdit(0),
      m_filterColumn(0),
      m_statusLabel(0),
      m_addBtn(0),
      m_delBtn(0),
      m_saveBtn(0),
      m_revertBtn(0)
{
    buildUi();

    QSqlDatabase db = DbManager::instance().database(m_connName, m_dbName);
    if (!db.isOpen()) {
        m_loadError = QStringLiteral("数据库连接未打开");
        return;
    }

    m_model = new QSqlTableModel(this, db);
    m_model->setTable(m_tableName);
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    if (!m_model->select()) {
        m_loadError = m_model->lastError().text().trimmed();
        return;
    }

    m_view->setModel(m_model);
    m_view->resizeColumnsToContents();

    populateFilterColumns();
    updateStatus();

    connect(m_model, &QSqlTableModel::dataChanged, this, &TableTab::updateStatus);
    connect(m_model, &QSqlTableModel::rowsInserted, this, &TableTab::updateStatus);
    connect(m_model, &QSqlTableModel::rowsRemoved, this, &TableTab::updateStatus);
    connect(m_model, &QSqlTableModel::layoutChanged, this, &TableTab::updateStatus);
}

TableTab::~TableTab()
{
}

void TableTab::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout *top = new QHBoxLayout();
    m_addBtn    = new QPushButton(QStringLiteral("新增行"), this);
    m_delBtn    = new QPushButton(QStringLiteral("删除选中行"), this);
    m_saveBtn   = new QPushButton(QStringLiteral("保存修改"), this);
    m_revertBtn = new QPushButton(QStringLiteral("撤销"), this);
    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    QPushButton *exportBtn  = new QPushButton(QStringLiteral("导出CSV"), this);

    m_addBtn->setStyleSheet(QLatin1String("font-weight:bold;"));
    m_saveBtn->setStyleSheet(QLatin1String("font-weight:bold;"));

    top->addWidget(m_addBtn);
    top->addWidget(m_delBtn);
    top->addWidget(m_saveBtn);
    top->addWidget(m_revertBtn);
    top->addWidget(refreshBtn);
    top->addStretch(1);

    m_filterColumn = new QComboBox(this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("输入关键字过滤"));
    QPushButton *filterBtn = new QPushButton(QStringLiteral("过滤"), this);
    QPushButton *clearFilterBtn = new QPushButton(QStringLiteral("清除"), this);
    top->addWidget(new QLabel(QStringLiteral("查找："), this));
    top->addWidget(m_filterColumn);
    top->addWidget(m_filterEdit);
    top->addWidget(filterBtn);
    top->addWidget(clearFilterBtn);
    top->addWidget(exportBtn);

    m_view = new QTableView(this);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked
                            | QAbstractItemView::EditKeyPressed
                            | QAbstractItemView::AnyKeyPressed);
    m_view->setAlternatingRowColors(true);
    m_view->setSortingEnabled(true);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setDefaultSectionSize(24);

    m_statusLabel = new QLabel(this);

    layout->addLayout(top);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_statusLabel);

    connect(m_addBtn, &QPushButton::clicked, this, &TableTab::addRow);
    connect(m_delBtn, &QPushButton::clicked, this, &TableTab::deleteRows);
    connect(m_saveBtn, &QPushButton::clicked, this, &TableTab::saveChanges);
    connect(m_revertBtn, &QPushButton::clicked, this, &TableTab::revertChanges);
    connect(refreshBtn, &QPushButton::clicked, this, [this]() { refresh(); });
    connect(exportBtn, &QPushButton::clicked, this, &TableTab::exportCsv);
    connect(filterBtn, &QPushButton::clicked, this, &TableTab::applyFilter);
    connect(clearFilterBtn, &QPushButton::clicked, this, &TableTab::clearFilter);
    connect(m_filterEdit, &QLineEdit::returnPressed, this, &TableTab::applyFilter);

    if (m_readOnly) {
        m_addBtn->setEnabled(false);
        m_delBtn->setEnabled(false);
        m_saveBtn->setEnabled(false);
        m_revertBtn->setEnabled(false);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

void TableTab::populateFilterColumns()
{
    if (!m_model)
        return;
    const QString current = m_filterColumn->currentData().toString();
    m_filterColumn->blockSignals(true);
    m_filterColumn->clear();
    m_filterColumn->addItem(QStringLiteral("（全部列）"), QString());
    QSqlRecord rec = m_model->record();
    int select = 0;
    for (int i = 0; i < rec.count(); ++i) {
        const QString name = rec.fieldName(i);
        m_filterColumn->addItem(name, name);
        if (name == current)
            select = i + 1;
    }
    m_filterColumn->setCurrentIndex(select);
    m_filterColumn->blockSignals(false);
}

QString TableTab::quotedIdentifier(const QString &field) const
{
    QSqlDatabase db = m_model ? m_model->database()
                              : DbManager::instance().database(m_connName, m_dbName);
    const QString drv = db.driverName();
    if (drv == QLatin1String("QODBC"))
        return QLatin1Char('[') + field + QLatin1Char(']');
    if (drv == QLatin1String("QMYSQL") || drv == QLatin1String("QMYSQL3")) {
        QString escaped = field;
        escaped.replace(QLatin1Char('`'), QStringLiteral("``"));
        return QLatin1Char('`') + escaped + QLatin1Char('`');
    }
    return QLatin1Char('"') + field + QLatin1Char('"');
}

void TableTab::addRow()
{
    if (!m_model || m_readOnly)
        return;
    const int row = m_model->rowCount();
    if (!m_model->insertRow(row)) {
        QMessageBox::warning(this, QStringLiteral("操作失败"),
                             QStringLiteral("无法新增行：\n%1")
                             .arg(m_model->lastError().text()));
        return;
    }
    m_view->scrollToBottom();
    m_view->selectRow(row);
    const int editCol = qMin(1, m_model->columnCount() - 1);
    const QModelIndex idx = m_model->index(row, editCol);
    m_view->setFocus();
    m_view->edit(idx);
    updateStatus();
}

void TableTab::deleteRows()
{
    if (!m_model || m_readOnly)
        return;
    const QModelIndexList selected = m_view->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择要删除的行（可按住 Ctrl 多选）。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除选中的 %1 行吗？保存后生效。").arg(selected.size()))
            != QMessageBox::Yes)
        return;

    QList<int> rows;
    foreach (const QModelIndex &idx, selected)
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    foreach (int row, rows)
        m_model->removeRow(row);
    updateStatus();
}

void TableTab::saveChanges()
{
    if (!m_model || m_readOnly)
        return;
    if (!m_model->isDirty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("没有需要保存的更改。"));
        return;
    }
    if (!m_model->submitAll()) {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
            QStringLiteral("保存到数据库失败：\n%1\n\n已撤销本次未成功的修改。")
            .arg(m_model->lastError().text()));
        m_model->revertAll();
        updateStatus();
        return;
    }
    m_model->select(); // 重新查询，取回自增主键等数据库生成的值
    populateFilterColumns();
    updateStatus();
    m_statusLabel->setText(m_statusLabel->text()
                           + QStringLiteral("    （已保存）"));
}

void TableTab::revertChanges()
{
    if (!m_model || m_readOnly)
        return;
    m_model->revertAll();
    updateStatus();
}

void TableTab::refresh()
{
    if (!m_model)
        return;
    m_model->setFilter(QString());
    if (!m_model->select()) {
        QMessageBox::warning(this, QStringLiteral("刷新失败"),
                             m_model->lastError().text());
        return;
    }
    m_filterEdit->clear();
    populateFilterColumns();
    m_view->resizeColumnsToContents();
    updateStatus();
}

void TableTab::applyFilter()
{
    if (!m_model)
        return;
    const QString keyword = m_filterEdit->text().trimmed();
    const QString column = m_filterColumn->currentData().toString();

    if (keyword.isEmpty() || column.isEmpty()) {
        m_model->setFilter(QString());
    } else {
        QString escaped = keyword;
        escaped.replace(QLatin1Char('\''), QLatin1String("''"));
        m_model->setFilter(quotedIdentifier(column)
                           + QStringLiteral(" LIKE '%") + escaped
                           + QStringLiteral("%'"));
    }
    if (!m_model->select()) {
        QMessageBox::warning(this, QStringLiteral("过滤失败"),
                             m_model->lastError().text());
        return;
    }
    updateStatus();
}

void TableTab::clearFilter()
{
    m_filterEdit->clear();
    m_filterColumn->setCurrentIndex(0);
    if (m_model) {
        m_model->setFilter(QString());
        m_model->select();
    }
    updateStatus();
}

void TableTab::exportCsv()
{
    if (!m_model)
        return;
    const QString defaultName = m_tableName + QLatin1String(".csv");
    const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("导出为 CSV"),
                QDir::homePath() + QLatin1Char('/') + defaultName,
                QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：\n%1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true); // 带 BOM，Excel 直接打开不乱码

    QSqlRecord rec = m_model->record();
    QStringList headers;
    for (int c = 0; c < rec.count(); ++c)
        headers << rec.fieldName(c);
    out << headers.join(QLatin1Char(',')) << QLatin1Char('\n');

    for (int r = 0; r < m_model->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < rec.count(); ++c) {
            QString v = m_model->data(m_model->index(r, c)).toString();
            v.replace(QLatin1Char('"'), QLatin1String("\"\""));
            if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
                    || v.contains(QLatin1Char('\n')))
                v = QLatin1Char('"') + v + QLatin1Char('"');
            cells << v;
        }
        out << cells.join(QLatin1Char(',')) << QLatin1Char('\n');
    }
    out.flush();
    file.close();
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 行到：\n%2")
                             .arg(m_model->rowCount()).arg(QDir::toNativeSeparators(path)));
}

void TableTab::updateStatus()
{
    if (!m_statusLabel || !m_model)
        return;
    QString s = QStringLiteral("%1%2 · 共 %3 行")
            .arg(m_readOnly ? QStringLiteral("视图 ") : QString(),
                 m_tableName)
            .arg(m_model->rowCount());
    if (m_model->isDirty())
        s += QStringLiteral(" · 有未保存的更改");
    m_statusLabel->setText(s);
}

bool TableTab::confirmClose()
{
    if (m_model && m_model->isDirty()) {
        const QMessageBox::StandardButton ret = QMessageBox::question(
                    this, QStringLiteral("尚未保存"),
                    QStringLiteral("表“%1”有未保存的修改，是否先保存？")
                        .arg(m_tableName),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel)
            return false;
        if (ret == QMessageBox::Save) {
            if (!m_model->submitAll()) {
                QMessageBox::critical(this, QStringLiteral("保存失败"),
                                      m_model->lastError().text());
                return false;
            }
        }
    }
    return true;
}
