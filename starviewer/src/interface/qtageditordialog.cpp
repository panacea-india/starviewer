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

#include "qtageditordialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

#include "logging.h"
#include "starviewerapplication.h"

namespace udg {

QTagEditorDialog::QTagEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("DICOM tag editor"));
    resize(820, 600);

    QPushButton *open = new QPushButton(tr("Open..."), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_add = new QPushButton(tr("Add..."), this);
    m_save = new QPushButton(tr("Save as..."), this);
    connect(open, &QPushButton::clicked, this, &QTagEditorDialog::open);
    connect(m_remove, &QPushButton::clicked, this, &QTagEditorDialog::removeSelected);
    connect(m_add, &QPushButton::clicked, this, &QTagEditorDialog::add);
    connect(m_save, &QPushButton::clicked, this, &QTagEditorDialog::save);

    m_file = new QLabel(tr("No file"), this);
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Attribute") << tr("Name") << tr("VR") << tr("Value"));
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { edit(row); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &QTagEditorDialog::updateButtons);

    QLabel *note = new QLabel(tr("Double click a value to change it. The value is checked for the type (VR) of the attribute. The file that is open is never written over: save in another file. "
                                 "Changing UIDs or the type of image can make the file useless for other programs."), this);
    note->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(open);
    row->addWidget(m_add);
    row->addWidget(m_remove);
    row->addWidget(m_save);
    row->addStretch();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(m_file);
    layout->addWidget(m_table, 1);
    layout->addWidget(note);
    layout->addWidget(buttons);
    updateButtons();
}

void QTagEditorDialog::updateButtons()
{
    bool loaded = m_editor.isLoaded();
    int row = m_table->currentRow();
    bool selected = row >= 0 && row < m_entries.size();
    m_add->setEnabled(loaded);
    m_save->setEnabled(loaded);
    m_remove->setEnabled(loaded && selected);
}

void QTagEditorDialog::fill()
{
    m_entries = m_editor.entries();
    m_table->setRowCount(m_entries.size());

    for (int i = 0; i < m_entries.size(); i++)
    {
        const DicomTagEditor::Entry &e = m_entries[i];
        QString indent = QString(e.depth * 4, ' ');
        m_table->setItem(i, 0, new QTableWidgetItem(indent + e.path.section('/', -1)));
        m_table->setItem(i, 1, new QTableWidgetItem(indent + e.name));
        m_table->setItem(i, 2, new QTableWidgetItem(e.vr));
        QTableWidgetItem *value = new QTableWidgetItem(e.value);

        if (!e.editable)
        {
            value->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        }

        m_table->setItem(i, 3, value);
    }

    setWindowTitle(m_editor.isModified() ? tr("DICOM tag editor (changed)") : tr("DICOM tag editor"));
    updateButtons();
}

void QTagEditorDialog::open()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open a DICOM file"), QString(), tr("DICOM files (*.dcm *);;All files (*)"));

    if (path.isEmpty())
    {
        return;
    }

    QString error;

    if (!m_editor.load(path, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, error);
        return;
    }

    m_file->setText(QDir::toNativeSeparators(path));
    fill();
}

void QTagEditorDialog::edit(int row)
{
    if (row < 0 || row >= m_entries.size())
    {
        return;
    }

    const DicomTagEditor::Entry &entry = m_entries[row];

    if (!entry.editable)
    {
        QMessageBox::information(this, ApplicationNameString, tr("The value of this attribute (%1) cannot be edited here.").arg(entry.vr));
        return;
    }

    bool accepted = false;
    QString value = QInputDialog::getText(this, tr("Change the value"), tr("%1 (%2): several values are separated by a backslash").arg(entry.name, entry.vr), QLineEdit::Normal, entry.value, &accepted);

    if (!accepted)
    {
        return;
    }

    QString error;

    if (!m_editor.setValue(entry.path, value, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, error);
        return;
    }

    INFO_LOG(QString("The attribute %1 was changed in the tag editor").arg(entry.path));
    fill();
    m_table->selectRow(row);
}

void QTagEditorDialog::removeSelected()
{
    int row = m_table->currentRow();

    if (row < 0 || row >= m_entries.size())
    {
        return;
    }

    const DicomTagEditor::Entry &entry = m_entries[row];

    if (QMessageBox::question(this, tr("Remove"), tr("Remove %1 %2 (and what is in it)?").arg(entry.path.section('/', -1), entry.name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QString error;

    if (!m_editor.remove(entry.path, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, error);
        return;
    }

    fill();
}

void QTagEditorDialog::add()
{
    bool accepted = false;
    QString text = QInputDialog::getText(this, tr("Add an attribute"), tr("The attribute as group,element in hexadecimal (such as 0010,0040):"), QLineEdit::Normal, QString(), &accepted).trimmed();

    if (!accepted)
    {
        return;
    }

    QRegularExpressionMatch m = QRegularExpression("^([0-9A-Fa-f]{4}),([0-9A-Fa-f]{4})$").match(text);

    if (!m.hasMatch())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("\"%1\" is not an attribute: it is two numbers of four hexadecimal digits separated by a comma.").arg(text));
        return;
    }

    QString value = QInputDialog::getText(this, tr("Add an attribute"), tr("The value:"), QLineEdit::Normal, QString(), &accepted);

    if (!accepted)
    {
        return;
    }

    QString error;

    if (!m_editor.add(QString(), m.captured(1).toUShort(nullptr, 16), m.captured(2).toUShort(nullptr, 16), value, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, error);
        return;
    }

    fill();
}

void QTagEditorDialog::save()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save in another file"), QString(), tr("DICOM files (*.dcm)"));

    if (path.isEmpty())
    {
        return;
    }

    QString error;

    if (!m_editor.save(path, &error))
    {
        QMessageBox::warning(this, ApplicationNameString, error);
        return;
    }

    INFO_LOG(QString("A file edited in the tag editor was saved as %1").arg(path));
    QMessageBox::information(this, ApplicationNameString, tr("Saved. The file that was open is not changed."));
}

} // End namespace udg
