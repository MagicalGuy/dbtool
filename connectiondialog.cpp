#include "connectiondialog.h"
#include "dbmanager.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

// 在指定 SQLite 文件中创建一套示例库（部门 + 员工），便于立即体验增删改查
static bool buildSampleSqlite(const QString &filePath, QString *error)
{
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"),
                                                     QLatin1String("dbtool_sample_builder"));
        db.setDatabaseName(filePath);
        if (!db.open()) {
            if (error) *error = db.lastError().text().trimmed();
            QSqlDatabase::removeDatabase(QLatin1String("dbtool_sample_builder"));
            return false;
        }

        QSqlQuery q(db);
        const QStringList ddl = QStringList()
            << QStringLiteral("CREATE TABLE departments ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "name TEXT NOT NULL,"
                              "location TEXT)")
            << QStringLiteral("CREATE TABLE employees ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "name TEXT NOT NULL,"
                              "gender TEXT,"
                              "birth_date TEXT,"
                              "department_id INTEGER,"
                              "position TEXT,"
                              "salary REAL,"
                              "hire_date TEXT)")
            << QStringLiteral("INSERT INTO departments(name, location) VALUES "
                              "('研发部','南京'),('市场部','上海'),"
                              "('财务部','北京'),('人力资源部','广州')")
            << QStringLiteral("INSERT INTO employees"
                              "(name,gender,birth_date,department_id,position,salary,hire_date) VALUES "
                              "('张伟','男','1988-03-12',1,'研发经理',18500.0,'2015-06-01'),"
                              "('李娜','女','1992-07-25',1,'软件工程师',13200.0,'2017-09-10'),"
                              "('王强','男','1995-01-30',1,'软件工程师',11800.0,'2019-04-15'),"
                              "('刘洋','男','1990-11-05',2,'市场专员',9800.0,'2018-02-20'),"
                              "('陈静','女','1993-09-18',2,'市场经理',15600.0,'2016-11-03'),"
                              "('杨帆','男','1991-05-22',3,'会计',10500.0,'2017-07-08'),"
                              "('赵磊','男','1987-12-01',3,'财务主管',16800.0,'2014-03-18'),"
                              "('孙丽','女','1996-04-14',4,'HR专员',9200.0,'2020-08-01'),"
                              "('周杰','男','1994-08-09',1,'测试工程师',11000.0,'2019-12-16'),"
                              "('吴敏','女','1989-02-27',4,'HR经理',14200.0,'2015-10-12')");

        foreach (const QString &sql, ddl) {
            if (!q.exec(sql)) {
                if (error) *error = q.lastError().text().trimmed();
                db.close();
                QSqlDatabase::removeDatabase(QLatin1String("dbtool_sample_builder"));
                return false;
            }
        }
    }
    QSqlDatabase::removeDatabase(QLatin1String("dbtool_sample_builder"));
    return true;
}

ConnectionDialog::ConnectionDialog(const QStringList &existingNames,
                                   const QString &originalName,
                                   QWidget *parent)
    : QDialog(parent),
      m_existingNames(existingNames),
      m_originalName(originalName)
{
    buildUi();
    updateFieldState();
}

