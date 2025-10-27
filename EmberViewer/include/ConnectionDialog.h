/*
    EmberViewer - Connection Dialog
    
    Dialog for adding/editing saved connections.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

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

    // Set initial values (for editing)
    void setConnectionName(const QString &name);
    void setHost(const QString &host);
    void setPort(int port);

    // Get values
    QString getConnectionName() const;
    QString getHost() const;
    int getPort() const;

private:
    void setupUi();

    QLineEdit *m_nameEdit;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
};

#endif // CONNECTIONDIALOG_H
