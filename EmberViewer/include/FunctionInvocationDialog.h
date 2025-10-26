/*
    EmberViewer - Function invocation dialog
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef FUNCTIONINVOCATIONDIALOG_H
#define FUNCTIONINVOCATIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVariant>
#include <QStringList>
#include <QList>

class FunctionInvocationDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit FunctionInvocationDialog(const QString &functionName,
                                     const QString &description,
                                     const QStringList &argNames,
                                     const QList<int> &argTypes,
                                     QWidget *parent = nullptr);
    
    QList<QVariant> getArguments() const;
    
private:
    QList<int> m_argTypes;
    QList<QWidget*> m_inputWidgets;
    QDialogButtonBox *m_buttonBox;
};

#endif // FUNCTIONINVOCATIONDIALOG_H