void ConnectionDialog::buildUi()
{
    setWindowTitle(m_originalName.isEmpty()
                   ? QStringLiteral("新建连接")
                   : QStringLiteral("编辑连接"));
    setMinimumWidth(460);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("给这个连接起个名字，如：本机测试库"));

    m_driverCombo = new QComboBox(this);
    m_driverCombo->addItems(QSqlDatabase::drivers());
    // 默认优先选择开箱即用的 SQLite
    const int sqliteIdx = m_driverCombo->findText(QLatin1String("QSQLITE"));
    if (sqliteIdx >= 0)
        m_driverCombo->setCurrentIndex(sqliteIdx);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(QStringLiteral("如 127.0.0.1"));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(0, 65535);
    m_portSpin->setSpecialValueText(QStringLiteral("默认"));

    m_userEdit = new QLineEdit(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_showPwdCheck = new QCheckBox(QStringLiteral("显示"), this);

    m_dbEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    m_sampleBtn = new QPushButton(QStringLiteral("创建示例库..."), this);

    QHBoxLayout *pwdRow = new QHBoxLayout();
    pwdRow->setContentsMargins(0, 0, 0, 0);
    pwdRow->addWidget(m_passwordEdit);
    pwdRow->addWidget(m_showPwdCheck);

    QHBoxLayout *dbRow = new QHBoxLayout();
    dbRow->setContentsMargins(0, 0, 0, 0);
    dbRow->addWidget(m_dbEdit);
    dbRow->addWidget(m_browseBtn);
    dbRow->addWidget(m_sampleBtn);

    QFormLayout *form = new QFormLayout();
    form->addRow(QStringLiteral("连接名称："), m_nameEdit);
    form->addRow(QStringLiteral("数据库类型："), m_driverCombo);
    form->addRow(QStringLiteral("主机地址："), m_hostEdit);
    form->addRow(QStringLiteral("端口："), m_portSpin);
    form->addRow(QStringLiteral("用户名："), m_userEdit);
    form->addRow(QStringLiteral("密码："), pwdRow);
    form->addRow(QStringLiteral("数据库："), dbRow);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *testBtn = buttonBox->addButton(
                QStringLiteral("测试连接"), QDialogButtonBox::ActionRole);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QLabel *tip = new QLabel(
                QStringLiteral("提示：选择 QSQLITE 无需安装任何数据库服务器，"
                               "最适合快速上手。"), this);
    tip->setWordWrap(true);
    tip->setStyleSheet(QLatin1String("color:#666;"));
    mainLayout->addWidget(tip);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttonBox);

    connect(m_driverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::updateFieldState);
    connect(m_showPwdCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_browseBtn, &QPushButton::clicked, this, &ConnectionDialog::browseFile);
    connect(m_sampleBtn, &QPushButton::clicked,
            this, &ConnectionDialog::createSampleDatabase);
    connect(testBtn, &QPushButton::clicked, this, &ConnectionDialog::testConnection);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConnectionDialog::updateFieldState()
{
    const QString drv = m_driverCombo->currentText();
    const bool isSqlite = (drv == QLatin1String("QSQLITE"));
    const bool isOdbc   = (drv == QLatin1String("QODBC"));

    m_hostEdit->setEnabled(!isSqlite && !isOdbc);
    m_portSpin->setEnabled(!isSqlite && !isOdbc);
    m_userEdit->setEnabled(!isSqlite);
    m_passwordEdit->setEnabled(!isSqlite);
    m_showPwdCheck->setEnabled(!isSqlite);

    m_browseBtn->setVisible(isSqlite);
    m_sampleBtn->setVisible(isSqlite);

    if (isSqlite) {
        m_portSpin->setValue(0); // SQLite 不使用端口
        m_dbEdit->setPlaceholderText(QStringLiteral("选择或新建一个 .db / .sqlite 文件"));
    } else if (isOdbc) {
        m_portSpin->setValue(0);
        m_dbEdit->setPlaceholderText(QStringLiteral("DSN 名称，或 DRIVER={...};SERVER=... 连接串"));
    } else {
        // 网络数据库：端口为“默认(0)”时自动填入该库的默认端口
        const bool isMysql = (drv == QLatin1String("QMYSQL")
                              || drv == QLatin1String("QMYSQL3"));
        if (m_portSpin->value() == 0) {
            ConnectionInfo tmp;
            tmp.driver = drv;
            m_portSpin->setValue(tmp.defaultPort());
        }
        m_dbEdit->setPlaceholderText(isMysql
            ? QStringLiteral("可留空：留空则连接后列出所有数据库；填写则默认打开该库")
            : QStringLiteral("数据库名称"));
    }
}

