// SPDX-License-Identifier: GPL-3.0-or-later
#include "tablebasedialog.h"

#include "datafolders.h"
#include "tablebases.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QString megabytes(qint64 bytes)
{
    return QString::number(double(bytes) / (1024.0 * 1024.0), 'f', 0);
}

// First bytes of valid Syzygy files (tbcore.c): WDL 0x5d23e871, DTZ 0xa50c66d7, little endian.
bool hasMagic(const QByteArray &head, bool dtz)
{
    static const QByteArray wdl("\x71\xe8\x23\x5d", 4);
    static const QByteArray dtzMagic("\xd7\x66\x0c\xa5", 4);
    return head.startsWith(dtz ? dtzMagic : wdl);
}

} // namespace

TablebaseDialog::TablebaseDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Endgame tablebases"));
    m_sources = {
        { QStringLiteral("tablebase.lichess.ovh"),
          QStringLiteral("https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl/"),
          QStringLiteral("https://tablebase.lichess.ovh/tables/standard/3-4-5-dtz/") },
        { QStringLiteral("tablebase.lichess.ovh (3-4-5)"),
          QStringLiteral("https://tablebase.lichess.ovh/tables/standard/3-4-5/"),
          QStringLiteral("https://tablebase.lichess.ovh/tables/standard/3-4-5/") },
        { QStringLiteral("tablebase.sesse.net"),
          QStringLiteral("http://tablebase.sesse.net/syzygy/3-4-5/"),
          QStringLiteral("http://tablebase.sesse.net/syzygy/3-4-5/") },
    };
    // An own mirror (a folder with all .rtbw/.rtbz files) can be put first.
    const QString mirror = qEnvironmentVariable("MBCHESS_SYZYGY_URL");
    if (!mirror.isEmpty()) {
        const QString base = mirror.endsWith(QLatin1Char('/')) ? mirror : mirror + QLatin1Char('/');
        m_sources.prepend({ base, base, base });
    }

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Syzygy endgame tablebases contain the perfect result of every position with "
                                "few pieces. Engines that can read them (Stockfish, Arasan, Viridithas, Texel, "
                                "Patricia, Crafty) then play such endgames without mistakes and see the result "
                                "early in their search. The other engines play without them."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_use = new QCheckBox(tr("&Use endgame tablebases"), this);
    m_use->setChecked(tablebases::enabled());
    layout->addWidget(m_use);

    auto *folderRow = new QHBoxLayout;
    m_folder = new QLineEdit(QDir::toNativeSeparators(tablebases::folder()), this);
    auto *browseButton = new QPushButton(tr("&Choose..."), this);
    auto *defaultButton = new QPushButton(tr("&Default"), this);
    defaultButton->setToolTip(QDir::toNativeSeparators(datafolders::syzygyFolder()));
    connect(browseButton, &QPushButton::clicked, this, &TablebaseDialog::browse);
    connect(defaultButton, &QPushButton::clicked, this,
            [this] { m_folder->setText(QDir::toNativeSeparators(datafolders::syzygyFolder())); });
    connect(m_folder, &QLineEdit::textChanged, this, &TablebaseDialog::updateStatus);
    folderRow->addWidget(new QLabel(tr("Folder:"), this));
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(browseButton);
    folderRow->addWidget(defaultButton);
    layout->addLayout(folderRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    // Gaviota: distance to mate, read by Texel and by mbChess itself
    auto *gtbBox = new QGroupBox(tr("Gaviota tables (distance to mate)"), this);
    auto *gtbLayout = new QVBoxLayout(gtbBox);
    auto *gtbIntro = new QLabel(tr("Gaviota tables (files *.gtb.cp4) give the number of moves to mate. Texel "
                                   "uses them, and mbChess shows the result of positions with up to 5 pieces "
                                   "below the board. Copy the tables into this folder; mbChess does not download "
                                   "them (3-4-5 pieces, about 6.5 GB)."), gtbBox);
    gtbIntro->setWordWrap(true);
    gtbLayout->addWidget(gtbIntro);
    auto *gtbRow = new QHBoxLayout;
    m_gtbFolder = new QLineEdit(QDir::toNativeSeparators(tablebases::gaviotaFolder()), gtbBox);
    auto *gtbBrowse = new QPushButton(tr("C&hoose..."), gtbBox);
    auto *gtbDefault = new QPushButton(tr("D&efault"), gtbBox);
    gtbDefault->setToolTip(QDir::toNativeSeparators(datafolders::gaviotaFolder()));
    connect(gtbBrowse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder of the Gaviota tables"),
                                                              QDir::fromNativeSeparators(m_gtbFolder->text()));
        if (!dir.isEmpty())
            m_gtbFolder->setText(QDir::toNativeSeparators(dir));
    });
    connect(gtbDefault, &QPushButton::clicked, this,
            [this] { m_gtbFolder->setText(QDir::toNativeSeparators(datafolders::gaviotaFolder())); });
    connect(m_gtbFolder, &QLineEdit::textChanged, this, &TablebaseDialog::updateGaviotaStatus);
    gtbRow->addWidget(new QLabel(tr("Folder:"), gtbBox));
    gtbRow->addWidget(m_gtbFolder, 1);
    gtbRow->addWidget(gtbBrowse);
    gtbRow->addWidget(gtbDefault);
    gtbLayout->addLayout(gtbRow);
    m_gtbStatus = new QLabel(gtbBox);
    m_gtbStatus->setWordWrap(true);
    gtbLayout->addWidget(m_gtbStatus);

    auto *box = new QGroupBox(tr("Download the Syzygy tables with 3, 4 and 5 pieces"), this);
    auto *form = new QFormLayout(box);
    m_wdlOnly = new QRadioButton(tr("Results only (WDL, about %1 MB) - enough for the engines' search")
                                     .arg(megabytes(tablebases::WdlBytes)), box);
    m_withDtz = new QRadioButton(tr("Results and distance to zeroing (WDL + DTZ, about %1 MB) - recommended")
                                     .arg(megabytes(tablebases::WdlBytes + tablebases::DtzBytes)), box);
    m_withDtz->setChecked(true);
    form->addRow(m_wdlOnly);
    form->addRow(m_withDtz);
    m_source = new QComboBox(box);
    for (const Source &s : m_sources)
        m_source->addItem(s.name);
    m_source->setToolTip(tr("If a file is missing there, the other servers are tried."));
    form->addRow(tr("Server:"), m_source);
    auto *buttons = new QHBoxLayout;
    m_downloadButton = new QPushButton(tr("Do&wnload"), box);
    m_cancelButton = new QPushButton(tr("C&ancel download"), box);
    connect(m_downloadButton, &QPushButton::clicked, this, &TablebaseDialog::startDownload);
    connect(m_cancelButton, &QPushButton::clicked, this, [this] { cancelDownload(tr("Download cancelled.")); });
    buttons->addWidget(m_downloadButton);
    buttons->addWidget(m_cancelButton);
    buttons->addStretch();
    form->addRow(buttons);
    m_progress = new QProgressBar(box);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    form->addRow(m_progress);
    m_fileLabel = new QLabel(box);
    m_fileLabel->setWordWrap(true);
    form->addRow(m_fileLabel);
    auto *note = new QLabel(tr("Files that are already there are skipped, so a download can be continued "
                               "later. The tables with 6 pieces (about 150 GB) are not offered."), box);
    note->setWordWrap(true);
    form->addRow(note);
    layout->addWidget(box);
    layout->addWidget(gtbBox);

    auto *dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_closeButton = dialogButtons->button(QDialogButtonBox::Ok);
    layout->addWidget(dialogButtons);

    setDownloading(false);
    updateStatus();
    updateGaviotaStatus();
    resize(620, sizeHint().height());
}

