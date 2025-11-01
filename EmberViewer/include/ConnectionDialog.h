#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QString>

class ConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget *parent = nullptr);
    ~ConnectionDialog();

    void setConnectionName(const QString &name);
    void setHost(const QString &host);
    void setPort(int port);

    QString getConnectionName() const;
    QString getHost() const;
    int getPort() const;

private:
    void setupUi();

    QLineEdit *m_nameEdit;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
};

#endif
