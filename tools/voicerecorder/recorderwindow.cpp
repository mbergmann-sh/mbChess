// SPDX-License-Identifier: GPL-3.0-or-later
#include "recorderwindow.h"
#include "words.h"

#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMediaDevices>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cstring>

namespace {

enum Column { FileColumn, TextColumn, StateColumn };

// Built-in sets of mbChess; a recorded voice must have another name.
const QStringList kReservedNames = { QStringLiteral("thorsten-de"), QStringLiteral("libritts-en"),
                                     QStringLiteral("amichess"), QStringLiteral("effects") };

// The words of a move, as mbChess says them (src/app/soundplayer.cpp, wordsForMove).
// Accepts English (KQRBN) and German (KDTLS) piece letters.
QStringList wordsForMove(QString s)
{
    QStringList words;
    s = s.trimmed();
    const bool mate = s.endsWith(QLatin1Char('#'));
    const bool check = s.endsWith(QLatin1Char('+'));
    while (!s.isEmpty() && QStringLiteral("#+!?").contains(s.back()))
        s.chop(1);
    if (s == QLatin1String("O-O-O") || s == QLatin1String("0-0-0")) {
        words << QStringLiteral("o-o-o");
    } else if (s == QLatin1String("O-O") || s == QLatin1String("0-0")) {
        words << QStringLiteral("o-o");
    } else {
        for (const QChar c : s) {
            switch (c.unicode()) {
            case 'x': case ':': words << QStringLiteral("takes"); break;
            case 'K': words << QStringLiteral("king"); break;
            case 'Q': case 'D': words << QStringLiteral("queen"); break;
            case 'R': case 'T': words << QStringLiteral("rook"); break;
            case 'B': case 'L': words << QStringLiteral("bishop"); break;
            case 'N': case 'S': words << QStringLiteral("knight"); break;
            default:
                if ((c >= QLatin1Char('a') && c <= QLatin1Char('h')) || (c >= QLatin1Char('1') && c <= QLatin1Char('8')))
                    words << QString(c);
                break;
            }
        }
    }
    if (mate)
        words << QStringLiteral("mate");
    else if (check)
        words << QStringLiteral("check");
    return words;
}

QAudioFormat playFormat()
{
    QAudioFormat f;
    f.setSampleRate(wav::kRate);
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Int16);
    return f;
}

} // namespace