TablebaseDialog::~TablebaseDialog()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

void TablebaseDialog::updateStatus()
{
    const QString path = QDir::fromNativeSeparators(m_folder->text().trimmed());
    const tablebases::Status st = tablebases::status(path);
    QString text = tr("Found: %1 of %2 WDL tables, %3 DTZ tables.").arg(st.wdl).arg(st.expected).arg(st.dtz);
    if (st.completeUpTo >= 3)
        text += QLatin1Char(' ') + tr("Complete up to %1 pieces.").arg(st.completeUpTo);
    else if (st.wdl == 0)
        text += QLatin1Char(' ') + tr("No tables yet: download them below.");
    m_status->setText(text);
}

void TablebaseDialog::updateGaviotaStatus()
{
    const QString path = QDir::fromNativeSeparators(m_gtbFolder->text().trimmed());
    const tablebases::Status st = tablebases::gaviotaStatus(path);
    QString text = tr("Found: %1 of %2 Gaviota tables.").arg(st.wdl).arg(st.expected);
    if (st.completeUpTo >= 3)
        text += QLatin1Char(' ') + tr("Complete up to %1 pieces.").arg(st.completeUpTo);
    m_gtbStatus->setText(text);
}

void TablebaseDialog::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder of the endgame tablebases"),
                                                          QDir::fromNativeSeparators(m_folder->text()));
    if (!dir.isEmpty())
        m_folder->setText(QDir::toNativeSeparators(dir));
}

