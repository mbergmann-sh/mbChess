// SPDX-License-Identifier: GPL-3.0-or-later
#include "enginedialog.h"

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
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

using mbc::engine::EngineOption;
using mbc::engine::UciEngine;

namespace {

bool isFileOption(const EngineOption &o)
{
    const QString n = o.name.toLower();
    return o.type == QLatin1String("string")
        && (n.contains(QLatin1String("file")) || n.contains(QLatin1String("path")) || n.contains(QLatin1String("dir"))
            || n.contains(QLatin1String("net")));
}

QString editorValue(QWidget *w)
{
    if (auto *c = qobject_cast<QCheckBox *>(w))
        return c->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
    if (auto *s = qobject_cast<QSpinBox *>(w))
        return QString::number(s->value());
    if (auto *c = qobject_cast<QComboBox *>(w))
        return c->currentText();
    if (auto *e = qobject_cast<QLineEdit *>(w))
        return e->text();
    if (auto *e = w->findChild<QLineEdit *>()) // file option: line edit + button
        return e->text();
    return QString();
}

bool sameValue(const EngineOption &o, const QString &value)
{
    if (o.type == QLatin1String("check") || o.type == QLatin1String("combo"))
        return value.compare(o.defaultValue, Qt::CaseInsensitive) == 0;
    return value == o.defaultValue;
}

} // namespace

EngineDialog::EngineDialog(const QString &selectPath, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage engines"));

    m_list = new QListWidget(this);
    m_list->setMinimumWidth(200);
    auto *addButton = new QPushButton(tr("&Add..."), this);
    m_removeButton = new QPushButton(tr("&Remove"), this);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(addButton);
    listButtons->addWidget(m_removeButton);
    auto *left = new QVBoxLayout;
    left->addWidget(m_list, 1);
    left->addLayout(listButtons);

    m_name = new QLineEdit(this);
    m_path = new QLabel(this);
    m_path->setWordWrap(true);
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_author = new QLabel(this);
    auto *info = new QFormLayout;
    info->addRow(tr("&Name:"), m_name);
    info->addRow(tr("Program:"), m_path);
    info->addRow(tr("Author:"), m_author);

    auto *optionsBox = new QGroupBox(tr("Engine options"), this);
    m_status = new QLabel(optionsBox);
    m_status->setWordWrap(true);
    m_scroll = new QScrollArea(optionsBox);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_resetButton = new QPushButton(tr("Restore &defaults"), optionsBox);
    auto *hint = new QLabel(tr("Strength, cores, hash and opening books are set in the \"New game\" dialog."), optionsBox);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    auto *optionsLayout = new QVBoxLayout(optionsBox);
    optionsLayout->addWidget(m_status);
    optionsLayout->addWidget(m_scroll, 1);
    auto *resetRow = new QHBoxLayout;
    resetRow->addWidget(hint, 1);
    resetRow->addWidget(m_resetButton);
    optionsLayout->addLayout(resetRow);

    auto *right = new QVBoxLayout;
    right->addLayout(info);
    right->addWidget(optionsBox, 1);

    auto *columns = new QHBoxLayout;
    columns->addLayout(left, 2);
    columns->addLayout(right, 3);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addWidget(buttons);

    m_probeTimer = new QTimer(this);
    m_probeTimer->setSingleShot(true);
    m_probeTimer->setInterval(8000);
    connect(m_probeTimer, &QTimer::timeout, this, [this] {
        if (m_probe && m_probePath == m_current)
            m_status->setText(tr("The engine does not answer. Is it a UCI engine?"));
        if (m_probe)
            m_probe->quit();
    });

    connect(buttons, &QDialogButtonBox::accepted, this, &EngineDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EngineDialog::reject);
    connect(addButton, &QPushButton::clicked, this, &EngineDialog::addEngine);
    connect(m_removeButton, &QPushButton::clicked, this, &EngineDialog::removeEngine);
    connect(m_resetButton, &QPushButton::clicked, this, &EngineDialog::resetOptions);
    connect(m_list, &QListWidget::currentRowChanged, this, [this] {
        storeCurrent();
        showEngine();
    });
    connect(m_name, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (QListWidgetItem *item = m_list->currentItem())
            item->setText(text.trimmed().isEmpty() ? m_name->placeholderText() : text.trimmed());
    });

    fillList(selectPath);
    resize(820, 560);
}

EngineDialog::~EngineDialog()
{
    if (m_probe)
        m_probe->quit();
}