RecorderWindow::RecorderWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_uiGerman = QLocale().language() == QLocale::German;
    setWindowTitle(t("mbChess – Ansagen aufnehmen", "mbChess – record announcements"));
    QSettings s;

    // --- voice: name, language, folder, microphone
    m_name = new QLineEdit(s.value(QStringLiteral("name"), t("Eigene Stimme", "My voice")).toString(), this);
    m_language = new QComboBox(this);
    m_language->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->setCurrentIndex(s.value(QStringLiteral("language"), m_uiGerman ? QStringLiteral("de") : QStringLiteral("en"))
                                        .toString() == QLatin1String("en"));

    // Default: the themes folder of an mbChess next to this program, so the voice
    // shows up in mbChess right away (menu Board > Voice).
    QString base = QCoreApplication::applicationDirPath() + QStringLiteral("/themes/sounds");
    if (!QFileInfo::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/mbchess.exe"))
        && !QFileInfo::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/mbchess")))
        base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/mbChess-Stimmen");
    m_baseFolder = s.value(QStringLiteral("baseFolder"), base).toString();
    m_folderLabel = new QLabel(this);
    m_folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_folderLabel->setWordWrap(true);
    m_folderLabel->setMinimumHeight(m_folderLabel->fontMetrics().height() * 2 + 4);
    auto *folderButton = new QPushButton(t("Ändern...", "Change..."), this);
    m_device = new QComboBox(this);

    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folderLabel, 1);
    folderRow->addWidget(folderButton);
    auto *voiceBox = new QGroupBox(t("Stimme", "Voice"), this);
    auto *form = new QFormLayout(voiceBox);
    form->addRow(t("&Name:", "&Name:"), m_name);
    form->addRow(t("&Sprache:", "&Language:"), m_language);
    form->addRow(t("Ordner:", "Folder:"), folderRow);
    form->addRow(t("&Mikrofon:", "&Microphone:"), m_device);

    // --- list of words
    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setHeaderLabels({ t("Datei", "File"), t("Text", "Text"), t("Aufnahme", "Recording") });
    m_list->header()->setSectionResizeMode(TextColumn, QHeaderView::Stretch);
    m_list->installEventFilter(this);
    for (int i = 0; i < announcementWords().size(); ++i)
        new QTreeWidgetItem(m_list);

    // --- current word
    m_wordLabel = new QLabel(this);
    QFont big = m_wordLabel->font();
    big.setPointSizeF(big.pointSizeF() * 2.2);
    big.setBold(true);
    m_wordLabel->setFont(big);
    m_wordLabel->setAlignment(Qt::AlignCenter);
    m_wordLabel->setWordWrap(true);
    m_wordLabel->setMinimumHeight(m_wordLabel->fontMetrics().height() * 2 + 8);
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_level = new QProgressBar(this);
    m_level->setRange(0, 100);
    m_level->setTextVisible(false);
    m_level->setMaximumHeight(14);

    m_recordButton = new QPushButton(this);
    m_playButton = new QPushButton(t("▶ &Abspielen", "▶ &Play"), this);
    m_deleteButton = new QPushButton(t("&Löschen", "&Delete"), this);
    m_nextButton = new QPushButton(t("Nächstes &fehlendes", "Next &missing"), this);
    QFont bold = m_recordButton->font();
    bold.setBold(true);
    m_recordButton->setFont(bold);
    m_recordButton->setMinimumHeight(m_recordButton->sizeHint().height() * 3 / 2);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_recordButton, 2);
    buttons->addWidget(m_playButton, 1);
    buttons->addWidget(m_deleteButton, 1);
    buttons->addWidget(m_nextButton, 1);

    m_clean = new QCheckBox(t("Stille abschneiden und Lautstärke angleichen", "Cut silence and even out the volume"), this);
    m_playAfter = new QCheckBox(t("Nach der Aufnahme abspielen", "Play after recording"), this);
    m_advance = new QCheckBox(t("Danach zum nächsten fehlenden Wort", "Then go to the next missing word"), this);
    m_clean->setChecked(s.value(QStringLiteral("clean"), true).toBool());
    m_playAfter->setChecked(s.value(QStringLiteral("playAfter"), true).toBool());
    m_advance->setChecked(s.value(QStringLiteral("advance"), false).toBool());

    m_testMove = new QLineEdit(s.value(QStringLiteral("testMove"), QStringLiteral("Nxf3+")).toString(), this);
    m_testMove->setToolTip(t("Zug in Kurznotation, z. B. Nxf3+, Sxf3+, e8=Q#, O-O",
                             "Move in short notation, e.g. Nxf3+, e8=Q#, O-O"));
    auto *testButton = new QPushButton(t("Probe-Ansage", "Test announcement"), this);
    auto *testRow = new QHBoxLayout;
    testRow->addWidget(new QLabel(t("Zug:", "Move:"), this));
    testRow->addWidget(m_testMove, 1);
    testRow->addWidget(testButton);

    auto *wordBox = new QGroupBox(t("Aufnahme", "Recording"), this);
    auto *wordLayout = new QVBoxLayout(wordBox);
    wordLayout->addWidget(m_wordLabel);
    wordLayout->addWidget(m_hintLabel);
    wordLayout->addWidget(m_level);
    wordLayout->addLayout(buttons);
    wordLayout->addWidget(m_clean);
    wordLayout->addWidget(m_playAfter);
    wordLayout->addWidget(m_advance);
    wordLayout->addSpacing(8);
    wordLayout->addLayout(testRow);
    wordLayout->addStretch(1);
    auto *help = new QLabel(t("<b>Leertaste</b>: Aufnahme starten/beenden &nbsp; <b>Enter</b>: abspielen<br>"
                              "Deutlich und etwas langsamer sprechen, mit kurzer Pause vor und nach dem Wort.",
                              "<b>Space</b>: start/stop recording &nbsp; <b>Enter</b>: play<br>"
                              "Speak clearly and a little slowly, with a short pause before and after the word."),
                            this);
    help->setWordWrap(true);
    wordLayout->addWidget(help);

    auto *right = new QVBoxLayout;
    right->addWidget(voiceBox);
    right->addWidget(wordBox, 1);
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->addWidget(m_list, 1);
    layout->addLayout(right, 1);
    setCentralWidget(central);
    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);

    connect(folderButton, &QPushButton::clicked, this, &RecorderWindow::chooseBaseFolder);
    connect(m_name, &QLineEdit::textChanged, this, &RecorderWindow::refreshList);
    connect(m_language, &QComboBox::currentIndexChanged, this, &RecorderWindow::refreshList);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &RecorderWindow::updateWordPanel);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, &RecorderWindow::playCurrent);
    connect(m_recordButton, &QPushButton::clicked, this, &RecorderWindow::toggleRecording);
    connect(m_playButton, &QPushButton::clicked, this, &RecorderWindow::playCurrent);
    connect(m_deleteButton, &QPushButton::clicked, this, &RecorderWindow::deleteCurrent);
    connect(m_nextButton, &QPushButton::clicked, this, &RecorderWindow::nextMissing);
    connect(testButton, &QPushButton::clicked, this, &RecorderWindow::playTestMove);
    connect(m_testMove, &QLineEdit::returnPressed, this, &RecorderWindow::playTestMove);

    // Space starts and stops a recording (not while typing in a text field)
    auto *space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(space, &QShortcut::activated, this, [this] {
        if (m_recordButton->isEnabled())
            toggleRecording();
    });

    fillDevices();
    refreshList();
    // start with the first word that is not recorded yet
    m_list->setCurrentItem(m_list->topLevelItem(m_list->topLevelItemCount() - 1));
    nextMissing();
    resize(1000, 640);
    m_list->setFocus();
}

