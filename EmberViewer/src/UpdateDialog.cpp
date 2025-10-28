/*
    EmberViewer - Update Dialog Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "UpdateDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QFont>

UpdateDialog::UpdateDialog(const UpdateManager::UpdateInfo &updateInfo, QWidget *parent)
    : QDialog(parent)
    , m_updateInfo(updateInfo)
    , m_userAction(RemindLater)
    , m_titleLabel(nullptr)
    , m_versionLabel(nullptr)
    , m_releaseNotesBrowser(nullptr)
    , m_downloadSizeLabel(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_updateNowButton(nullptr)
    , m_remindLaterButton(nullptr)
    , m_skipVersionButton(nullptr)
{
    setupUi();
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::setupUi()
{
    setWindowTitle("Update Available");
    setMinimumSize(600, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    m_titleLabel = new QLabel("A new version of EmberViewer is available!");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // Version information
    QString currentVersion = UpdateManager::getCurrentVersion();
    m_versionLabel = new QLabel(QString("Current version: %1\nNew version: %2")
        .arg(currentVersion)
        .arg(m_updateInfo.version));
    mainLayout->addWidget(m_versionLabel);

    mainLayout->addSpacing(10);

    // Release notes group
    QGroupBox *notesGroup = new QGroupBox("What's New");
    QVBoxLayout *notesLayout = new QVBoxLayout(notesGroup);

    m_releaseNotesBrowser = new QTextBrowser();
    m_releaseNotesBrowser->setOpenExternalLinks(true);
    m_releaseNotesBrowser->setHtml(formatReleaseNotes(m_updateInfo.releaseNotes));
    notesLayout->addWidget(m_releaseNotesBrowser);

    mainLayout->addWidget(notesGroup);

    // Download size info
    m_downloadSizeLabel = new QLabel(QString("Download size: %1")
        .arg(formatFileSize(m_updateInfo.assetSize)));
    mainLayout->addWidget(m_downloadSizeLabel);

    // Progress bar (initially hidden)
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // Status label (initially hidden)
    m_statusLabel = new QLabel();
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addSpacing(10);

    // Button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_skipVersionButton = new QPushButton("Skip This Version");
    connect(m_skipVersionButton, &QPushButton::clicked, this, &UpdateDialog::onSkipVersionClicked);
    buttonLayout->addWidget(m_skipVersionButton);

    m_remindLaterButton = new QPushButton("Remind Me Later");
    connect(m_remindLaterButton, &QPushButton::clicked, this, &UpdateDialog::onRemindLaterClicked);
    buttonLayout->addWidget(m_remindLaterButton);

    m_updateNowButton = new QPushButton("Update Now");
    m_updateNowButton->setDefault(true);
    connect(m_updateNowButton, &QPushButton::clicked, this, &UpdateDialog::onUpdateNowClicked);
    buttonLayout->addWidget(m_updateNowButton);

    mainLayout->addLayout(buttonLayout);
}

void UpdateDialog::onUpdateNowClicked()
{
    m_userAction = UpdateNow;

    // Disable buttons during update
    m_updateNowButton->setEnabled(false);
    m_remindLaterButton->setEnabled(false);
    m_skipVersionButton->setEnabled(false);

    // Show progress bar
    m_progressBar->setVisible(true);
    m_statusLabel->setVisible(true);
    m_statusLabel->setText("Downloading update...");

    emit updateNowClicked();
}

void UpdateDialog::onRemindLaterClicked()
{
    m_userAction = RemindLater;
    emit remindLaterClicked();
    accept();
}

void UpdateDialog::onSkipVersionClicked()
{
    m_userAction = SkipVersion;
    emit skipVersionClicked();
    accept();
}

void UpdateDialog::setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(percentage);

        QString progressText = QString("Downloading: %1 / %2 (%3%)")
            .arg(formatFileSize(bytesReceived))
            .arg(formatFileSize(bytesTotal))
            .arg(percentage);
        m_statusLabel->setText(progressText);
    }
}

void UpdateDialog::setInstallationStatus(const QString &status)
{
    m_statusLabel->setText(status);
    m_progressBar->setVisible(false);
}

QString UpdateDialog::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;

    if (bytes >= GB) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 1);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

QString UpdateDialog::formatReleaseNotes(const QString &markdown) const
{
    // Simple Markdown-to-HTML conversion
    // This is a basic implementation - could be enhanced with a proper Markdown library

    QString html = markdown;

    // Convert headers (## Header -> <h3>Header</h3>)
    html.replace(QRegularExpression("## (.+)"), "<h3>\\1</h3>");
    html.replace(QRegularExpression("# (.+)"), "<h2>\\1</h2>");

    // Convert bold (**text** -> <b>text</b>)
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");

    // Convert italic (*text* -> <i>text</i>)
    html.replace(QRegularExpression("\\*(.+?)\\*"), "<i>\\1</i>");

    // Convert bullet points (- item -> <li>item</li>)
    html.replace(QRegularExpression("^- (.+)$", QRegularExpression::MultilineOption), "<li>\\1</li>");
    html.replace(QRegularExpression("^\\* (.+)$", QRegularExpression::MultilineOption), "<li>\\1</li>");

    // Wrap list items in <ul>
    if (html.contains("<li>")) {
        html = "<ul>" + html + "</ul>";
    }

    // Convert line breaks
    html.replace("\n\n", "<br><br>");

    // Wrap in basic HTML
    QString styledHtml = QString(
        "<html><body style='font-family: sans-serif;'>"
        "%1"
        "</body></html>"
    ).arg(html);

    return styledHtml;
}
