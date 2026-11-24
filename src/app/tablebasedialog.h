// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - "Endgame tablebases" dialog: switch the tables on or off, choose the
// folders of the Syzygy and the Gaviota tables, see which tables are there, and
// download the Syzygy 3-4-5 piece tables (WDL, optionally DTZ) with progress and cancel. Files already there are skipped,
// so an interrupted download simply continues.
#pragma once

#include <QDialog>
#include <QFile>
#include <QPointer>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QRadioButton;

class TablebaseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TablebaseDialog(QWidget *parent = nullptr);
    ~TablebaseDialog() override;

    void accept() override;
    void reject() override;

private:
    struct Source
    {
        QString name;
        QString wdlBase; // URL of the folder with the .rtbw files
        QString dtzBase; // URL of the folder with the .rtbz files
    };

    void updateStatus();
    void browse();
    void updateGaviotaStatus();
    void startDownload();
    void cancelDownload(const QString &message);
    void nextFile();
    void requestCurrent();
    void onReadyRead();
    void onFinished();
    void setDownloading(bool on);
    bool ensureWritableFolder();

    QCheckBox *m_use = nullptr;
    QLineEdit *m_folder = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_gtbFolder = nullptr;
    QLabel *m_gtbStatus = nullptr;
    QRadioButton *m_wdlOnly = nullptr;
    QRadioButton *m_withDtz = nullptr;
    QComboBox *m_source = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_fileLabel = nullptr;
    QPushButton *m_closeButton = nullptr;

    QVector<Source> m_sources;
    QNetworkAccessManager *m_net = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QStringList m_queue;        // file names still to load
    QString m_current;          // file being loaded
    QVector<int> m_tryOrder;    // source indices to try for the current file
    int m_try = 0;
    int m_goodSource = -1;      // the source that worked last
    int m_totalFiles = 0;
    int m_doneFiles = 0;
    int m_failedFiles = 0;
    qint64 m_totalBytes = 0;    // estimate
    qint64 m_doneBytes = 0;
    qint64 m_currentBytes = 0;
    QByteArray m_head;          // first bytes of the current file (magic number check)
    bool m_downloading = false;
};
