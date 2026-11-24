// SPDX-License-Identifier: GPL-3.0-or-later
#include "newgamedialog.h"
#include "enginedialog.h"
#include "enginemanager.h"

#include "datafolders.h"

#include <engine/uciengine.h>

#include <QComboBox>
#include <QCoreApplication>
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
#include <QPushButton>
#include <QRandomGenerator>
#include <QCheckBox>
#include <QSettings>
#include <QSpinBox>
#include <QThread>

namespace {

struct TimePreset { qint64 baseSec; qint64 incSec; };
const TimePreset kTimePresets[] = {
    { 0, 0 }, { 60, 0 }, { 180, 2 }, { 300, 0 }, { 300, 3 }, { 600, 5 },
    { 900, 10 }, { 1800, 0 }, { 3600, 30 }, { 5400, 30 }
};

int defaultThreads()
{
    return qMax(1, QThread::idealThreadCount() - 1);
}

} // namespace

const NewGameDialog::Level NewGameDialog::kLevels[] = {
    { 1350, QT_TRANSLATE_NOOP("NewGameDialog", "Beginner (about 1350 Elo)") },
    { 1600, QT_TRANSLATE_NOOP("NewGameDialog", "Casual player (about 1600 Elo)") },
    { 1900, QT_TRANSLATE_NOOP("NewGameDialog", "Club player (about 1900 Elo)") },
    { 2200, QT_TRANSLATE_NOOP("NewGameDialog", "Expert (about 2200 Elo)") },
    { 2500, QT_TRANSLATE_NOOP("NewGameDialog", "Master (about 2500 Elo)") },
    { 0,    QT_TRANSLATE_NOOP("NewGameDialog", "Full strength") },
};

int NewGameDialog::levelCount()
{
    return int(sizeof(kLevels) / sizeof(kLevels[0]));
}

QString NewGameDialog::levelName(int elo)
{
    for (int i = 0; i < levelCount(); ++i)
        if (kLevels[i].elo == elo)
            return QCoreApplication::translate("NewGameDialog", kLevels[i].name);
    return QStringLiteral("%1 Elo").arg(elo);
}

// ---------------------------------------------------------------------------

QString GameSettings::detectEngine()
{
    // Engines are only looked for in the program's "engines" folder; Stockfish first.
    // (enginemanager lists Stockfish first, then the other engines, then added ones)
    const QVector<EngineEntry> engines = enginemanager::engines();
    return engines.isEmpty() ? QString() : engines.first().path;
}

GameSettings GameSettings::load()
{
    QSettings s;
    GameSettings g;
    g.mode = Mode(qBound(0, s.value(QStringLiteral("game/mode"), int(HumanWhiteVsEngine)).toInt(), 3));
    g.colorChoice = ColorChoice(qBound(0, s.value(QStringLiteral("game/color"),
                                                  int(g.mode == HumanBlackVsEngine ? PlayBlack : PlayWhite)).toInt(), 2));
    g.playerName = s.value(QStringLiteral("game/playerName")).toString();
    g.enginePath = s.value(QStringLiteral("engine/path")).toString();
    if (g.enginePath.isEmpty() || !QFileInfo(g.enginePath).isExecutable())
        g.enginePath = detectEngine();
    g.engine2Path = s.value(QStringLiteral("engine/path2")).toString();
    if (!QFileInfo(g.engine2Path).isExecutable())
        g.engine2Path.clear(); // the dialog then offers a second engine
    g.strengthElo = s.value(QStringLiteral("engine/elo"), 1600).toInt();
    g.threads = s.value(QStringLiteral("engine/threads"), defaultThreads()).toInt();
    g.hashMb = s.value(QStringLiteral("engine/hashMb"), 128).toInt();
    g.timeControl.baseMs = s.value(QStringLiteral("game/timeBaseMs"), 300000).toLongLong();
    g.timeControl.incrementMs = s.value(QStringLiteral("game/timeIncMs"), 3000).toLongLong();
    g.autoQueen = s.value(QStringLiteral("game/autoQueen"), false).toBool();
    g.useBook = s.value(QStringLiteral("book/use"), true).toBool();
    g.bookFile = s.value(QStringLiteral("book/file")).toString();
    g.bookDepth = qBound(1, s.value(QStringLiteral("book/depth"), 12).toInt(), 60);
    g.useChessbook = s.value(QStringLiteral("chessbook/use"), true).toBool();
    g.chessbookFile = s.value(QStringLiteral("chessbook/file"), QStringLiteral("*")).toString();
    g.chessbookDepth = qBound(1, s.value(QStringLiteral("chessbook/depth"), 20).toInt(), 60);
    g.engineOwnBook = s.value(QStringLiteral("book/engineOwnBook"), true).toBool();
    g.engineBookFile = s.value(QStringLiteral("book/engineFile")).toString();
    return g;
}

