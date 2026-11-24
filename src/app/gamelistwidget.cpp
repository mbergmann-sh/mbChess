// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamelistwidget.h"

#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using mbc::pgn::GameInfo;
using mbc::pgn::PgnIndex;

// ---------------------------------------------------------------------------
// Model

void GameListModel::setIndex(std::shared_ptr<const PgnIndex> index)
{
    beginResetModel();
    m_index = std::move(index);
    m_current = -1;
    endResetModel();
}

void GameListModel::setCurrent(int game)
{
    const int old = m_current;
    m_current = game;
    if (old >= 0 && old < rowCount())
        emit dataChanged(index(old, 0), index(old, ColumnCount - 1), { Qt::FontRole });
    if (game >= 0 && game < rowCount())
        emit dataChanged(index(game, 0), index(game, ColumnCount - 1), { Qt::FontRole });
}

int GameListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_index ? 0 : m_index->size();
}

int GameListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

namespace {

QString resultText(const QString &r)
{
    if (r == QLatin1String("1/2-1/2"))
        return QStringLiteral("½-½");
    return r.isEmpty() ? QStringLiteral("*") : r;
}

// "2024.05.??" -> "2024.05", "????.??.??" -> ""
QString dateText(QString d)
{
    while (d.endsWith(QLatin1String(".??")))
        d.chop(3);
    return d.startsWith(QLatin1String("????")) ? QString() : d;
}

QString unknownToEmpty(const QString &s)
{
    return s == QLatin1String("?") ? QString() : s;
}

} // namespace

QVariant GameListModel::data(const QModelIndex &index, int role) const
{
    if (!m_index || !index.isValid() || index.row() >= m_index->size())
        return QVariant();
    const GameInfo &g = m_index->info(index.row());
    if (role == Qt::FontRole && index.row() == m_current) {
        QFont f;
        f.setBold(true);
        return f;
    }
    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Number: case WhiteElo: case BlackElo: case Moves:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        case Result:
            return int(Qt::AlignCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role != Qt::DisplayRole && role != Qt::UserRole && role != Qt::ToolTipRole)
        return QVariant();
    const bool sort = role == Qt::UserRole;
    switch (index.column()) {
    case Number: return index.row() + 1;
    case White: return unknownToEmpty(g.white);
    case WhiteElo: return sort || g.whiteElo > 0 ? QVariant(g.whiteElo) : QVariant(QString());
    case Black: return unknownToEmpty(g.black);
    case BlackElo: return sort || g.blackElo > 0 ? QVariant(g.blackElo) : QVariant(QString());
    case Result: return resultText(g.result);
    case Moves: return sort ? QVariant(g.plies) : QVariant((g.plies + 1) / 2);
    case Eco: return g.eco;
    case Date: return sort ? g.date : dateText(g.date);
    case Event: return unknownToEmpty(g.event);
    case Site: return unknownToEmpty(g.site);
    case Round: return unknownToEmpty(g.round);
    default: break;
    }
    return QVariant();
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case Number: return tr("No.");
    case White: return tr("White");
    case WhiteElo: return tr("Elo");
    case Black: return tr("Black");
    case BlackElo: return tr("Elo");
    case Result: return tr("Result");
    case Moves: return tr("Moves");
    case Eco: return tr("ECO");
    case Date: return tr("Date");
    case Event: return tr("Event");
    case Site: return tr("Site");
    case Round: return tr("Round");
    default: break;
    }
    return QVariant();
}

void GameListModel::retranslate()
{
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

// ---------------------------------------------------------------------------
// Widget

GameListWidget::GameListWidget(QWidget *parent)
    : QWidget(parent)
{
    m_fileLabel = new QLabel(this);
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_search = new QLineEdit(this);
    m_search->setClearButtonEnabled(true);
    m_countLabel = new QLabel(this);
    m_cancel = new QToolButton(this);
    m_cancel->setVisible(false);

    m_model = new GameListModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_view = new QTableView(this);
    m_view->setModel(m_proxy);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(GameListModel::Number, Qt::AscendingOrder);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    m_view->verticalHeader()->hide();
    m_view->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setSectionsMovable(true);
    m_view->horizontalHeader()->setHighlightSections(false);
    const int ch = fontMetrics().averageCharWidth();
    const int widths[GameListModel::ColumnCount] = { 7, 22, 6, 22, 6, 6, 6, 5, 10, 24, 18, 6 };
    for (int c = 0; c < GameListModel::ColumnCount; ++c)
        m_view->setColumnWidth(c, widths[c] * ch + 12);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_fileLabel, 1);
    top->addWidget(m_countLabel);
    top->addWidget(m_cancel);
    top->addWidget(m_search, 1);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(top);
    layout->addWidget(m_view, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterFixedString(text.trimmed());
        updateCount();
    });
    connect(m_view, &QTableView::activated, this, [this](const QModelIndex &index) {
        if (index.isValid())
            emit gameActivated(m_proxy->mapToSource(index).row());
    });
    connect(m_cancel, &QToolButton::clicked, this, [this]() {
        if (m_cancelFlag)
            *m_cancelFlag = true;
    });

    retranslate();
}

