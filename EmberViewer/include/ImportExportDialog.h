









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

    bool shouldMerge() const;  

private:
    void setupUi();

    QRadioButton *m_mergeRadio;
    QRadioButton *m_replaceRadio;
};

#endif 