void TablebaseDialog::setDownloading(bool on)
{
    m_downloading = on;
    m_downloadButton->setEnabled(!on);
    m_cancelButton->setEnabled(on);
    m_folder->setEnabled(!on);
    if (m_gtbFolder)
        m_gtbFolder->setEnabled(!on);
    m_wdlOnly->setEnabled(!on);
    m_withDtz->setEnabled(!on);
    m_source->setEnabled(!on);
}

bool TablebaseDialog::ensureWritableFolder()
{
    QString path = QDir::fromNativeSeparators(m_folder->text().trimmed());
    if (path.isEmpty())
        path = datafolders::syzygyFolder();
    auto writable = [](const QString &p) {
        if (!QDir().mkpath(p))
            return false;
        QFile test(QDir(p).filePath(QStringLiteral(".mbchess-write-test")));
        const bool ok = test.open(QIODevice::WriteOnly);
        test.remove();
        return ok;
    };
    if (writable(path)) {
        m_folder->setText(QDir::toNativeSeparators(path));
        return true;
    }
    const QString alt = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Syzygy");
    const auto answer = QMessageBox::question(
        this, windowTitle(),
        tr("The folder \"%1\" cannot be written to. Use \"%2\" instead?")
            .arg(QDir::toNativeSeparators(path), QDir::toNativeSeparators(alt)));
    if (answer != QMessageBox::Yes || !writable(alt))
        return false;
    m_folder->setText(QDir::toNativeSeparators(alt));
    return true;
}

void TablebaseDialog::startDownload()
{
    if (m_downloading || !ensureWritableFolder())
        return;
    const QDir dir(QDir::fromNativeSeparators(m_folder->text()));
    m_queue.clear();
    const bool dtz = m_withDtz->isChecked();
    const QStringList names = tablebases::tableNames(5);
    m_totalBytes = tablebases::WdlBytes + (dtz ? tablebases::DtzBytes : 0);
    m_doneBytes = 0;
    for (const QString &n : names) {
        for (const QString &ext : dtz ? QStringList{ QStringLiteral(".rtbw"), QStringLiteral(".rtbz") }
                                      : QStringList{ QStringLiteral(".rtbw") }) {
            const QFileInfo fi(dir.filePath(n + ext));
            if (fi.isFile() && fi.size() > 0)
                m_doneBytes += fi.size(); // already there
            else
                m_queue << n + ext;
        }
    }
    m_totalFiles = int(m_queue.size());
    m_doneFiles = 0;
    m_failedFiles = 0;
    if (m_queue.isEmpty()) {
        m_fileLabel->setText(tr("All tables are already there."));
        m_progress->setValue(1000);
        return;
    }
    m_goodSource = m_source->currentIndex();
    m_use->setChecked(true);
    setDownloading(true);
    m_progress->setValue(0);
    nextFile();
}

void TablebaseDialog::nextFile()
{
    if (!m_downloading)
        return;
    if (m_queue.isEmpty()) {
        setDownloading(false);
        updateStatus();
        m_progress->setValue(1000);
        m_fileLabel->setText(m_failedFiles == 0
                                 ? tr("Download finished: %n file(s) loaded.", nullptr, m_doneFiles)
                                 : tr("Download finished: %1 file(s) loaded, %2 could not be loaded. "
                                      "Try again later or with another server.")
                                       .arg(m_doneFiles)
                                       .arg(m_failedFiles));
        return;
    }
    m_current = m_queue.takeFirst();
    // the server that worked last first, then the others in the list order
    m_tryOrder.clear();
    m_tryOrder << m_goodSource;
    for (int i = 0; i < m_sources.size(); ++i)
        if (i != m_goodSource)
            m_tryOrder << i;
    m_try = 0;
    requestCurrent();
}