void GameSettings::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("game/mode"), int(mode));
    s.setValue(QStringLiteral("game/color"), int(colorChoice));
    s.setValue(QStringLiteral("game/playerName"), playerName);
    s.setValue(QStringLiteral("engine/path"), enginePath);
    s.setValue(QStringLiteral("engine/path2"), engine2Path);
    s.setValue(QStringLiteral("engine/elo"), strengthElo);
    s.setValue(QStringLiteral("engine/threads"), threads);
    s.setValue(QStringLiteral("engine/hashMb"), hashMb);
    s.setValue(QStringLiteral("game/timeBaseMs"), timeControl.baseMs);
    s.setValue(QStringLiteral("game/timeIncMs"), timeControl.incrementMs);
    s.setValue(QStringLiteral("game/autoQueen"), autoQueen);
    s.setValue(QStringLiteral("book/use"), useBook);
    s.setValue(QStringLiteral("book/file"), bookFile);
    s.setValue(QStringLiteral("book/depth"), bookDepth);
    s.setValue(QStringLiteral("chessbook/use"), useChessbook);
    s.setValue(QStringLiteral("chessbook/file"), chessbookFile);
    s.setValue(QStringLiteral("chessbook/depth"), chessbookDepth);
    s.setValue(QStringLiteral("book/engineOwnBook"), engineOwnBook);
    s.setValue(QStringLiteral("book/engineFile"), engineBookFile);
}

// ---------------------------------------------------------------------------

