







#include "ImportExportDialog.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLabel>

ImportExportDialog::ImportExportDialog(QWidget *parent)
    : QDialog(parent)
    , m_mergeRadio(nullptr)
    , m_replaceRadio(nullptr)
{
    setupUi();
    setWindowTitle("Import Connections");
    resize(350, 150);
}

ImportExportDialog::~ImportExportDialog()
{
}

void ImportExportDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *label = new QLabel("How would you like to import the connections?", this);
    mainLayout->addWidget(label);

    QGroupBox *groupBox = new QGroupBox("Import Mode", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

    m_mergeRadio = new QRadioButton("Merge with existing connections", groupBox);
    m_mergeRadio->setChecked(true);  
    groupLayout->addWidget(m_mergeRadio);

    m_replaceRadio = new QRadioButton("Replace all existing connections", groupBox);
    groupLayout->addWidget(m_replaceRadio);

    mainLayout->addWidget(groupBox);

    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
    mainLayout->addStretch();
}

bool ImportExportDialog::shouldMerge() const
{
    return m_mergeRadio->isChecked();
}