void TablebaseDialog::requestCurrent()
{
    const bool dtz = m_current.endsWith(QLatin1String(".rtbz"));
    const Source &src = m_sources.at(m_tryOrder.at(m_try));
    const QUrl url((dtz ? src.dtzBase : src.wdlBase) + m_current);

    const QDir dir(QDir::fromNativeSeparators(m_folder->text()));
    m_file.setFileName(dir.filePath(m_current + QStringLiteral(".part")));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cancelDownload(tr("\"%1\" cannot be written: %2").arg(QDir::toNativeSeparators(m_file.fileName()), m_file.errorString()));
        return;
    }
    m_currentBytes = 0;
    m_head.clear();

    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mbChess"));
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &TablebaseDialog::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &TablebaseDialog::onFinished);

    m_fileLabel->setText(tr("%1 (file %2 of %3, %4 of about %5 MB) from %6")
                             .arg(m_current)
                             .arg(m_doneFiles + m_failedFiles + 1)
                             .arg(m_totalFiles)
                             .arg(megabytes(m_doneBytes), megabytes(m_totalBytes), src.name));
}

void TablebaseDialog::onReadyRead()
{
    if (!m_reply)
        return;
    const QByteArray data = m_reply->readAll();
    if (m_head.size() < 4)
        m_head += data.left(4 - m_head.size());
    m_file.write(data);
    m_currentBytes += data.size();
    const qint64 done = m_doneBytes + m_currentBytes;
    m_progress->setValue(int(qMin<qint64>(999, m_totalBytes > 0 ? done * 1000 / m_totalBytes : 0)));
}

void TablebaseDialog::onFinished()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return; // cancelDownload() cleans up
    onReadyRead(); // anything left
    m_file.close();
    const bool dtz = m_current.endsWith(QLatin1String(".rtbz"));
    const bool ok = reply->error() == QNetworkReply::NoError && hasMagic(m_head, dtz);
    if (!ok) {
        m_file.remove();
        if (++m_try < m_tryOrder.size()) {
            requestCurrent(); // the next server
            return;
        }
        ++m_failedFiles;
        nextFile();
        return;
    }
    const QString target = m_file.fileName().chopped(5); // without ".part"
    QFile::remove(target);
    if (!m_file.rename(target)) {
        cancelDownload(tr("\"%1\" cannot be written: %2").arg(QDir::toNativeSeparators(target), m_file.errorString()));
        return;
    }
    m_goodSource = m_tryOrder.at(m_try);
    m_doneBytes += m_currentBytes;
    ++m_doneFiles;
    if (m_doneFiles % 10 == 0)
        updateStatus();
    nextFile();
}

void TablebaseDialog::cancelDownload(const QString &message)
{
    if (!m_downloading)
        return;
    setDownloading(false);
    m_queue.clear();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_file.isOpen())
        m_file.close();
    m_file.remove();
    m_fileLabel->setText(message);
    updateStatus();
}

void TablebaseDialog::accept()
{
    if (m_downloading) {
        const auto answer = QMessageBox::question(this, windowTitle(), tr("Cancel the running download?"));
        if (answer != QMessageBox::Yes)
            return;
        cancelDownload(tr("Download cancelled."));
    }
    tablebases::setEnabled(m_use->isChecked());
    tablebases::setFolder(QDir::fromNativeSeparators(m_folder->text().trimmed()));
    tablebases::setGaviotaFolder(QDir::fromNativeSeparators(m_gtbFolder->text().trimmed()));
    tablebases::openGaviotaForGui();
    QDialog::accept();
}

void TablebaseDialog::reject()
{
    if (m_downloading) {
        const auto answer = QMessageBox::question(this, windowTitle(), tr("Cancel the running download?"));
        if (answer != QMessageBox::Yes)
            return;
        cancelDownload(tr("Download cancelled."));
    }
    QDialog::reject();
}
