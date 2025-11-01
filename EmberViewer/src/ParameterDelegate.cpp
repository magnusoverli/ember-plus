







#include "ParameterDelegate.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDebug>
#include <QDebug>


namespace ParameterType {
    enum {
        None = 0,
        Integer = 1,
        Real = 2,
        String = 3,
        Boolean = 4,
        Trigger = 5,
        Enum = 6,
        Octets = 7
    };
}

ParameterDelegate::ParameterDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* ParameterDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);

    
    if (index.column() != 2) {
        return nullptr;
    }

    
    QModelIndex metadataIndex = index.sibling(index.row(), 0);
    int type = metadataIndex.data(TypeRole).toInt();
    int access = metadataIndex.data(AccessRole).toInt();
    bool isOnline = metadataIndex.data(Qt::UserRole + 8).toBool();

    
    if (!isOnline) {
        return nullptr;
    }

    
    if (access != 2 && access != 3) {
        return nullptr; 
    }
    
    
    if (type == ParameterType::None) {
        QString value = index.model()->data(index, Qt::DisplayRole).toString().toLower();
        
        if (value == "true" || value == "false") {
            type = ParameterType::Boolean;
        } else {
            bool isInt;
            value.toInt(&isInt);
            if (isInt) {
                type = ParameterType::Integer;
            } else {
                bool isDouble;
                value.toDouble(&isDouble);
                if (isDouble) {
                    type = ParameterType::Real;
                } else {
                    type = ParameterType::String;
                }
            }
        }
    }

    
    switch (type) {
        case ParameterType::Integer: {
            QSpinBox *editor = new QSpinBox(parent);
            
            
            if (metadataIndex.data(MinimumRole).isValid()) {
                editor->setMinimum(metadataIndex.data(MinimumRole).toInt());
            } else {
                editor->setMinimum(INT_MIN);
            }
            
            if (metadataIndex.data(MaximumRole).isValid()) {
                editor->setMaximum(metadataIndex.data(MaximumRole).toInt());
            } else {
                editor->setMaximum(INT_MAX);
            }
            
            editor->setFrame(true);
            return editor;
        }

        case ParameterType::Real: {
            QDoubleSpinBox *editor = new QDoubleSpinBox(parent);
            editor->setDecimals(3);
            
            
            if (metadataIndex.data(MinimumRole).isValid()) {
                editor->setMinimum(metadataIndex.data(MinimumRole).toDouble());
            } else {
                editor->setMinimum(-1000000.0);
            }
            
            if (metadataIndex.data(MaximumRole).isValid()) {
                editor->setMaximum(metadataIndex.data(MaximumRole).toDouble());
            } else {
                editor->setMaximum(1000000.0);
            }
            
            editor->setFrame(true);
            return editor;
        }

        case ParameterType::String: {
            QLineEdit *editor = new QLineEdit(parent);
            editor->setFrame(true);
            return editor;
        }

        case ParameterType::Boolean: {
            
            
            QComboBox *editor = new QComboBox(parent);
            editor->addItem("false");
            editor->addItem("true");
            editor->setFrame(true);
            return editor;
        }

        case ParameterType::Enum: {
            QComboBox *editor = new QComboBox(parent);
            
            
            QStringList options = metadataIndex.data(EnumOptionsRole).toStringList();
            if (!options.isEmpty()) {
                editor->addItems(options);
            } else {
                editor->addItem("(no options available)");
            }
            
            editor->setFrame(true);
            return editor;
        }

        case ParameterType::Trigger: {
            
            
            return nullptr;
        }

        default:
            return nullptr;
    }
}

void ParameterDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::DisplayRole).toString();
    QModelIndex metadataIndex = index.sibling(index.row(), 0);
    int type = metadataIndex.data(TypeRole).toInt();

    switch (type) {
        case ParameterType::Integer: {
            QSpinBox *spinBox = qobject_cast<QSpinBox*>(editor);
            if (spinBox) {
                spinBox->setValue(value.toInt());
            }
            break;
        }

        case ParameterType::Real: {
            QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox*>(editor);
            if (spinBox) {
                spinBox->setValue(value.toDouble());
            }
            break;
        }

        case ParameterType::String: {
            QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
            if (lineEdit) {
                lineEdit->setText(value);
            }
            break;
        }

        case ParameterType::Boolean: {
            QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
            if (comboBox) {
                
                QString lower = value.toLower();
                if (lower == "true" || lower == "1") {
                    comboBox->setCurrentIndex(1);
                } else {
                    comboBox->setCurrentIndex(0);
                }
            }
            break;
        }

        case ParameterType::Enum: {
            QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
            if (comboBox) {
                
                int idx = comboBox->findText(value);
                if (idx >= 0) {
                    comboBox->setCurrentIndex(idx);
                } else {
                    
                    bool ok;
                    int numIdx = value.toInt(&ok);
                    if (ok && numIdx >= 0 && numIdx < comboBox->count()) {
                        comboBox->setCurrentIndex(numIdx);
                    }
                }
            }
            break;
        }
    }
}

void ParameterDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QModelIndex metadataIndex = index.sibling(index.row(), 0);
    int type = metadataIndex.data(TypeRole).toInt();
    QString newValue;

    switch (type) {
        case ParameterType::Integer: {
            QSpinBox *spinBox = qobject_cast<QSpinBox*>(editor);
            if (spinBox) {
                newValue = QString::number(spinBox->value());
            }
            break;
        }

        case ParameterType::Real: {
            QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox*>(editor);
            if (spinBox) {
                newValue = QString::number(spinBox->value());
            }
            break;
        }

        case ParameterType::String: {
            QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
            if (lineEdit) {
                newValue = lineEdit->text();
            }
            break;
        }

        case ParameterType::Boolean: {
            QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
            if (comboBox) {
                newValue = comboBox->currentText(); 
            }
            break;
        }

        case ParameterType::Enum: {
            QComboBox *comboBox = qobject_cast<QComboBox*>(editor);
            if (comboBox) {
                
                QList<QVariant> enumValues = metadataIndex.data(EnumValuesRole).toList();
                int currentIdx = comboBox->currentIndex();
                
                if (!enumValues.isEmpty() && currentIdx >= 0 && currentIdx < enumValues.size()) {
                    newValue = QString::number(enumValues[currentIdx].toInt());
                } else {
                    
                    newValue = QString::number(currentIdx);
                }
            }
            break;
        }

        default:
            return;
    }

    if (!newValue.isEmpty()) {
        
        model->setData(index, newValue, Qt::DisplayRole);

        
        QString path = metadataIndex.data(Qt::UserRole).toString(); 
        emit valueChanged(path, newValue);
    }
}

void ParameterDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

