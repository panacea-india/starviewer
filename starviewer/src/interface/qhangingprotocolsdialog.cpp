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

#include "qhangingprotocolsdialog.h"

#include <memory>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "coresettings.h"
#include "hangingprotocol.h"
#include "hangingprotocoldisplayset.h"
#include "hangingprotocolimageset.h"
#include "hangingprotocolmask.h"
#include "hangingprotocolsloader.h"
#include "hangingprotocolxmlreader.h"
#include "hangingprotocolxmlwriter.h"
#include "logging.h"
#include "settings.h"
#include "starviewerapplication.h"

namespace udg {

namespace {

// Changes the name, priority, modalities and the places of the viewers of a protocol of the user, in its file. The image sets (what series go to the viewers) are not changed here.
class EditProtocolDialog : public QDialog {
public:
    EditProtocolDialog(const QString &path, QWidget *parent)
        : QDialog(parent), m_path(path)
    {
        setWindowTitle(tr("Edit the protocol"));
        resize(640, 480);

        HangingProtocolXMLReader reader;
        m_protocol.reset(reader.readFile(path));

        m_name = new QLineEdit(this);
        m_modalities = new QLineEdit(this);
        m_modalities->setToolTip(tr("The modalities the protocol is for, separated by commas (CT, MR...)"));
        m_priority = new QDoubleSpinBox(this);
        m_priority->setRange(0.0, 1000.0);
        m_priority->setDecimals(2);
        m_priority->setToolTip(tr("When more than one protocol fits the study, the one with the highest priority is used"));
        m_strict = new QCheckBox(tr("Use it only if all the viewers can be filled"), this);
        m_allDifferent = new QCheckBox(tr("Every viewer must show a different series"), this);

        QFormLayout *form = new QFormLayout();
        form->addRow(tr("Name:"), m_name);
        form->addRow(tr("Modalities:"), m_modalities);
        form->addRow(tr("Priority:"), m_priority);
        form->addRow(QString(), m_strict);
        form->addRow(QString(), m_allDifferent);

        m_table = new QTableWidget(0, 5, this);
        m_table->setHorizontalHeaderLabels(QStringList() << tr("Viewer") << tr("Series") << tr("Position (x1\\y1\\x2\\y2)") << tr("Image number") << tr("Phase"));
        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

        QLabel *note = new QLabel(tr("The position is the top left and the bottom right corners of the viewer in the window, from 0 to 1 (0\\1\\1\\0 is the whole window). The image number and the phase "
                                     "are -1 to leave them as they are. The series of each viewer cannot be changed here: edit the XML file for that. The changes are used when the program starts."), this);
        note->setWordWrap(true);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &EditProtocolDialog::save);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(m_table, 1);
        layout->addWidget(note);
        layout->addWidget(buttons);

        if (!m_protocol)
        {
            return;
        }

        m_name->setText(m_protocol->getName());
        m_modalities->setText(m_protocol->getHangingProtocolMask()->getProtocolList().join(", "));
        m_priority->setValue(m_protocol->getPriority());
        m_strict->setChecked(m_protocol->isStrict());
        m_allDifferent->setChecked(m_protocol->getAllDifferent());

        m_displaySets = m_protocol->getDisplaySets();
        m_table->setRowCount(m_displaySets.size());

        for (int row = 0; row < m_displaySets.size(); row++)
        {
            HangingProtocolDisplaySet *set = m_displaySets[row];
            setCell(row, 0, QString::number(set->getIdentifier()), false);
            setCell(row, 1, set->getImageSet() ? QString::number(set->getImageSet()->getIdentifier()) : QString(), false);
            setCell(row, 2, set->getPosition(), true);
            setCell(row, 3, QString::number(set->getSlice()), true);
            setCell(row, 4, QString::number(set->getPhase()), true);
        }
    }