NewGameDialog::NewGameDialog(const GameSettings &settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New game"));
    auto *form = new QFormLayout;

    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Engine (computer)"));
    m_mode->addItem(tr("Human (both sides on this board)"));
    m_mode->addItem(tr("Engine against engine"));
    m_mode->setCurrentIndex(settings.mode == GameSettings::HumanVsHuman ? 1
                            : settings.mode == GameSettings::EngineVsEngine ? 2 : 0);
    form->addRow(tr("&Opponent:"), m_mode);

    m_color = new QComboBox(this);
    m_color->addItem(tr("White (you move first)"));
    m_color->addItem(tr("Black"));
    m_color->addItem(tr("Random"));
    m_color->setCurrentIndex(int(settings.colorChoice));
    form->addRow(tr("&Your colour:"), m_color);

    m_name = new QLineEdit(settings.playerName, this);
    m_name->setPlaceholderText(tr("Player"));
    form->addRow(tr("Your &name:"), m_name);

    m_engine = new QComboBox(this);
    fillEngines(settings.enginePath);
    m_browse = new QPushButton(tr("&Other engine..."), this);
    auto *manage = new QPushButton(tr("Mana&ge..."), this);
    manage->setToolTip(tr("Engine list, names and engine options"));
    auto *engineRow = new QHBoxLayout;
    engineRow->addWidget(m_engine, 1);
    engineRow->addWidget(m_browse);
    engineRow->addWidget(manage);
    connect(manage, &QPushButton::clicked, this, [this] {
        EngineDialog dialog(m_engine->currentData().toString(), this);
        if (dialog.exec() == QDialog::Accepted)
            fillEngines(dialog.selectedPath().isEmpty() ? m_engine->currentData().toString() : dialog.selectedPath());
    });
    m_engineLabel = new QLabel(this);
    m_engineLabel->setBuddy(m_engine); // a layout as field gets no buddy automatically
    form->addRow(m_engineLabel, engineRow);
    // "engine against engine": the second engine plays Black
    m_engine2 = new QComboBox(this);
    for (const EngineEntry &e : enginemanager::engines())
        m_engine2->addItem(e.name, e.path);
    {
        int i = m_engine2->findData(QFileInfo(settings.engine2Path).absoluteFilePath());
        if (i < 0 && !settings.engine2Path.isEmpty() && QFileInfo(settings.engine2Path).isExecutable()) {
            m_engine2->addItem(enginemanager::entry(settings.engine2Path).name, QFileInfo(settings.engine2Path).absoluteFilePath());
            i = m_engine2->count() - 1;
        }
        if (i >= 0)
            m_engine2->setCurrentIndex(i);
        else if (m_engine2->count() > 1)
            m_engine2->setCurrentIndex(1);
    }
    m_engine2Label = new QLabel(tr("Engine (&Black):"), this);
    m_engine2Label->setBuddy(m_engine2);
    form->addRow(m_engine2Label, m_engine2);
    m_engineHint = new QLabel(this);
    m_engineHint->setWordWrap(true);
    form->addRow(QString(), m_engineHint);
    connect(m_browse, &QPushButton::clicked, this, &NewGameDialog::browseEngine);

    m_strength = new QComboBox(this);
    for (int i = 0; i < levelCount(); ++i) {
        m_strength->addItem(tr(kLevels[i].name), kLevels[i].elo);
        if (kLevels[i].elo == settings.strengthElo)
            m_strength->setCurrentIndex(i);
    }
    form->addRow(tr("&Strength:"), m_strength);

    m_threads = new QSpinBox(this);
    m_threads->setRange(1, qMax(1, QThread::idealThreadCount()));
    m_threads->setValue(qBound(1, settings.threads, m_threads->maximum()));
    m_threads->setSuffix(tr(" of %1").arg(QThread::idealThreadCount()));
    form->addRow(tr("CPU &cores:"), m_threads);

    m_hash = new QSpinBox(this);
    m_hash->setRange(16, 4096);
    m_hash->setSingleStep(16);
    m_hash->setValue(settings.hashMb);
    m_hash->setSuffix(QStringLiteral(" MB"));
    form->addRow(tr("&Hash table:"), m_hash);

    m_time = new QComboBox(this);
    for (const TimePreset &p : kTimePresets) {
        TimeControl tc;
        tc.baseMs = p.baseSec * 1000;
        tc.incrementMs = p.incSec * 1000;
        m_time->addItem(tc.label(), tc.pgnTag());
        if (tc.baseMs == settings.timeControl.baseMs && tc.incrementMs == settings.timeControl.incrementMs)
            m_time->setCurrentIndex(m_time->count() - 1);
    }
    form->addRow(tr("&Time control:"), m_time);

    m_autoQueen = new QCheckBox(tr("Always promote to &queen"), this);
    m_autoQueen->setChecked(settings.autoQueen);
    form->addRow(QString(), m_autoQueen);
    m_fromPosition = new QCheckBox(tr("Play from the position on the boa&rd"), this);
    m_fromPosition->setToolTip(tr("Off: the game starts from the initial position."));
    m_fromPosition->setVisible(false);
    form->addRow(QString(), m_fromPosition);

    // Opening books and chessbooks
    auto *bookBox = new QGroupBox(tr("Opening book and chessbooks"), this);
    auto *bookForm = new QFormLayout(bookBox);
    m_useBook = new QCheckBox(tr("Engine plays from the &opening book"), bookBox);
    m_useBook->setChecked(settings.useBook);
    bookForm->addRow(m_useBook);
    m_bookCombo = new QComboBox(bookBox);
    fillFileCombo(m_bookCombo, datafolders::openingsFolder(), QStringLiteral("*.bin"),
                  tr("built-in book (mbChess)"), QString(), settings.bookFile);
    m_bookDepth = new QSpinBox(bookBox);
    m_bookDepth->setRange(1, 60);
    m_bookDepth->setValue(settings.bookDepth);
    m_bookDepth->setPrefix(tr("up to move "));
    auto *bookRow = new QHBoxLayout;
    bookRow->addWidget(m_bookCombo, 1);
    bookRow->addWidget(m_bookDepth);
    auto *bookLabel = new QLabel(tr("&Book (PolyGlot):"), bookBox);
    bookLabel->setBuddy(m_bookCombo);
    bookForm->addRow(bookLabel, bookRow);

    m_useChessbook = new QCheckBox(tr("Engine also plays from &game databases (chessbooks)"), bookBox);
    m_useChessbook->setChecked(settings.useChessbook);
    bookForm->addRow(m_useChessbook);
    m_chessbookCombo = new QComboBox(bookBox);
    fillFileCombo(m_chessbookCombo, datafolders::chessbooksFolder(), QStringLiteral("*.pgn"),
                  tr("all databases in the folder \"Chessbooks\""), QStringLiteral("*"), settings.chessbookFile);
    m_chessbookDepth = new QSpinBox(bookBox);
    m_chessbookDepth->setRange(1, 60);
    m_chessbookDepth->setValue(settings.chessbookDepth);
    m_chessbookDepth->setPrefix(tr("up to move "));
    auto *chessbookRow = new QHBoxLayout;
    chessbookRow->addWidget(m_chessbookCombo, 1);
    chessbookRow->addWidget(m_chessbookDepth);
    auto *chessbookLabel = new QLabel(tr("&Chessbook (PGN):"), bookBox);
    chessbookLabel->setBuddy(m_chessbookCombo);
    bookForm->addRow(chessbookLabel, chessbookRow);
    m_chessbookHint = new QLabel(bookBox);
    m_chessbookHint->setWordWrap(true);
    m_chessbookHint->setText(tr("Moves that scored well in the games of the database are played. Opening books "
                                "belong in the folder \"%1\", databases in \"%2\".")
                                 .arg(QDir::toNativeSeparators(datafolders::openingsFolder()),
                                      QDir::toNativeSeparators(datafolders::chessbooksFolder())));
    QFont small = m_chessbookHint->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    m_chessbookHint->setFont(small);
    bookForm->addRow(m_chessbookHint);

    m_ownBook = new QCheckBox(tr("Let the engine use its o&wn book, if it has one"), bookBox);
    m_ownBook->setChecked(settings.engineOwnBook);
    bookForm->addRow(m_ownBook);
    m_engineBookFile = new QLineEdit(QDir::toNativeSeparators(settings.engineBookFile), bookBox);
    m_engineBookFile->setPlaceholderText(tr("engine default (PolyGlot engines: mbChess book)"));
    m_engineBookFile->setClearButtonEnabled(true);
    m_engineBookBrowse = new QPushButton(tr("Choose..."), bookBox);
    auto *engineBookRow = new QHBoxLayout;
    engineBookRow->addWidget(m_engineBookFile, 1);
    engineBookRow->addWidget(m_engineBookBrowse);
    connect(m_engineBookBrowse, &QPushButton::clicked, this, [this] {
        const QString text = m_engineBookFile->text();
        const QString start = text.isEmpty() ? datafolders::openingsFolder() : QFileInfo(text).absolutePath();
        const QString file = QFileDialog::getOpenFileName(this, tr("Choose an opening book"), start,
                                                          tr("Opening books (*.bin);;All files (*)"));
        if (!file.isEmpty())
            m_engineBookFile->setText(QDir::toNativeSeparators(file));
    });
    // A row with a layout gets no buddy: without one the label would show the "&".
    auto *engineBookLabel = new QLabel(tr("Engine boo&k:"), bookBox);
    engineBookLabel->setBuddy(m_engineBookFile);
    bookForm->addRow(engineBookLabel, engineBookRow);
    connect(m_useBook, &QCheckBox::toggled, this, &NewGameDialog::updateEnabled);
    connect(m_useChessbook, &QCheckBox::toggled, this, &NewGameDialog::updateEnabled);
    connect(m_ownBook, &QCheckBox::toggled, this, &NewGameDialog::updateEnabled);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("&Start game"));
    connect(buttons, &QDialogButtonBox::accepted, this, &NewGameDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &NewGameDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(bookBox);
    layout->addWidget(buttons);

    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewGameDialog::updateEnabled);
    connect(m_engine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewGameDialog::updateEnabled);
    updateEnabled();
    setMinimumWidth(520);
}