void ConnectionDialog::browseFile()
{
    const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("选择 SQLite 数据库文件"),
                QDir::homePath() + QLatin1String("/demo.db"),
                QStringLiteral("SQLite 数据库 (*.db *.sqlite *.sqlite3);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_dbEdit->setText(QDir::toNativeSeparators(path));
}

void ConnectionDialog::createSampleDatabase()
{
    QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("创建示例 SQLite 数据库"),
                QDir::homePath() + QLatin1String("/sample.db"),
                QStringLiteral("SQLite 数据库 (*.db *.sqlite *.sqlite3)"));
    if (path.isEmpty())
        return;

    if (QFile::exists(path)) {
        const QFileInfo fi(path);
        if (fi.size() > 0) {
            const auto ret = QMessageBox::question(
                        this, QStringLiteral("文件已存在"),
                        QStringLiteral("文件 %1 已存在且非空，是否删除并重建为示例库？")
                                   .arg(QDir::toNativeSeparators(path)),
                        QMessageBox::Yes | QMessageBox::No);
            if (ret != QMessageBox::Yes)
                return;
        }
        QFile::remove(path);
    }

    QString err;
    if (!buildSampleSqlite(path, &err)) {
        QMessageBox::critical(this, QStringLiteral("创建失败"),
                              QStringLiteral("无法创建示例数据库：\n%1").arg(err));
        return;
    }

    m_driverCombo->setCurrentText(QLatin1String("QSQLITE"));
    m_dbEdit->setText(QDir::toNativeSeparators(path));
    if (m_nameEdit->text().trimmed().isEmpty())
        m_nameEdit->setText(QStringLiteral("示例SQLite"));
    QMessageBox::information(this, QStringLiteral("创建成功"),
                             QStringLiteral("示例数据库已创建，包含 departments、employees 两张表。"));
}

ConnectionInfo ConnectionDialog::collect() const
{
    ConnectionInfo info;
    info.name     = m_nameEdit->text().trimmed();
    info.driver   = m_driverCombo->currentText();
    info.host     = m_hostEdit->text().trimmed();
    info.port     = m_portSpin->value();
    info.userName = m_userEdit->text().trimmed();
    info.password = m_passwordEdit->text();
    info.database = m_dbEdit->text().trimmed();
    return info;
}

bool ConnectionDialog::validate(ConnectionInfo *info, QString *error) const
{
    const ConnectionInfo data = collect();
    if (info) *info = data;

    if (data.name.isEmpty()) {
        if (error) *error = QStringLiteral("请填写连接名称");
        return false;
    }
    if (m_existingNames.contains(data.name)
            && data.name != m_originalName) {
        if (error) *error = QStringLiteral("连接名称“%1”已存在，请换一个").arg(data.name);
        return false;
    }
    if (data.driver == QLatin1String("QSQLITE")) {
        if (data.database.isEmpty()) {
            if (error) *error = QStringLiteral("请选择 SQLite 数据库文件");
            return false;
        }
    } else if (data.driver == QLatin1String("QODBC")) {
        if (data.database.isEmpty()) {
            if (error) *error = QStringLiteral("请填写 ODBC 的 DSN 名称或连接串");
            return false;
        }
    } else {
        if (data.host.isEmpty()) {
            if (error) *error = QStringLiteral("请填写主机地址");
            return false;
        }
        // MySQL 允许不指定数据库（连接到实例后列出所有库）；
        // PostgreSQL 等其他网络库仍需指定要连接的数据库。
        const bool isMysql = data.driver == QLatin1String("QMYSQL")
                || data.driver == QLatin1String("QMYSQL3");
        if (!isMysql && data.database.isEmpty()) {
            if (error) *error = QStringLiteral("请填写数据库名称");
            return false;
        }
    }
    return true;
}

void ConnectionDialog::testConnection()
{
    ConnectionInfo info;
    QString err;
    if (!validate(&info, &err)) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"), err);
        return;
    }

    // SQLite 选择了尚不存在的文件时，Qt 会自动创建一个空库，这是正常现象
    QString testErr;
    if (DbManager::testConnection(info, &testErr)) {
        QMessageBox::information(this, QStringLiteral("连接成功"),
                                 QStringLiteral("可以正常连接到“%1”。").arg(info.name));
    } else {
        QMessageBox::critical(this, QStringLiteral("连接失败"),
                              QStringLiteral("连接失败：\n%1").arg(testErr));
    }
}

void ConnectionDialog::accept()
{
    ConnectionInfo info;
    QString err;
    if (!validate(&info, &err)) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), err);
        return;
    }
    QDialog::accept();
}

void ConnectionDialog::setConnection(const ConnectionInfo &info)
{
    m_nameEdit->setText(info.name);
    {
        const int idx = m_driverCombo->findText(info.driver);
        if (idx >= 0)
            m_driverCombo->setCurrentIndex(idx);
    }
    m_hostEdit->setText(info.host);
    m_portSpin->setValue(info.port);
    m_userEdit->setText(info.userName);
    m_passwordEdit->setText(info.password);
    m_dbEdit->setText(info.database);
    updateFieldState();
}

ConnectionInfo ConnectionDialog::connectionInfo() const
{
    return collect();
}
