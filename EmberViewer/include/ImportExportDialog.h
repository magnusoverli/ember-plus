/*
    EmberViewer - Import/Export Dialog
    
    Dialog for importing connections with merge/replace options.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef IMPORTEXPORTDIALOG_H
#define IMPORTEXPORTDIALOG_H

#include <QDialog>
#include <QRadioButton>

class ImportExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportExportDialog(QWidget *parent = nullptr);
    ~ImportExportDialog();

    bool shouldMerge() const;  // true = merge, false = replace

private:
    void setupUi();

    QRadioButton *m_mergeRadio;
    QRadioButton *m_replaceRadio;
};

#endif // IMPORTEXPORTDIALOG_H
