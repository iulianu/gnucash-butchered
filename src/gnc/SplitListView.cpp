/*
 * SplitListView.cpp
 * Copyright (C) 2010 Christian Stimming
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include "SplitListView.hpp"

#include "engine/gnc-event.h" // for GNC_EVENT_ITEM_ADDED
#include "gncmm/Account.hpp"
#include "gnc/SplitListModel.hpp"
#include "gnc/AccountSelectionDelegate.hpp"

#include <QtGui/QAbstractItemDelegate>
#include <QUndoStack>
#include <QDebug>

namespace gnc
{

SplitListView::SplitListView(Glib::RefPtr<Account> account, QUndoStack* undoStack, QWidget* parent)
        : base_class(parent)
        , m_account(account)
        , m_eventWrapperAccount(*this, &SplitListView::accountEvent)
        , m_eventWrapperBook(*this, &SplitListView::bookEvent)
{
    // Create a model that is used in this view
    SplitListModel *smodel = new SplitListModel(account, undoStack, this);
    setModel(smodel);
    connect(this, SIGNAL(editorClosed(const QModelIndex&,QAbstractItemDelegate::EndEditHint)),
            smodel, SLOT(editorClosed(const QModelIndex&,QAbstractItemDelegate::EndEditHint)));

    // Create a separate delegate only for the Account colum
    QAbstractItemDelegate *accountDelegate = new AccountSelectionDelegate(this);
    setItemDelegateForColumn(SplitListModel::COLUMN_ACCOUNT, accountDelegate);

    // Appearance of this view
    setAlternatingRowColors(true);

    // Move the focus to the latest line
    scrollToBottom();
    if (model()->rowCount() > 0)
        setCurrentIndex(model()->index(model()->rowCount() - 1, 0));
}

void SplitListView::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
    base_class::closeEditor(editor, hint);
    //qDebug() << "closeEditor, row=" << currentIndex().row() << "hint=" << hint;
    QModelIndex index = currentIndex();
    if (hint != QAbstractItemDelegate::NoHint)
        Q_EMIT editorClosed(index, hint);
    if (index.isValid()
            && hint == QAbstractItemDelegate::SubmitModelCache
            && index.row() < model()->rowCount() - 1)
    {
        switch (index.column())
        {
        case SplitListModel::COLUMN_INCREASE:
        case SplitListModel::COLUMN_DECREASE:
            setCurrentIndex(model()->index(index.row() + 1,
                                           SplitListModel::COLUMN_DATE,
                                           index.parent()));
            break;
        default:
            break;
        }
    }
}

void SplitListView::accountEvent( ::Account* v, QofEventId event_type)
{
    if (v != m_account->gobj())
        return;
    //qDebug() << "SplitListView::accountEvent, id=" << qofEventToString(event_type);
    switch (event_type)
    {
    case QOF_EVENT_DESTROY:
        // This account seems to be getting deleted - better close
        // this view.
        deleteLater();
        break;
    default:
        break;
    }
}

void SplitListView::bookEvent( ::QofBook* v, QofEventId event_type)
{
    qDebug() << "SplitListView::bookEvent, id=" << qofEventToString(event_type);

    switch (event_type)
    {
    case QOF_EVENT_DESTROY:
        // The book is being deleted - better close this view ASAP!
        deleteLater();
        break;
    default:
        break;
    }
}

}