void NewGameDialog::fillFileCombo(QComboBox *combo, const QString &folder, const QString &pattern,
                                  const QString &firstText, const QString &firstData, const QString &current)
{
    // first entry, the files of the folder, an earlier chosen file, "Other file..."
    combo->addItem(firstText, firstData);
    const QFileInfoList files = QDir(folder).entryInfoList({ pattern }, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &f : files)
        combo->addItem(f.completeBaseName(), f.absoluteFilePath());
    if (!current.isEmpty() && combo->findData(current) < 0 && QFileInfo(current).isFile())
        combo->addItem(QFileInfo(current).completeBaseName(), current);
    combo->addItem(tr("Other file..."), QStringLiteral("?"));
    const int index = combo->findData(current);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    combo->setProperty("lastIndex", combo->currentIndex());
    connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, combo, folder, pattern](int index) {
        if (combo->itemData(index).toString() != QLatin1String("?")) {
            combo->setProperty("lastIndex", index);
            return;
        }
        const QString filter = pattern == QLatin1String("*.bin") ? tr("Opening books (*.bin);;All files (*)")
                                                                 : tr("Game databases (*.pgn);;All files (*)");
        const QString file = QFileDialog::getOpenFileName(this, tr("Choose a file"), folder, filter);
        if (file.isEmpty()) {
            combo->setCurrentIndex(combo->property("lastIndex").toInt());
            return;
        }
        int found = combo->findData(file);
        if (found < 0) {
            combo->insertItem(combo->count() - 1, QFileInfo(file).completeBaseName(), file);
            found = combo->count() - 2;
        }
        combo->setCurrentIndex(found);
        combo->setProperty("lastIndex", found);
    });
}

