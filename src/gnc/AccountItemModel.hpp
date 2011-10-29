/*
 * AccountItemModel.hpp
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

#ifndef GNC_ACCOUNTITEMMODEL_HPP
#define GNC_ACCOUNTITEMMODEL_HPP

#include "config.h"
#include "gncmm/Account.hpp"
#include "gnc/QofEventWrapper.hpp"
#include "gnc/conv.hpp"
#include "gnc/metatype.hpp"

#include <QAbstractItemModel>

namespace gnc
{

/** This is the data model for an account tree.
 *
 * It was written following the "Simple Tree Model Example" in the qt4
 * documentation. In particular, the trick is that each returned
 * QModelIndex of our model implementation which is valid will have a
 * ::Account* pointer in its QModelIndex::internalPointer() member,
 * which can then be used in the data() method and the other
 * methods. This trick works quite nice because our C structures
 * ::Account* already have all the methods to iterate the tree back
 * and forth, and our C++ wrapper gnc::Account doesn't add any
 * functionality except for some nicer method names.
 */
class AccountTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    AccountTreeModel(Glib::RefPtr<Account> rootaccount, QObject *parent = 0);

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

protected:
    Glib::RefPtr<Account> m_root;
};

typedef QList< ::Account*> AccountQList;
inline AccountQList accountFromGList(GList *glist)
{
    return from_glist<AccountQList>(glist);
}

/** Specialization of the account tree model for when all accounts
 * should be viewed as a flat list instead of a tree. Only the index()
 * and parent() functions had to be overridden - quite easy. */
class AccountListModel : public AccountTreeModel
{
    Q_OBJECT
public:
    typedef AccountTreeModel base_class;
    AccountListModel(Glib::RefPtr<Account> rootaccount, QObject *parent = 0);

    int rowCount(const QModelIndex& parent = QModelIndex()) const { return m_list.size(); }

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;

    QModelIndex parent(const QModelIndex &index) const { return QModelIndex(); }

    int indexOf(AccountQList::value_type value) const { return m_list.indexOf(value); }
    const AccountQList::value_type at(int i) const { return m_list.at(i); }

public Q_SLOTS:
    void accountEvent( ::Account* v, QofEventId event_type);

private:
    void recreateCache();

    AccountQList m_list;
    QofEventWrapper<AccountListModel, ::Account*> m_eventWrapperAccount;
};

/** Specialization of the account list model that only shows the
 * "Account Full Name" in one single column.
 */
class AccountListNamesModel : public AccountListModel
{
    Q_OBJECT
public:
    typedef AccountListModel base_class;
    AccountListNamesModel(Glib::RefPtr<Account> rootaccount, QObject *parent = 0)
            : base_class(rootaccount, parent)
    {}
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
};

} // END namespace gnc

#endif