    bool isLoaded() const
    {
        return m_protocol != nullptr;
    }

private:
    void setCell(int row, int column, const QString &text, bool editable)
    {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setFlags(editable ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable) : Qt::ItemIsEnabled);
        m_table->setItem(row, column, item);
    }

    void save()
    {
        QString name = m_name->text().trimmed();

        if (name.isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("The protocol needs a name."));
            return;
        }

        QStringList modalities;

        for (const QString &part : m_modalities->text().split(',', QString::SkipEmptyParts))
        {
            modalities << part.trimmed();
        }

        // Everything is checked before anything is changed
        QVector<int> slices;
        QVector<int> phases;

        for (int row = 0; row < m_displaySets.size(); row++)
        {
            if (!HangingProtocolFiles::isValidPosition(m_table->item(row, 2)->text()))
            {
                QMessageBox::warning(this, ApplicationNameString, tr("The position of viewer %1 is not valid. It has four numbers from 0 to 1 separated by \\ : "
                                                                      "x1\\y1\\x2\\y2, with x1 smaller than x2 and y1 bigger than y2.").arg(m_table->item(row, 0)->text()));
                return;
            }

            bool sliceOk = false;
            bool phaseOk = false;
            int slice = m_table->item(row, 3)->text().toInt(&sliceOk);
            int phase = m_table->item(row, 4)->text().toInt(&phaseOk);

            if (!sliceOk || !phaseOk || slice < -1 || phase < -1)
            {
                QMessageBox::warning(this, ApplicationNameString, tr("The image number and the phase of viewer %1 must be whole numbers, -1 or more.").arg(m_table->item(row, 0)->text()));
                return;
            }

            slices << slice;
            phases << phase;
        }

        m_protocol->setName(name);
        m_protocol->setProtocolsList(modalities);
        m_protocol->setPriority(m_priority->value());
        m_protocol->setStrictness(m_strict->isChecked());
        m_protocol->setAllDifferent(m_allDifferent->isChecked());

        for (int row = 0; row < m_displaySets.size(); row++)
        {
            m_displaySets[row]->setPosition(m_table->item(row, 2)->text().trimmed());
            m_displaySets[row]->setSlice(slices[row]);
            m_displaySets[row]->setPhase(phases[row]);
        }

        QString error;

        if (!HangingProtocolXMLWriter::writeFile(*m_protocol, m_path, &error))
        {
            QMessageBox::warning(this, ApplicationNameString, tr("The protocol could not be saved: %1").arg(error));
            return;
        }

        INFO_LOG(QString("The hanging protocol file %1 was changed").arg(m_path));
        accept();
    }

private:
    QString m_path;
    std::unique_ptr<HangingProtocol> m_protocol;
    QList<HangingProtocolDisplaySet*> m_displaySets;
    QLineEdit *m_name;
    QLineEdit *m_modalities;
    QDoubleSpinBox *m_priority;
    QCheckBox *m_strict;
    QCheckBox *m_allDifferent;
    QTableWidget *m_table;
};

}

QHangingProtocolsDialog::QHangingProtocolsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hanging protocols"));
    resize(680, 460);

    m_builtInDirectories << HangingProtocolsLoader::defaultsDirectory();
    m_userDirectory = Settings().getValue(CoreSettings::UserHangingProtocolsPath).toString();

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Name") << tr("Modalities") << tr("Priority") << tr("Source"));
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &QHangingProtocolsDialog::updateButtons);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this]() {
        const HangingProtocolFiles::Entry *entry = selectedEntry();

        if (entry && entry->isUser)
        {
            editSelected();
        }
    });

    m_copy = new QPushButton(tr("Copy to mine..."), this);
    m_edit = new QPushButton(tr("Edit..."), this);
    m_delete = new QPushButton(tr("Delete"), this);
    QPushButton *folder = new QPushButton(tr("Open my folder"), this);
    connect(m_copy, &QPushButton::clicked, this, &QHangingProtocolsDialog::copySelected);
    connect(m_edit, &QPushButton::clicked, this, &QHangingProtocolsDialog::editSelected);
    connect(m_delete, &QPushButton::clicked, this, &QHangingProtocolsDialog::deleteSelected);
    connect(folder, &QPushButton::clicked, this, &QHangingProtocolsDialog::openFolder);

    QLabel *note = new QLabel(tr("The protocols that come with the program cannot be changed: copy one to make your own. Your protocols are in a folder of your own and are read when the program "
                                 "starts, so restart it to use the changes."), this);
    note->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(m_copy);
    row->addWidget(m_edit);
    row->addWidget(m_delete);
    row->addStretch();
    row->addWidget(folder);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    layout->addLayout(row);
    layout->addWidget(note);
    layout->addWidget(buttons);

    reload();
}