void NewGameDialog::updateEnabled()
{
    const bool eve = m_mode->currentIndex() == 2;
    const bool engine = m_mode->currentIndex() != 1;
    m_engineLabel->setText(eve ? tr("Engine (&White):") : tr("&Engine:"));
    m_engine2->setVisible(eve);
    m_engine2Label->setVisible(eve);
    m_color->setEnabled(engine && !eve);
    m_name->setEnabled(!eve);
    for (QWidget *w : std::initializer_list<QWidget *>{ m_engine, m_browse, m_strength, m_threads, m_hash,
                                                        m_engineHint, m_useBook, m_useChessbook, m_ownBook, m_chessbookHint })
        w->setEnabled(engine);
    for (QWidget *w : std::initializer_list<QWidget *>{ m_bookCombo, m_bookDepth })
        w->setEnabled(engine && m_useBook->isChecked());
    for (QWidget *w : std::initializer_list<QWidget *>{ m_chessbookCombo, m_chessbookDepth })
        w->setEnabled(engine && m_useChessbook->isChecked());
    for (QWidget *w : std::initializer_list<QWidget *>{ m_engineBookFile, m_engineBookBrowse })
        w->setEnabled(engine && m_ownBook->isChecked());
    const QString path = m_engine->currentData().toString();
    if (!engine)
        m_engineHint->clear();
    else if (path.isEmpty())
        m_engineHint->setText(tr("No engine found in the folder \"%1\". Build the engines or choose one with \"Other engine\".")
                                  .arg(QDir::toNativeSeparators(mbc::engine::enginesDirectory())));
    else if (!QFileInfo(path).isExecutable())
        m_engineHint->setText(tr("This file does not exist or is not executable."));
    else
        m_engineHint->setText(QDir::toNativeSeparators(path));
}