RecorderWindow::~RecorderWindow()
{
    stopPlayback();
    if (m_source)
        m_source->stop();
}

QString RecorderWindow::t(const char *de, const char *en) const
{
    return QString::fromUtf8(m_uiGerman ? de : en);
}

bool RecorderWindow::german() const
{
    return m_language->currentData().toString() == QLatin1String("de");
}

QString RecorderWindow::folder() const
{
    QString name = m_name->text().trimmed();
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (name.isEmpty())
        name = QStringLiteral("voice");
    return m_baseFolder + QLatin1Char('/') + name;
}

QString RecorderWindow::fileFor(int row) const
{
    return folder() + QLatin1Char('/') + QString::fromLatin1(announcementWords().at(row).file) + QStringLiteral(".wav");
}

int RecorderWindow::currentRow() const
{
    return m_list->indexOfTopLevelItem(m_list->currentItem());
}

void RecorderWindow::fillDevices()
{
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    const QAudioDevice def = QMediaDevices::defaultAudioInput();
    for (const QAudioDevice &d : inputs) {
        m_device->addItem(d.description(), QVariant::fromValue(d));
        if (d.id() == def.id())
            m_device->setCurrentIndex(m_device->count() - 1);
    }
    if (inputs.isEmpty()) {
        m_device->addItem(t("(kein Mikrofon gefunden)", "(no microphone found)"));
        m_device->setEnabled(false);
    }
}

void RecorderWindow::refreshList()
{
    const QString name = m_name->text().trimmed();
    const bool reserved = kReservedNames.contains(name, Qt::CaseInsensitive);
    m_folderLabel->setText(QDir::toNativeSeparators(folder()));
    for (int i = 0; i < m_list->topLevelItemCount(); ++i)
        updateRow(i);
    updateWordPanel();
    if (reserved)
        m_status->setText(t("Dieser Name gehört zu einer eingebauten Stimme von mbChess. Bitte einen anderen wählen.",
                            "This name belongs to a built-in voice of mbChess. Please choose another one."));
    m_recordButton->setEnabled(!reserved && m_device->isEnabled());
}

void RecorderWindow::updateRow(int row)
{
    QTreeWidgetItem *item = m_list->topLevelItem(row);
    const Word &w = announcementWords().at(row);
    item->setText(FileColumn, QString::fromLatin1(w.file));
    item->setText(TextColumn, QString::fromUtf8(german() ? w.de : w.en));
    const QString file = fileFor(row);
    if (QFileInfo::exists(file)) {
        const double sec = wav::seconds(wav::read(file));
        item->setText(StateColumn, QStringLiteral("✔ %1 s").arg(QLocale().toString(sec, 'f', 2)));
        item->setForeground(StateColumn, QColor(0, 140, 0));
    } else {
        item->setText(StateColumn, t("fehlt", "missing"));
        item->setForeground(StateColumn, palette().color(QPalette::PlaceholderText));
    }
}

