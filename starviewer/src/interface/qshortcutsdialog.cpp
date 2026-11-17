/*************************************************************************************
  Copyright (C) 2014 Laboratori de Gràfics i Imatge, Universitat de Girona &
  Institut de Diagnòstic per la Imatge.
  Girona 2014. All rights reserved.
  http://starviewer.udg.edu

  This file is part of the Starviewer (Medical Imaging Software) open source project.
  It is subject to the license terms in the LICENSE file found in the top-level
  directory of this distribution and at http://starviewer.udg.edu/license. No part of
  the Starviewer (Medical Imaging Software) open source project, including this file,
  may be copied, modified, propagated, or distributed except according to the
  terms contained in the LICENSE file.
 *************************************************************************************/

#include "qshortcutsdialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "logging.h"
#include "shortcutconfiguration.h"
#include "starviewerapplication.h"

namespace udg {

QShortcutsDialog::QShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard shortcuts"));
    resize(560, 560);

    m_keys = ShortcutConfiguration::keys();
    m_table = new QTableWidget(m_keys.size(), 3, this);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Action") << tr("Key") << QString());
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < m_keys.size(); row++)
    {
        const QString &key = m_keys.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(ShortcutConfiguration::displayName(key)));
        m_table->item(row, 0)->setFlags(Qt::ItemIsEnabled);

        QKeySequenceEdit *editor = new QKeySequenceEdit(this);
        QList<QKeySequence> sequences = ShortcutConfiguration::current(key);
        // A shortcut with more than one key shows the first one, and keeps the others if it is not changed
        editor->setKeySequence(sequences.isEmpty() ? QKeySequence() : sequences.first());
        connect(editor, &QKeySequenceEdit::keySequenceChanged, this, &QShortcutsDialog::updateConflicts);
        m_editors << editor;
        m_table->setCellWidget(row, 1, editor);

        QPushButton *reset = new QPushButton(tr("Default"), this);
        connect(reset, &QPushButton::clicked, this, [this, row]() {
            QList<QKeySequence> defaults = ShortcutConfiguration::defaults(m_keys.at(row));
            m_editors.at(row)->setKeySequence(defaults.isEmpty() ? QKeySequence() : defaults.first());
        });
        m_table->setCellWidget(row, 2, reset);
    }

    m_conflicts = new QLabel(this);
    m_conflicts->setWordWrap(true);

    QLabel *note = new QLabel(tr("The changes are used by the windows that are opened afterwards, so close and open the viewers (or restart) to see them."), this);
    note->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QShortcutsDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addWidget(m_conflicts);
    layout->addWidget(note);
    layout->addWidget(buttons);

    updateConflicts();
}

void QShortcutsDialog::updateConflicts()
{
    QMap<QString, QList<QKeySequence>> shortcuts;

    for (int row = 0; row < m_keys.size(); row++)
    {
        shortcuts.insert(ShortcutConfiguration::displayName(m_keys.at(row)), QList<QKeySequence>() << m_editors.at(row)->keySequence());
    }

    QMap<QString, QStringList> conflicts = ShortcutConfiguration::conflicts(shortcuts);
    QStringList lines;

    for (auto it = conflicts.constBegin(); it != conflicts.constEnd(); ++it)
    {
        lines << tr("%1 is used by: %2").arg(it.key(), it.value().join(", "));
    }

    m_conflicts->setText(lines.isEmpty() ? QString() : tr("Keys used by more than one action. It is not a problem if the actions are in different windows (the query screens) or work with a different button (the mouse and the wheel tools); it is if they can be used in the same window at the same time:") + "\n" + lines.join("\n"));
}

void QShortcutsDialog::apply()
{
    for (int row = 0; row < m_keys.size(); row++)
    {
        const QString &key = m_keys.at(row);
        QList<QKeySequence> existing = ShortcutConfiguration::current(key);
        QKeySequence shown = m_editors.at(row)->keySequence();

        // Only what was changed is saved: a shortcut with several keys that was not touched stays with all of them
        if (existing.isEmpty() ? !shown.isEmpty() : existing.first() != shown)
        {
            ShortcutConfiguration::set(key, QList<QKeySequence>() << shown);
            INFO_LOG(QString("The shortcut %1 was changed to \"%2\"").arg(key, shown.toString()));
        }
    }

    accept();
}

} // End namespace udg
