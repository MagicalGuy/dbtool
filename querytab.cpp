#include "querytab.h"
#include "dbmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QTableView>
#include <QSqlQueryModel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QTextCursor>

QueryTab::QueryTab(const QString &connName, const QString &dbName, QWidget *parent)
    : QWidget(parent),
      m_connName(connName),
      m_dbName(dbName),
      m_editor(0),
      m_view(0),
      m_model(0),
      m_msgLabel(0)
{
    buildUi();
}

QueryTab::~QueryTab()
{
}

void QueryTab::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout *bar = new QHBoxLayout();
    QPushButton *runBtn = new QPushButton(QStringLiteral("运行 (F5)"), this);
    runBtn->setStyleSheet(QLatin1String("font-weight:bold;"));
    QPushButton *clearBtn = new QPushButton(QStringLiteral("清空"), this);
    QPushButton *exportBtn = new QPushButton(QStringLiteral("导出结果CSV"), this);
    QLabel *hint = new QLabel(QStringLiteral("多条语句用分号分隔；仅执行选中内容（若有）"), this);
    hint->setStyleSheet(QLatin1String("color:#666;"));

    bar->addWidget(runBtn);
    bar->addWidget(clearBtn);
    bar->addStretch(1);
    bar->addWidget(hint);
    bar->addWidget(exportBtn);

    m_editor = new QPlainTextEdit(this);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    m_editor->setFont(mono);
    m_editor->setPlaceholderText(QStringLiteral(
        "-- 在此输入 SQL，例如：\n"
        "SELECT * FROM employees;\n"
        "-- 也可以一次执行多条语句（建表 / 插入 / 查询）"));

    m_view = new QTableView(this);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setAlternatingRowColors(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_model = new QSqlQueryModel(this);
    m_view->setModel(m_model);

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_editor);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 220 << 260);

    m_msgLabel = new QLabel(QStringLiteral("就绪"), this);

    layout->addLayout(bar);
    layout->addWidget(splitter, 1);
    layout->addWidget(m_msgLabel);

    connect(runBtn, &QPushButton::clicked, this, &QueryTab::run);
    connect(clearBtn, &QPushButton::clicked, this, &QueryTab::clearEditor);
    connect(exportBtn, &QPushButton::clicked, this, &QueryTab::exportCsv);

    QShortcut *f5 = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(f5, &QShortcut::activated, this, &QueryTab::run);
    QShortcut *ctrlEnter = new QShortcut(
                QKeySequence(Qt::CTRL + Qt::Key_Return), this);
    connect(ctrlEnter, &QShortcut::activated, this, &QueryTab::run);
    QShortcut *ctrlEnterNumpad = new QShortcut(
                QKeySequence(Qt::CTRL + Qt::Key_Enter), this);
    connect(ctrlEnterNumpad, &QShortcut::activated, this, &QueryTab::run);
}

void QueryTab::setSql(const QString &sql)
{
    m_editor->setPlainText(sql);
}

void QueryTab::clearEditor()
{
    m_editor->clear();
}

QString QueryTab::firstKeyword(const QString &stmt)
{
    int i = 0;
    const int n = stmt.size();
    while (i < n) {
        const QChar c = stmt.at(i);
        if (c.isSpace()) {
            ++i;
        } else if (c == QLatin1Char('-') && i + 1 < n
                   && stmt.at(i + 1) == QLatin1Char('-')) {
            i += 2;
            while (i < n && stmt.at(i) != QLatin1Char('\n'))
                ++i;
        } else if (c == QLatin1Char('/') && i + 1 < n
                   && stmt.at(i + 1) == QLatin1Char('*')) {
            i += 2;
            while (i + 1 < n
                   && !(stmt.at(i) == QLatin1Char('*')
                        && stmt.at(i + 1) == QLatin1Char('/')))
                ++i;
            i += 2;
        } else {
            break;
        }
    }
    QString word;
    while (i < n) {
        const QChar c = stmt.at(i);
        if (c.isLetterOrNumber() || c == QLatin1Char('_')) {
            word.append(c);
            ++i;
        } else {
            break;
        }
    }
    return word.toLower();
}

bool QueryTab::returnsResultSet(const QString &stmt)
{
    const QString kw = firstKeyword(stmt);
    return kw == QLatin1String("select")
            || kw == QLatin1String("with")
            || kw == QLatin1String("pragma")
            || kw == QLatin1String("explain")
            || kw == QLatin1String("show")
            || kw == QLatin1String("describe")
            || kw == QLatin1String("desc")
            || kw == QLatin1String("values");
}

