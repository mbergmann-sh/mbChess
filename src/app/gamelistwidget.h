// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - list of the games in a PGN file (game database): sortable columns,
// a search field, double click or Enter opens a game. Large files are indexed
// in a worker thread with progress.
#pragma once

#include <pgn/pgnindex.h>

#include <QAbstractTableModel>
#include <QWidget>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;
class QToolButton;

class GameListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Number, White, WhiteElo, Black, BlackElo, Result, Moves, Eco, Date, Event, Site, Round, ColumnCount };

    explicit GameListModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    void setIndex(std::shared_ptr<const mbc::pgn::PgnIndex> index);
    void setCurrent(int game);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void retranslate();

private:
    std::shared_ptr<const mbc::pgn::PgnIndex> m_index;
    int m_current = -1;
};

class GameListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GameListWidget(QWidget *parent = nullptr);
    ~GameListWidget() override;

    // Starts reading the file; gamesLoaded() or loadFailed() follows.
    void openFile(const QString &fileName);
    void close();

    QString fileName() const;
    int gameCount() const;
    int currentGame() const { return m_current; }
    bool loadGame(int game, mbc::Game *out, QString *error) const;
    void setCurrentGame(int game);
    // The next/previous game in the order the list shows (sorting, filter); -1 if none.
    int neighbourGame(int step) const;
    bool isLoading() const { return m_loading; }

signals:
    void gamesLoaded(int count);
    void loadFailed(const QString &error);
    void gameActivated(int game);

protected:
    void changeEvent(QEvent *e) override;

private:
    void retranslate();
    void updateCount();
    void finishLoading(std::shared_ptr<mbc::pgn::PgnIndex> index, bool ok, const QString &error);

    QLabel *m_fileLabel = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_countLabel = nullptr;
    QToolButton *m_cancel = nullptr;
    QTableView *m_view = nullptr;
    GameListModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    std::shared_ptr<const mbc::pgn::PgnIndex> m_index;
    int m_current = -1;
    bool m_loading = false;
    int m_percent = 0;
    std::shared_ptr<std::atomic_bool> m_cancelFlag;
};