void RecorderWindow::updateWordPanel()
{
    const int row = currentRow();
    const bool recording = m_source != nullptr;
    m_recordButton->setText(recording ? t("■ Aufnahme &beenden", "■ &Stop recording")
                            : (row >= 0 && QFileInfo::exists(fileFor(row)) ? t("● &Neu aufnehmen", "● &Record again")
                                                                           : t("● &Aufnehmen", "● &Record")));
    if (row < 0) {
        m_wordLabel->clear();
        m_hintLabel->clear();
        return;
    }
    const Word &w = announcementWords().at(row);
    m_wordLabel->setText(QStringLiteral("„%1“").arg(QString::fromUtf8(german() ? w.de : w.en)));
    m_hintLabel->setText(QString::fromUtf8(german() ? w.hintDe : w.hintEn));
    const bool exists = QFileInfo::exists(fileFor(row));
    m_playButton->setEnabled(exists && !recording);
    m_deleteButton->setEnabled(exists && !recording);
    m_list->setEnabled(!recording);
}

void RecorderWindow::chooseBaseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, t("Ordner für Stimmen wählen (bei mbChess: themes/sounds)", "Choose the folder for voices (in mbChess: themes/sounds)"),
        m_baseFolder);
    if (dir.isEmpty())
        return;
    m_baseFolder = dir;
    refreshList();
}

// --- recording ------------------------------------------------------------

void RecorderWindow::toggleRecording()
{
    if (m_source)
        stopRecording();
    else
        startRecording();
}

void RecorderWindow::startRecording()
{
    const int row = currentRow();
    if (row < 0 || !m_device->isEnabled())
        return;
    stopPlayback();
    const QAudioDevice dev = m_device->currentData().value<QAudioDevice>();
    QAudioFormat format = playFormat();
    if (!dev.isFormatSupported(format))
        format = dev.preferredFormat();
    m_sourceFormat = format;
    m_recorded.clear();
    m_recordingRow = row;
    m_source = new QAudioSource(dev, format, this);
    m_sourceIo = m_source->start();
    if (!m_sourceIo) {
        delete m_source;
        m_source = nullptr;
        QMessageBox::warning(this, windowTitle(), t("Das Mikrofon lässt sich nicht öffnen.", "The microphone cannot be opened."));
        return;
    }
    connect(m_sourceIo, &QIODevice::readyRead, this, &RecorderWindow::onAudioData);
    m_status->setText(t("Aufnahme läuft... (Leertaste beendet)", "Recording... (Space stops)"));
    updateWordPanel();
    m_recordButton->setFocus();
}

void RecorderWindow::onAudioData()
{
    if (!m_sourceIo)
        return;
    const QByteArray chunk = m_sourceIo->readAll();
    m_recorded += chunk;
    // level meter
    const int bytes = m_sourceFormat.bytesPerSample();
    if (bytes <= 0 || chunk.isEmpty())
        return;
    float peak = 0;
    const int n = chunk.size() / bytes;
    for (int i = 0; i < n; ++i)
        peak = qMax(peak, std::abs(m_sourceFormat.normalizedSampleValue(chunk.constData() + i * bytes)));
    m_level->setValue(int(peak * 100));
}

void RecorderWindow::stopRecording()
{
    if (!m_source)
        return;
    onAudioData();
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    m_sourceIo = nullptr;
    m_level->setValue(0);

    // to 48 kHz mono 16 bit
    const int bytes = m_sourceFormat.bytesPerSample();
    QVector<float> v;
    if (bytes > 0) {
        v.resize(m_recorded.size() / bytes);
        for (int i = 0; i < v.size(); ++i)
            v[i] = m_sourceFormat.normalizedSampleValue(m_recorded.constData() + i * bytes);
    }
    wav::Samples samples = wav::toMono48k(v, m_sourceFormat.channelCount(), m_sourceFormat.sampleRate());
    if (m_clean->isChecked())
        samples = wav::clean(samples);
    const int row = m_recordingRow;
    m_recordingRow = -1;
    if (samples.size() < wav::kRate / 20) {
        m_status->setText(t("Nichts aufgenommen – ist das Mikrofon eingeschaltet?", "Nothing recorded – is the microphone on?"));
        updateWordPanel();
        m_list->setFocus();
        return;
    }
    QDir().mkpath(folder());
    QString error;
    if (!wav::write(fileFor(row), samples, &error)) {
        QMessageBox::warning(this, windowTitle(), t("Die Datei konnte nicht gespeichert werden:\n%1", "The file could not be saved:\n%1").arg(error));
        updateWordPanel();
        return;
    }
    // theme.ini: the name mbChess shows in its voice menu
    QSettings ini(folder() + QStringLiteral("/theme.ini"), QSettings::IniFormat);
    ini.setValue(QStringLiteral("theme/name"), m_name->text().trimmed());
    ini.setValue(QStringLiteral("theme/language"), m_language->currentData().toString());

    updateRow(row);
    m_status->setText(t("Gespeichert: %1 (%2 s)", "Saved: %1 (%2 s)")
                          .arg(QDir::toNativeSeparators(fileFor(row)), QLocale().toString(wav::seconds(samples), 'f', 2)));
    if (m_playAfter->isChecked())
        play(samples);
    if (m_advance->isChecked())
        nextMissing();
    updateWordPanel();
    m_list->setFocus();
}

