

#include "TriggerWidget.h"
#include <QMessageBox>
#include <QTimer>
#include <QFont>

TriggerWidget::TriggerWidget(QWidget *parent)
    : QWidget(parent)
    , m_access(0)
    , m_triggerCount(0)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    
    m_identifierLabel = new QLabel(this);
    QFont identFont = m_identifierLabel->font();
    identFont.setPointSize(14);
    identFont.setBold(true);
    m_identifierLabel->setFont(identFont);
    m_identifierLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_identifierLabel);
    
    
    m_statusLabel = new QLabel("Ready to trigger", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #666; font-size: 11pt;");
    mainLayout->addWidget(m_statusLabel);
    
    mainLayout->addSpacing(10);
    
    
    m_triggerButton = new QPushButton("TRIGGER", this);
    m_triggerButton->setMinimumHeight(80);
    m_triggerButton->setMaximumWidth(300);
    QFont buttonFont = m_triggerButton->font();
    buttonFont.setPointSize(16);
    buttonFont.setBold(true);
    m_triggerButton->setFont(buttonFont);
    m_triggerButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #2196F3;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1976D2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0D47A1;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #BDBDBD;"
        "    color: #757575;"
        "}"
    );
    connect(m_triggerButton, &QPushButton::clicked, this, &TriggerWidget::onTriggerButtonClicked);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_triggerButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    mainLayout->addSpacing(10);
    
    
    m_lastTriggeredLabel = new QLabel("Never triggered", this);
    m_lastTriggeredLabel->setAlignment(Qt::AlignCenter);
    m_lastTriggeredLabel->setStyleSheet("color: #888; font-size: 10pt;");
    mainLayout->addWidget(m_lastTriggeredLabel);
    
    
    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignCenter);
    m_pathLabel->setStyleSheet("color: #AAA; font-size: 9pt; padding-top: 10px;");
    m_pathLabel->setWordWrap(true);
    mainLayout->addWidget(m_pathLabel);
    
    mainLayout->addStretch();
}

TriggerWidget::~TriggerWidget()
{
}

void TriggerWidget::setParameterInfo(const QString &identifier, const QString &path, int access)
{
    m_identifier = identifier;
    m_parameterPath = path;
    m_access = access;
    
    m_identifierLabel->setText(identifier);
    m_pathLabel->setText(QString("Path: %1").arg(path));
    
    
    bool canWrite = (access == 2 || access == 3);
    setTriggerEnabled(canWrite);
    
    if (!canWrite) {
        m_statusLabel->setText("Read-only (cannot trigger)");
        m_statusLabel->setStyleSheet("color: #f44336; font-size: 11pt;");
    }
}

void TriggerWidget::setTriggerEnabled(bool enabled)
{
    m_triggerButton->setEnabled(enabled);
    if (!enabled && m_access != 1) {  
        m_statusLabel->setText("Trigger disabled");
        m_statusLabel->setStyleSheet("color: #999; font-size: 11pt;");
    }
}

void TriggerWidget::onTriggerButtonClicked()
{
    
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("Confirm Trigger");
    confirmBox.setText(QString("Trigger parameter '%1'?").arg(m_identifier));
    confirmBox.setInformativeText("This action will send a trigger signal to the device.");
    confirmBox.setIcon(QMessageBox::Question);
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirmBox.setDefaultButton(QMessageBox::Cancel);
    
    int result = confirmBox.exec();
    
    if (result == QMessageBox::Yes) {
        
        emit triggerActivated(m_parameterPath, "1");
        
        
        m_lastTriggerTime = QDateTime::currentDateTime();
        m_triggerCount++;
        
        updateLastTriggeredDisplay();
        showTriggerFeedback();
    }
}

void TriggerWidget::updateLastTriggeredDisplay()
{
    if (!m_lastTriggerTime.isValid()) {
        m_lastTriggeredLabel->setText("Never triggered");
        return;
    }
    
    QString timeStr = m_lastTriggerTime.toString("yyyy-MM-dd hh:mm:ss");
    m_lastTriggeredLabel->setText(QString("Last triggered: %1 (Count: %2)")
                                 .arg(timeStr)
                                 .arg(m_triggerCount));
}

void TriggerWidget::showTriggerFeedback()
{
    
    m_statusLabel->setText("✓ Triggered!");
    m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 11pt; font-weight: bold;");
    
    
    m_triggerButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 20px;"
        "}"
    );
    
    
    QTimer::singleShot(1000, this, [this]() {
        m_statusLabel->setText("Ready to trigger");
        m_statusLabel->setStyleSheet("color: #666; font-size: 11pt;");
        
        m_triggerButton->setStyleSheet(
            "QPushButton {"
            "    background-color: #2196F3;"
            "    color: white;"
            "    border: none;"
            "    border-radius: 8px;"
            "    padding: 20px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #1976D2;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #0D47A1;"
            "}"
            "QPushButton:disabled {"
            "    background-color: #BDBDBD;"
            "    color: #757575;"
            "}"
        );
    });
}