GameListWidget::~GameListWidget()
{
    if (m_cancelFlag)
        *m_cancelFlag = true; // a running scan stops soon and cleans up after itself
}

QString GameListWidget::fileName() const
{
    return m_index ? m_index->fileName() : QString();
}

int GameListWidget::gameCount() const
{
    return m_index ? m_index->size() : 0;
}

void GameListWidget::openFile(const QString &fileName)
{
    if (m_cancelFlag)
        *m_cancelFlag = true;
    auto index = std::make_shared<PgnIndex>();
    auto cancel = std::make_shared<std::atomic_bool>(false);
    auto percent = std::make_shared<std::atomic_int>(0);
    m_cancelFlag = cancel;
    m_loading = true;
    m_percent = 0;
    m_fileLabel->setText(QFileInfo(fileName).fileName());
    m_fileLabel->setToolTip(QFileInfo(fileName).absoluteFilePath());
    m_cancel->setVisible(true);
    updateCount();

    // Small files are read right away, large ones in a worker thread.
    if (QFileInfo(fileName).size() < 4 * 1024 * 1024) {
        QString error;
        const bool ok = index->scan(fileName, &error);
        finishLoading(index, ok, error);
        return;
    }

    auto ok = std::make_shared<bool>(false);
    auto error = std::make_shared<QString>();
    QThread *thread = QThread::create([index, cancel, percent, ok, error, fileName]() {
        *ok = index->scan(fileName, error.get(), [cancel, percent](qint64 done, qint64 total) {
            *percent = total > 0 ? int(done * 100 / total) : 0;
            return !*cancel;
        });
    });
    auto *timer = new QTimer(this);
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, this, [this, percent, cancel]() {
        if (m_cancelFlag == cancel) {
            m_percent = *percent;
            updateCount();
        }
    });
    connect(thread, &QThread::finished, this, [this, thread, index, cancel, ok, error]() {
        thread->deleteLater();
        if (m_cancelFlag != cancel)
            return; // replaced by a newer file
        if (*cancel) {
            m_loading = false;
            m_cancel->setVisible(false);
            m_cancelFlag.reset();
            updateCount();
            emit loadFailed(QString());
            return;
        }
        finishLoading(index, *ok, *error);
    });
    connect(thread, &QThread::finished, timer, &QObject::deleteLater);
    timer->start();
    thread->start(QThread::LowPriority);
}

void GameListWidget::finishLoading(std::shared_ptr<PgnIndex> index, bool ok, const QString &error)
{
    m_loading = false;
    m_cancel->setVisible(false);
    m_cancelFlag.reset();
    if (!ok) {
        updateCount();
        emit loadFailed(error);
        return;
    }
    m_index = index;
    m_model->setIndex(m_index);
    m_current = -1;
    m_search->clear();
    m_proxy->setFilterFixedString(QString());
    updateCount();
    emit gamesLoaded(m_index->size());
}

void GameListWidget::close()
{
    if (m_cancelFlag)
        *m_cancelFlag = true;
    m_cancelFlag.reset();
    m_loading = false;
    m_index.reset();
    m_model->setIndex(nullptr);
    m_current = -1;
    m_fileLabel->clear();
    m_cancel->setVisible(false);
    updateCount();
}

bool GameListWidget::loadGame(int game, mbc::Game *out, QString *error) const
{
    return m_index && m_index->load(game, out, error);
}

void GameListWidget::setCurrentGame(int game)
{
    m_current = game;
    m_model->setCurrent(game);
    const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(game, 0));
    if (proxyIndex.isValid()) {
        m_view->setCurrentIndex(proxyIndex);
        m_view->scrollTo(proxyIndex);
    }
}

int GameListWidget::neighbourGame(int step) const
{
    if (!m_index || m_proxy->rowCount() == 0)
        return -1;
    const QModelIndex cur = m_proxy->mapFromSource(m_model->index(m_current, 0));
    int row = cur.isValid() ? cur.row() + step : (step > 0 ? 0 : m_proxy->rowCount() - 1);
    if (row < 0 || row >= m_proxy->rowCount())
        return -1;
    return m_proxy->mapToSource(m_proxy->index(row, 0)).row();
}

void GameListWidget::updateCount()
{
    if (m_loading) {
        m_countLabel->setText(tr("Reading... %1 %").arg(m_percent));
        return;
    }
    if (!m_index) {
        m_countLabel->setText(tr("No database open"));
        return;
    }
    const int total = m_index->size();
    const int shown = m_proxy->rowCount();
    m_countLabel->setText(shown == total ? tr("%n game(s)", nullptr, total)
                                         : tr("%1 of %n game(s)", nullptr, total).arg(shown));
}

void GameListWidget::retranslate()
{
    m_search->setPlaceholderText(tr("Search: player, event, site, ECO, year..."));
    m_cancel->setText(tr("Cancel"));
    m_model->retranslate();
    updateCount();
}

void GameListWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslate();
}