// --- playback ------------------------------------------------------------

void RecorderWindow::play(const wav::Samples &samples)
{
    stopPlayback();
    m_playData = QByteArray(reinterpret_cast<const char *>(samples.constData()), samples.size() * 2);
    m_playData.append(QByteArray(wav::kRate * 2 / 5, '\0')); // 400 ms silence, some backends cut the end
    m_playBuffer = new QBuffer(&m_playData, this);
    m_playBuffer->open(QIODevice::ReadOnly);
    m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), playFormat(), this);
    m_sink->start(m_playBuffer);
}

void RecorderWindow::stopPlayback()
{
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    if (m_playBuffer) {
        m_playBuffer->close();
        m_playBuffer->deleteLater();
        m_playBuffer = nullptr;
    }
}

void RecorderWindow::playCurrent()
{
    const int row = currentRow();
    if (row < 0 || m_source)
        return;
    const wav::Samples s = wav::read(fileFor(row));
    if (!s.isEmpty())
        play(s);
}

void RecorderWindow::deleteCurrent()
{
    const int row = currentRow();
    if (row < 0)
        return;
    QFile::remove(fileFor(row));
    updateRow(row);
    updateWordPanel();
}

void RecorderWindow::nextMissing()
{
    const int n = m_list->topLevelItemCount();
    const int start = qMax(0, currentRow());
    for (int k = 1; k <= n; ++k) {
        const int row = (start + k) % n;
        if (!QFileInfo::exists(fileFor(row))) {
            m_list->setCurrentItem(m_list->topLevelItem(row));
            return;
        }
    }
    m_status->setText(t("Alle Wörter sind aufgenommen. In mbChess: Brett > Stimme > „%1“.",
                        "All words are recorded. In mbChess: Board > Voice > \"%1\".")
                          .arg(m_name->text().trimmed()));
}

void RecorderWindow::playTestMove()
{
    wav::Samples all;
    const int pause = wav::kRate * 110 / 1000; // as in mbChess
    QStringList missing;
    for (const QString &w : wordsForMove(m_testMove->text())) {
        const wav::Samples s = wav::read(folder() + QLatin1Char('/') + w + QStringLiteral(".wav"));
        if (s.isEmpty()) {
            missing << w;
            continue;
        }
        if (!all.isEmpty())
            all += wav::Samples(pause, 0);
        all += s;
    }
    if (!missing.isEmpty())
        m_status->setText(t("Es fehlen noch: %1", "Still missing: %1").arg(missing.join(QStringLiteral(", "))));
    if (!all.isEmpty())
        play(all);
}

// --- keys, closing -------------------------------------------------------

bool RecorderWindow::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == m_list && e->type() == QEvent::KeyPress) {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) {
            playCurrent();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, e);
}

void RecorderWindow::closeEvent(QCloseEvent *e)
{
    if (m_source)
        stopRecording();
    QSettings s;
    s.setValue(QStringLiteral("name"), m_name->text().trimmed());
    s.setValue(QStringLiteral("language"), m_language->currentData().toString());
    s.setValue(QStringLiteral("baseFolder"), m_baseFolder);
    s.setValue(QStringLiteral("clean"), m_clean->isChecked());
    s.setValue(QStringLiteral("playAfter"), m_playAfter->isChecked());
    s.setValue(QStringLiteral("advance"), m_advance->isChecked());
    s.setValue(QStringLiteral("testMove"), m_testMove->text());
    QMainWindow::closeEvent(e);
}
