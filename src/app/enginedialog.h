// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - "Manage engines" dialog: list of engines, add and remove, rename, and the
// engine's own UCI options (read from the engine, saved per engine).
#pragma once

#include "enginemanager.h"

#include <engine/uciengine.h>

#include <QDialog>
#include <QHash>
#include <QList>

class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QTimer;

class EngineDialog : public QDialog
{
    Q_OBJECT
public:
    // Selects the engine with the given path at the start (if any).
    explicit EngineDialog(const QString &selectPath = QString(), QWidget *parent = nullptr);
    ~EngineDialog() override;

    // The engine selected when the dialog was closed.
    QString selectedPath() const { return m_selected; }

    void accept() override;

private:
    struct Pending
    {
        QString name;
        QMap<QString, QString> options;
        bool optionsLoaded = false;
    };

    void fillList(const QString &selectPath);
    void showEngine();
    void storeCurrent();       // widgets -> m_pending
    void probe(const QString &path);
    void buildOptions();
    void addEngine();
    void removeEngine();
    void resetOptions();

    QListWidget *m_list = nullptr;
    QLineEdit *m_name = nullptr;
    QLabel *m_path = nullptr;
    QLabel *m_author = nullptr;
    QLabel *m_status = nullptr;
    QScrollArea *m_scroll = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_resetButton = nullptr;

    QVector<EngineEntry> m_engines;
    QHash<QString, Pending> m_pending;                         // by path
    QHash<QString, QList<mbc::engine::EngineOption>> m_known; // options reported by the engine
    QHash<QString, QString> m_authors;
    QHash<QString, QWidget *> m_editors;                       // option name -> editor of the shown engine
    QString m_current;                                         // path of the shown engine
    QString m_selected;

    mbc::engine::UciEngine *m_probe = nullptr;
    QString m_probePath;
    QTimer *m_probeTimer = nullptr;
};
