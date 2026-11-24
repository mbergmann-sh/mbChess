// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess voice recorder - main window: one row per word, record, listen, record again.
#pragma once

#include "wav.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMainWindow>

class QAudioSink;
class QAudioSource;
class QBuffer;
class QCheckBox;
class QComboBox;
class QIODevice;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class RecorderWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit RecorderWindow(QWidget *parent = nullptr);
    ~RecorderWindow() override;

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    QString t(const char *de, const char *en) const;
    bool german() const;            // language of the words being recorded
    QString folder() const;         // where the WAV files go
    QString fileFor(int row) const;
    int currentRow() const;

    void fillDevices();
    void refreshList();
    void updateRow(int row);
    void updateWordPanel();
    void chooseBaseFolder();

    void toggleRecording();
    void startRecording();
    void stopRecording();
    void onAudioData();
    void play(const wav::Samples &samples);
    void stopPlayback();
    void playCurrent();
    void deleteCurrent();
    void nextMissing();
    void playTestMove();

    // UI
    QLineEdit *m_name = nullptr;
    QComboBox *m_language = nullptr;
    QLabel *m_folderLabel = nullptr;
    QComboBox *m_device = nullptr;
    QTreeWidget *m_list = nullptr;
    QLabel *m_wordLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QProgressBar *m_level = nullptr;
    QPushButton *m_recordButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QCheckBox *m_clean = nullptr;
    QCheckBox *m_playAfter = nullptr;
    QCheckBox *m_advance = nullptr;
    QLineEdit *m_testMove = nullptr;
    QLabel *m_status = nullptr;

    QString m_baseFolder;
    bool m_uiGerman = true;

    // Recording
    QAudioSource *m_source = nullptr;
    QIODevice *m_sourceIo = nullptr;
    QAudioFormat m_sourceFormat;
    QByteArray m_recorded;
    int m_recordingRow = -1;

    // Playback
    QAudioSink *m_sink = nullptr;
    QBuffer *m_playBuffer = nullptr;
    QByteArray m_playData;
};