void EngineDialog::fillList(const QString &selectPath)
{
    m_engines = enginemanager::engines();
    m_list->blockSignals(true);
    m_list->clear();
    int select = m_engines.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_engines.size(); ++i) {
        const EngineEntry &e = m_engines.at(i);
        if (!m_pending.contains(e.path))
            m_pending.insert(e.path, Pending{ e.name, e.options, false });
        auto *item = new QListWidgetItem(m_pending.value(e.path).name, m_list);
        item->setToolTip(QDir::toNativeSeparators(e.path));
        if (!e.inFolder) {
            QFont f = item->font();
            f.setItalic(true);
            item->setFont(f);
        }
        if (!selectPath.isEmpty() && QFileInfo(e.path) == QFileInfo(selectPath))
            select = i;
    }
    m_list->blockSignals(false);
    m_current.clear();
    m_list->setCurrentRow(select);
    if (select < 0)
        showEngine();
}

void EngineDialog::showEngine()
{
    const int row = m_list->currentRow();
    m_editors.clear();
    if (row < 0 || row >= m_engines.size()) {
        m_current.clear();
        m_name->clear();
        m_name->setEnabled(false);
        m_path->clear();
        m_author->clear();
        m_status->setText(tr("No engine in the list. Build the engines or add one."));
        delete m_scroll->takeWidget();
        m_removeButton->setEnabled(false);
        m_resetButton->setEnabled(false);
        return;
    }
    const EngineEntry &e = m_engines.at(row);
    m_current = e.path;
    m_name->setEnabled(true);
    m_name->setPlaceholderText(e.defaultName);
    m_name->setText(m_pending.value(e.path).name);
    m_path->setText(QDir::toNativeSeparators(e.path));
    m_author->setText(m_authors.value(e.path));
    m_removeButton->setEnabled(!e.inFolder);
    m_removeButton->setToolTip(e.inFolder ? tr("Engines in the folder \"engines\" next to the program are always in the list.")
                                          : QString());
    if (m_known.contains(e.path)) {
        buildOptions();
    } else {
        delete m_scroll->takeWidget();
        m_resetButton->setEnabled(false);
        probe(e.path);
    }
}

void EngineDialog::probe(const QString &path)
{
    if (m_probe) {
        m_probe->disconnect(this);
        m_probe->quit();
        m_probe->deleteLater();
        m_probe = nullptr;
    }
    if (!QFileInfo(path).isExecutable()) {
        m_status->setText(tr("This file does not exist or is not executable."));
        return;
    }
    m_status->setText(tr("Asking the engine for its options..."));
    m_probePath = path;
    m_probe = new UciEngine(this);
    UciEngine *probe = m_probe;
    connect(probe, &UciEngine::optionsKnown, this, [this, probe, path] {
        m_known.insert(path, probe->options());
        QString author = probe->author();
        if (!probe->name().isEmpty())
            author = author.isEmpty() ? probe->name() : QStringLiteral("%1 (%2)").arg(author, probe->name());
        m_authors.insert(path, author);
        m_probeTimer->stop();
        probe->quit();
        if (m_current == path) {
            m_author->setText(author);
            buildOptions();
        }
    });
    connect(probe, &UciEngine::errorOccurred, this, [this, path](const QString &message) {
        if (m_current == path)
            m_status->setText(message);
    });
    const EngineEntry probeEntry = enginemanager::entry(path);
    m_probe->start(path, probeEntry.arguments, probeEntry.protocol);
    m_probeTimer->start();
}

