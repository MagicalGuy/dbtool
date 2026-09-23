#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QStringList>

#include "connection.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

/**
 * @brief 新建 / 编辑数据库连接的对话框
 */
class ConnectionDialog : public QDialog
{
    Q_OBJECT
public:
    // existingNames 用于重名校验；编辑时把原名放入 originalName
    explicit ConnectionDialog(const QStringList &existingNames,
                              const QString &originalName = QString(),
                              QWidget *parent = 0);

    void setConnection(const ConnectionInfo &info);
    ConnectionInfo connectionInfo() const;

public slots:
    void accept() override;

private slots:
    void updateFieldState();
    void browseFile();
    void createSampleDatabase();
    void testConnection();

private:
    void buildUi();
    ConnectionInfo collect() const;
    bool validate(ConnectionInfo *info, QString *error) const;

    QStringList m_existingNames;
    QString m_originalName;

    QLineEdit *m_nameEdit;
    QComboBox *m_driverCombo;
    QLineEdit *m_hostEdit;
    QSpinBox  *m_portSpin;
    QLineEdit *m_userEdit;
    QLineEdit *m_passwordEdit;
    QCheckBox *m_showPwdCheck;
    QLineEdit *m_dbEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_sampleBtn;
};

#endif // CONNECTIONDIALOG_H