QStringList QueryTab::splitStatements(const QString &sql)
{
    enum State { Normal, InSingle, InDouble, InBacktick,
                 InBracket, InLineComment, InBlockComment };
    State state = Normal;
    QStringList result;
    QString cur;

    for (int i = 0; i < sql.size(); ++i) {
        const QChar c = sql.at(i);
        const QChar nxt = (i + 1 < sql.size()) ? sql.at(i + 1) : QChar();

        switch (state) {
        case Normal:
            if (c == QLatin1Char('-') && nxt == QLatin1Char('-')) {
                state = InLineComment;
                cur.append(c);
                cur.append(nxt);
                ++i;
            } else if (c == QLatin1Char('/') && nxt == QLatin1Char('*')) {
                state = InBlockComment;
                cur.append(c);
                cur.append(nxt);
                ++i;
            } else if (c == QLatin1Char('\'')) {
                state = InSingle;
                cur.append(c);
            } else if (c == QLatin1Char('"')) {
                state = InDouble;
                cur.append(c);
            } else if (c == QLatin1Char('`')) {
                state = InBacktick;
                cur.append(c);
            } else if (c == QLatin1Char('[')) {
                state = InBracket;
                cur.append(c);
            } else if (c == QLatin1Char(';')) {
                if (!cur.trimmed().isEmpty())
                    result.append(cur.trimmed());
                cur.clear();
            } else {
                cur.append(c);
            }
            break;
        case InLineComment:
            cur.append(c);
            if (c == QLatin1Char('\n'))
                state = Normal;
            break;
        case InBlockComment:
            cur.append(c);
            if (c == QLatin1Char('*') && nxt == QLatin1Char('/')) {
                cur.append(nxt);
                ++i;
                state = Normal;
            }
            break;
        case InSingle:
            cur.append(c);
            if (c == QLatin1Char('\'')) {
                if (nxt == QLatin1Char('\'')) { // 转义的单引号 ''
                    cur.append(nxt);
                    ++i;
                } else {
                    state = Normal;
                }
            }
            break;
        case InDouble:
            cur.append(c);
            if (c == QLatin1Char('"')) {
                if (nxt == QLatin1Char('"')) {
                    cur.append(nxt);
                    ++i;
                } else {
                    state = Normal;
                }
            }
            break;
        case InBacktick:
            cur.append(c);
            if (c == QLatin1Char('`'))
                state = Normal;
            break;
        case InBracket:
            cur.append(c);
            if (c == QLatin1Char(']'))
                state = Normal;
            break;
        }
    }
    if (!cur.trimmed().isEmpty())
        result.append(cur.trimmed());
    return result;
}

void QueryTab::run()
{
    QString sql = m_editor->toPlainText();
    const QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        sql = cursor.selectedText();
        sql.replace(QChar(0x2029), QLatin1Char('\n')); // 选择文本的换行符
    }
    sql = sql.trimmed();
    if (sql.isEmpty()) {
        m_msgLabel->setStyleSheet(QLatin1String("color:#b00;"));
        m_msgLabel->setText(QStringLiteral("请输入或选中要执行的 SQL。"));
        return;
    }

    QSqlDatabase db = DbManager::instance().database(m_connName, m_dbName);
    if (!db.isOpen()) {
        m_msgLabel->setStyleSheet(QLatin1String("color:#b00;"));
        m_msgLabel->setText(QStringLiteral("数据库连接未打开，请先在左侧连接。"));
        return;
    }

    const QStringList statements = splitStatements(sql);
    if (statements.isEmpty()) {
        m_msgLabel->setStyleSheet(QLatin1String("color:#b00;"));
        m_msgLabel->setText(QStringLiteral("没有可执行的 SQL 语句。"));
        return;
    }

    QElapsedTimer timer;
    timer.start();

    int executed = 0;
    int affected = 0;
    QString lastResultSetSql;
    QString errorText;
    int errorIndex = -1;

    for (int i = 0; i < statements.size(); ++i) {
        const QString &stmt = statements.at(i);
        QSqlQuery query(db);
        if (!query.exec(stmt)) {
            errorIndex = i + 1;
            errorText = query.lastError().text().trimmed();
            break;
        }
        ++executed;
        if (returnsResultSet(stmt)) {
            lastResultSetSql = stmt;
        } else {
            const int n = query.numRowsAffected();
            if (n >= 0)
                affected += n;
        }
    }

    const qint64 elapsed = timer.elapsed();

    if (errorIndex >= 0) {
        m_model->clear();
        m_view->setModel(m_model);
        m_msgLabel->setStyleSheet(QLatin1String("color:#b00;"));
        m_msgLabel->setText(QStringLiteral("第 %1 / %2 条语句执行失败：%3")
                            .arg(errorIndex).arg(statements.size())
                            .arg(errorText));
        return;
    }

    if (!lastResultSetSql.isEmpty()) {
        m_model->setQuery(lastResultSetSql, db);
        m_view->setModel(m_model);
        if (m_model->lastError().isValid()) {
            m_msgLabel->setStyleSheet(QLatin1String("color:#b00;"));
            m_msgLabel->setText(QStringLiteral("查询失败：%1")
                                .arg(m_model->lastError().text().trimmed()));
            return;
        }
        m_view->resizeColumnsToContents();
        m_msgLabel->setStyleSheet(QLatin1String("color:#060;"));
        m_msgLabel->setText(QStringLiteral(
            "执行成功：%1 条语句，返回 %2 行，耗时 %3 ms")
            .arg(executed).arg(m_model->rowCount()).arg(elapsed));
    } else {
        m_model->clear();
        m_view->setModel(m_model);
        m_msgLabel->setStyleSheet(QLatin1String("color:#060;"));
        m_msgLabel->setText(QStringLiteral(
            "执行成功：%1 条语句，影响 %2 行，耗时 %3 ms")
            .arg(executed).arg(affected).arg(elapsed));
    }
}

void QueryTab::exportCsv()
{
    if (m_model->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("当前没有可导出的查询结果。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("导出结果为 CSV"),
                QDir::homePath() + QLatin1String("/query_result.csv"),
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
    out.setGenerateByteOrderMark(true);

    QStringList headers;
    for (int c = 0; c < m_model->columnCount(); ++c)
        headers << m_model->headerData(c, Qt::Horizontal).toString();
    out << headers.join(QLatin1Char(',')) << QLatin1Char('\n');

    for (int r = 0; r < m_model->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_model->columnCount(); ++c) {
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
                             .arg(m_model->rowCount())
                             .arg(QDir::toNativeSeparators(path)));
}
