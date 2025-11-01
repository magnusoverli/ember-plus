









#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QProgressBar>
#include "UpdateManager.h"

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    enum UpdateAction {
        UpdateNow,
        RemindLater,
        SkipVersion
    };

    explicit UpdateDialog(const UpdateManager::UpdateInfo &updateInfo, QWidget *parent = nullptr);
    ~UpdateDialog();

    
    UpdateAction getUserAction() const { return m_userAction; }

    
    void setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    
    void setInstallationStatus(const QString &status);

signals:
    void updateNowClicked();
    void remindLaterClicked();
    void skipVersionClicked();

private slots:
    void onUpdateNowClicked();
    void onRemindLaterClicked();
    void onSkipVersionClicked();

private:
    void setupUi();
    QString formatFileSize(qint64 bytes) const;
    QString formatReleaseNotes(const QString &markdown) const;

    UpdateManager::UpdateInfo m_updateInfo;
    UpdateAction m_userAction;

    
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QTextBrowser *m_releaseNotesBrowser;
    QLabel *m_downloadSizeLabel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_updateNowButton;
    QPushButton *m_remindLaterButton;
    QPushButton *m_skipVersionButton;
};

#endif 