void NewGameDialog::fillEngines(const QString &select)
{
    m_engine->clear();
    for (const EngineEntry &e : enginemanager::engines())
        m_engine->addItem(e.name, e.path);
    selectEngine(select);
}

void NewGameDialog::selectEngine(const QString &path)
{
    if (path.isEmpty())
        return;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    int index = m_engine->findData(absolute);
    if (index < 0 && QFileInfo(absolute).isExecutable()) {
        // an engine outside the engines folder (chosen with "Other engine"): into the
        // engine list, so that it can get options and stays available
        enginemanager::add(absolute);
        m_engine->addItem(enginemanager::entry(absolute).name, absolute);
        index = m_engine->count() - 1;
    }
    if (index >= 0)
        m_engine->setCurrentIndex(index);
}

void NewGameDialog::browseEngine()
{
    QString start = m_engine->currentData().toString();
    start = start.isEmpty() ? mbc::engine::enginesDirectory() : QFileInfo(start).absolutePath();
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose a UCI engine"), start);
    if (!file.isEmpty())
        selectEngine(file);
}

void NewGameDialog::offerBoardPosition(bool checked)
{
    m_fromPosition->setVisible(true);
    m_fromPosition->setChecked(checked);
}

bool NewGameDialog::fromBoardPosition() const
{
    return !m_fromPosition->isHidden() && m_fromPosition->isChecked();
}

void NewGameDialog::accept()
{
    if (m_mode->currentIndex() != 1
        && (!QFileInfo(m_engine->currentData().toString()).isExecutable()
            || (m_mode->currentIndex() == 2 && !QFileInfo(m_engine2->currentData().toString()).isExecutable()))) {
        QMessageBox::warning(this, tr("No engine"), tr("Please choose an executable UCI engine first."));
        return;
    }
    for (QLineEdit *edit : { m_engineBookFile }) {
        if (edit->isEnabled() && !edit->text().trimmed().isEmpty() && !QFileInfo(edit->text().trimmed()).isFile()) {
            QMessageBox::warning(this, tr("Opening book"), tr("The book file \"%1\" does not exist.").arg(edit->text().trimmed()));
            edit->setFocus();
            return;
        }
    }
    QDialog::accept();
}

GameSettings NewGameDialog::settings() const
{
    GameSettings g;
    g.colorChoice = GameSettings::ColorChoice(m_color->currentIndex());
    if (m_mode->currentIndex() == 1) {
        g.mode = GameSettings::HumanVsHuman;
    } else if (m_mode->currentIndex() == 2) {
        g.mode = GameSettings::EngineVsEngine;
    } else {
        bool white = g.colorChoice == GameSettings::PlayWhite;
        if (g.colorChoice == GameSettings::PlayRandom)
            white = QRandomGenerator::global()->bounded(2) == 0;
        g.mode = white ? GameSettings::HumanWhiteVsEngine : GameSettings::HumanBlackVsEngine;
    }
    g.playerName = m_name->text().trimmed();
    g.enginePath = m_engine->currentData().toString();
    g.engine2Path = m_engine2->currentData().toString();
    if (g.engine2Path.isEmpty())
        g.engine2Path = g.enginePath;
    g.strengthElo = m_strength->currentData().toInt();
    g.threads = m_threads->value();
    g.hashMb = m_hash->value();
    g.timeControl = TimeControl::fromPgnTag(m_time->currentData().toString());
    g.autoQueen = m_autoQueen->isChecked();
    g.useBook = m_useBook->isChecked();
    g.bookFile = m_bookCombo->currentData().toString();
    g.bookDepth = m_bookDepth->value();
    g.useChessbook = m_useChessbook->isChecked();
    g.chessbookFile = m_chessbookCombo->currentData().toString();
    g.chessbookDepth = m_chessbookDepth->value();
    g.engineOwnBook = m_ownBook->isChecked();
    g.engineBookFile = QDir::fromNativeSeparators(m_engineBookFile->text().trimmed());
    return g;
}