void EngineDialog::buildOptions()
{
    m_editors.clear();
    delete m_scroll->takeWidget();
    const QList<EngineOption> options = m_known.value(m_current);
    Pending &p = m_pending[m_current];
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    int shown = 0;
    for (const EngineOption &o : options) {
        if (o.type == QLatin1String("button") || enginemanager::isManagedOption(o.name)
            || o.name.startsWith(QLatin1String("UCI_"), Qt::CaseInsensitive))
            continue;
        const QString value = p.options.value(o.name, o.defaultValue);
        QWidget *editor = nullptr;
        if (o.type == QLatin1String("check")) {
            auto *c = new QCheckBox(page);
            c->setChecked(value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
            editor = c;
        } else if (o.type == QLatin1String("spin")) {
            auto *s = new QSpinBox(page);
            s->setRange(o.min.isEmpty() ? INT_MIN : o.min.toInt(), o.max.isEmpty() ? INT_MAX : o.max.toInt());
            s->setValue(value.toInt());
            editor = s;
        } else if (o.type == QLatin1String("combo")) {
            auto *c = new QComboBox(page);
            c->addItems(o.vars);
            const int i = c->findText(value, Qt::MatchFixedString);
            c->setCurrentIndex(i >= 0 ? i : 0);
            editor = c;
        } else if (isFileOption(o)) {
            auto *box = new QWidget(page);
            auto *row = new QHBoxLayout(box);
            row->setContentsMargins(0, 0, 0, 0);
            auto *e = new QLineEdit(value == QLatin1String("<empty>") ? QString() : value, box);
            auto *b = new QPushButton(tr("..."), box);
            b->setMaximumWidth(b->fontMetrics().horizontalAdvance(QStringLiteral("....")) + 16);
            row->addWidget(e, 1);
            row->addWidget(b);
            const bool dir = o.name.contains(QLatin1String("dir"), Qt::CaseInsensitive)
                || o.name.contains(QLatin1String("path"), Qt::CaseInsensitive);
            connect(b, &QPushButton::clicked, this, [this, e, dir] {
                const QString start = e->text().isEmpty() ? QFileInfo(m_current).absolutePath() : e->text();
                const QString f = dir ? QFileDialog::getExistingDirectory(this, QString(), start)
                                      : QFileDialog::getOpenFileName(this, QString(), start);
                if (!f.isEmpty())
                    e->setText(QDir::toNativeSeparators(f));
            });
            editor = box;
        } else {
            editor = new QLineEdit(value == QLatin1String("<empty>") ? QString() : value, page);
        }
        QString tip = tr("Default: %1").arg(o.defaultValue.isEmpty() ? tr("(empty)") : o.defaultValue);
        if (o.type == QLatin1String("spin"))
            tip += QStringLiteral("  (%1 … %2)").arg(o.min, o.max);
        editor->setToolTip(tip);
        auto *label = new QLabel(o.name + QLatin1Char(':'), page);
        label->setToolTip(tip);
        if (p.options.contains(o.name)) { // changed by the user: bold
            QFont f = label->font();
            f.setBold(true);
            label->setFont(f);
        }
        form->addRow(label, editor);
        m_editors.insert(o.name, editor);
        ++shown;
    }
    m_scroll->setWidget(page);
    p.optionsLoaded = true;
    m_resetButton->setEnabled(shown > 0);
    m_status->setText(shown > 0 ? tr("Changed options are shown in bold and are sent to the engine at every start.")
                                : tr("This engine has no further options."));
}

void EngineDialog::storeCurrent()
{
    if (m_current.isEmpty() || !m_pending.contains(m_current))
        return;
    Pending &p = m_pending[m_current];
    p.name = m_name->text().trimmed();
    if (!p.optionsLoaded || m_editors.isEmpty())
        return;
    const QList<EngineOption> options = m_known.value(m_current);
    for (const EngineOption &o : options) {
        QWidget *w = m_editors.value(o.name);
        if (!w)
            continue;
        const QString v = editorValue(w);
        if (sameValue(o, v) || (o.defaultValue == QLatin1String("<empty>") && v.isEmpty()))
            p.options.remove(o.name);
        else
            p.options.insert(o.name, v);
    }
}

void EngineDialog::resetOptions()
{
    if (m_current.isEmpty())
        return;
    m_pending[m_current].options.clear();
    buildOptions();
}

void EngineDialog::addEngine()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose a UCI engine"), mbc::engine::enginesDirectory());
    if (file.isEmpty())
        return;
    if (!QFileInfo(file).isExecutable()) {
        QMessageBox::warning(this, windowTitle(), tr("This file does not exist or is not executable."));
        return;
    }
    storeCurrent();
    enginemanager::add(file); // already listed: just select it
    fillList(file);
}

void EngineDialog::removeEngine()
{
    const int row = m_list->currentRow();
    if (row < 0 || m_engines.at(row).inFolder)
        return;
    const EngineEntry e = m_engines.at(row);
    if (QMessageBox::question(this, windowTitle(), tr("Remove \"%1\" from the list? The program file stays where it is.")
                                                       .arg(m_pending.value(e.path).name))
        != QMessageBox::Yes)
        return;
    enginemanager::remove(e.path);
    m_pending.remove(e.path);
    m_current.clear();
    fillList(QString());
}

void EngineDialog::accept()
{
    storeCurrent();
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        bool listed = false;
        for (const EngineEntry &e : std::as_const(m_engines))
            listed = listed || e.path == it.key();
        if (!listed)
            continue;
        enginemanager::rename(it.key(), it.value().name);
        if (it.value().optionsLoaded || !it.value().options.isEmpty())
            enginemanager::setOptions(it.key(), it.value().options);
    }
    m_selected = m_current;
    QDialog::accept();
}