void QHangingProtocolsDialog::reload()
{
    m_entries = HangingProtocolFiles::list(m_builtInDirectories, m_userDirectory);
    m_table->setRowCount(m_entries.size());

    for (int i = 0; i < m_entries.size(); i++)
    {
        const HangingProtocolFiles::Entry &entry = m_entries[i];
        m_table->setItem(i, 0, new QTableWidgetItem(entry.name));
        m_table->setItem(i, 1, new QTableWidgetItem(entry.modalities.join(", ")));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(entry.priority)));
        m_table->setItem(i, 3, new QTableWidgetItem(entry.isUser ? tr("Mine") : tr("Built-in")));
    }

    updateButtons();
}

const HangingProtocolFiles::Entry* QHangingProtocolsDialog::selectedEntry() const
{
    int row = m_table->currentRow();
    return row >= 0 && row < m_entries.size() && m_table->item(row, 0) && m_table->item(row, 0)->isSelected() ? &m_entries[row] : nullptr;
}

void QHangingProtocolsDialog::updateButtons()
{
    const HangingProtocolFiles::Entry *entry = selectedEntry();
    m_copy->setEnabled(entry != nullptr);
    m_edit->setEnabled(entry && entry->isUser);
    m_delete->setEnabled(entry && entry->isUser);
}

void QHangingProtocolsDialog::copySelected()
{
    const HangingProtocolFiles::Entry *entry = selectedEntry();

    if (!entry)
    {
        return;
    }

    bool accepted = false;
    QString name = QInputDialog::getText(this, tr("Copy to mine"), tr("Name of your protocol:"), QLineEdit::Normal, entry->name + " " + tr("(copy)"), &accepted);

    if (!accepted)
    {
        return;
    }

    QString error;
    QString path = HangingProtocolFiles::copyToUser(entry->path, m_userDirectory, name, &error);

    if (path.isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The protocol could not be copied: %1").arg(error));
        return;
    }

    INFO_LOG(QString("The hanging protocol %1 was copied to %2").arg(entry->path, path));
    reload();
}

void QHangingProtocolsDialog::editSelected()
{
    const HangingProtocolFiles::Entry *entry = selectedEntry();

    if (!entry || !entry->isUser)
    {
        return;
    }

    EditProtocolDialog dialog(entry->path, this);

    if (!dialog.isLoaded())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The protocol could not be read."));
        return;
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        reload();
    }
}

void QHangingProtocolsDialog::deleteSelected()
{
    const HangingProtocolFiles::Entry *entry = selectedEntry();

    if (!entry || !entry->isUser)
    {
        return;
    }

    if (QMessageBox::question(this, tr("Delete"), tr("Delete the protocol \"%1\"? It cannot be undone.").arg(entry->name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QString error;

    if (!HangingProtocolFiles::removeUserFile(entry->path, m_userDirectory, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The protocol could not be deleted: %1").arg(error));
        return;
    }

    INFO_LOG(QString("The hanging protocol file %1 was deleted").arg(entry->path));
    reload();
}

void QHangingProtocolsDialog::openFolder()
{
    QDir().mkpath(m_userDirectory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_userDirectory));
}

} // End namespace udg
