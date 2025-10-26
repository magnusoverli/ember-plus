/*
    EmberViewer - Function invocation dialog
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "FunctionInvocationDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>

FunctionInvocationDialog::FunctionInvocationDialog(const QString &functionName,
                                                 const QString &description,
                                                 const QStringList &argNames,
                                                 const QList<int> &argTypes,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_argTypes(argTypes)
{
    setWindowTitle(QString("Invoke Function: %1").arg(functionName));
    setMinimumWidth(400);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    if (!description.isEmpty()) {
        QLabel *descLabel = new QLabel(description);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet("QLabel { color: #666; font-style: italic; padding: 5px; }");
        mainLayout->addWidget(descLabel);
    }
    
    if (argNames.isEmpty()) {
        QLabel *noArgsLabel = new QLabel("This function has no arguments.");
        noArgsLabel->setStyleSheet("QLabel { padding: 10px; }");
        mainLayout->addWidget(noArgsLabel);
    } else {
        QLabel *argsLabel = new QLabel("<b>Arguments:</b>");
        mainLayout->addWidget(argsLabel);
        
        QFormLayout *formLayout = new QFormLayout();
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        
        for (int i = 0; i < argNames.size() && i < argTypes.size(); i++) {
            QString argName = argNames[i];
            int argType = argTypes[i];
            
            QWidget *inputWidget = nullptr;
            
            switch (argType) {
                case 1: {
                    QSpinBox *spinBox = new QSpinBox();
                    spinBox->setRange(INT_MIN, INT_MAX);
                    spinBox->setValue(0);
                    inputWidget = spinBox;
                    break;
                }
                case 2: {
                    QDoubleSpinBox *doubleSpinBox = new QDoubleSpinBox();
                    doubleSpinBox->setRange(-1e9, 1e9);
                    doubleSpinBox->setDecimals(6);
                    doubleSpinBox->setValue(0.0);
                    inputWidget = doubleSpinBox;
                    break;
                }
                case 3: {
                    QLineEdit *lineEdit = new QLineEdit();
                    inputWidget = lineEdit;
                    break;
                }
                case 4: {
                    QCheckBox *checkBox = new QCheckBox();
                    inputWidget = checkBox;
                    break;
                }
                default: {
                    QLineEdit *lineEdit = new QLineEdit();
                    lineEdit->setPlaceholderText("(Unsupported type)");
                    inputWidget = lineEdit;
                    break;
                }
            }
            
            m_inputWidgets.append(inputWidget);
            formLayout->addRow(argName + ":", inputWidget);
        }
        
        mainLayout->addLayout(formLayout);
    }
    
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

QList<QVariant> FunctionInvocationDialog::getArguments() const
{
    QList<QVariant> arguments;
    
    for (int i = 0; i < m_inputWidgets.size(); i++) {
        QWidget *widget = m_inputWidgets[i];
        int type = m_argTypes[i];
        
        switch (type) {
            case 1: {
                QSpinBox *spinBox = qobject_cast<QSpinBox*>(widget);
                if (spinBox) {
                    arguments.append(spinBox->value());
                }
                break;
            }
            case 2: {
                QDoubleSpinBox *doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget);
                if (doubleSpinBox) {
                    arguments.append(doubleSpinBox->value());
                }
                break;
            }
            case 3: {
                QLineEdit *lineEdit = qobject_cast<QLineEdit*>(widget);
                if (lineEdit) {
                    arguments.append(lineEdit->text());
                }
                break;
            }
            case 4: {
                QCheckBox *checkBox = qobject_cast<QCheckBox*>(widget);
                if (checkBox) {
                    arguments.append(checkBox->isChecked());
                }
                break;
            }
            default:
                arguments.append(QVariant());
                break;
        }
    }
    
    return arguments;
}
