







#include "ConnectionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(nullptr)
    , m_hostEdit(nullptr)
    , m_portSpin(nullptr)
{
    setupUi();
    setWindowTitle("Connection Details");
    resize(400, 150);
}

ConnectionDialog::~ConnectionDialog()
{
}

void ConnectionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    
    QFormLayout *formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Enter connection name");
    formLayout->addRow("Name:", m_nameEdit);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("e.g., 192.168.1.100 or localhost");
    formLayout->addRow("Host:", m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(9092);  
    formLayout->addRow("Port:", m_portSpin);

    mainLayout->addLayout(formLayout);

    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    
    m_nameEdit->setFocus();
}

void ConnectionDialog::setConnectionName(const QString &name)
{
    m_nameEdit->setText(name);
}

void ConnectionDialog::setHost(const QString &host)
{
    m_hostEdit->setText(host);
}

void ConnectionDialog::setPort(int port)
{
    m_portSpin->setValue(port);
}

QString ConnectionDialog::getConnectionName() const
{
    return m_nameEdit->text();
}

QString ConnectionDialog::getHost() const
{
    return m_hostEdit->text();
}

int ConnectionDialog::getPort() const
{
    return m_portSpin->value();
}
