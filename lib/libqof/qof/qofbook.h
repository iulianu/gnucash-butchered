/********************************************************************\
 * qofbook.h -- Encapsulate all the information about a dataset.    *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/
/** @addtogroup Object
    @{ */
/** @addtogroup Book
    A QOF Book is a dataset.  It provides a single handle
    through which all the various collections of entities
    can be found.   In particular, given only the type of
    the entity, the collection can be found.

    Books also provide the 'natural' place to working with
    a storage backend, as a book can encapsulate everything
    held in storage.
    @{ */
/** @file qofbook.h
 * @brief Encapsulate all the information about a dataset.
 *
 * @author Copyright (c) 1998, 1999, 2001, 2003 Linas Vepstas <linas@linas.org>
 * @author Copyright (c) 2000 Dave Peticolas
 */

#ifndef QOF_BOOK_H
#define QOF_BOOK_H

#include "qofid.h"
#include "kvp_frame.h"

/** @brief Encapsulates all the information about a dataset
 * manipulated by QOF.  This is the top-most structure
 * used for anchoring data.
 */

/** Lookup an entity by guid, returning pointer to the entity */
#define QOF_BOOK_LOOKUP_ENTITY(book,guid,e_type,c_type) ({  \
  QofEntity *val = NULL;                                    \
  if (guid && book) {                                       \
    QofCollection *col;                                     \
    col = qof_book_get_collection (book, e_type);           \
    val = qof_collection_lookup_entity (col, guid);         \
  }                                                         \
  (c_type *) val;                                           \
})

/** \brief QofBook reference */
typedef struct _QofBook       QofBook;

/** GList of QofBook */
typedef GList                 QofBookList;

typedef void (*QofBookFinalCB) (QofBook *, gpointer key, gpointer user_data);

/** Register the book object with the QOF object system. */
gboolean qof_book_register (void);

/** Allocate, initialise and return a new QofBook.  Books contain references
 *  to all of the top-level object containers. */
QofBook * qof_book_new (void);

/** End any editing sessions associated with book, and free all memory
    associated with it. */
void      qof_book_destroy (QofBook *book);

/** Close a book to editing.

It is up to the application to check this flag,
and once marked closed, books cannnot be marked as open.
*/
void qof_book_mark_closed (QofBook *book);

/** Return The table of entities of the given type.
 *
 *  When an object's constructor calls qof_instance_init(), a
 *  reference to the object is stored in the book.  The book stores
 *  all the references to initialized instances, sorted by type.  This
 *  function returns a collection of the references for the specified
 *  type.
 *
 *  If the collection doesn't yet exist for the indicated type,
 *  it is created.  Thus, this routine is gaurenteed to return
 *  a non-NULL value.  (Unless the system malloc failed (out of
 *  memory) in which case what happens??).
 */
QofCollection  * qof_book_get_collection (QofBook *, QofIdType);

/** Invoke the indicated callback on each collection in the book. */
typedef void (*QofCollectionForeachCB) (QofCollection *, gpointer user_data);
void qof_book_foreach_collection (QofBook *, QofCollectionForeachCB, gpointer);

/** Return The kvp data for the book.
 *  Note that the book KVP data is persistent, and is stored/retrieved
 *  from the file/database.  Thus, the book KVP is the correct place to
 *  store data that needs to be persistent accross sessions (or shared
 *  between multiple users).  To store application runtime data, use
 *  qof_book_set_data() instead.
 */
#define qof_book_get_slots(book) qof_instance_get_slots(QOF_INSTANCE(book))

/** The qof_book_set_data() allows arbitrary pointers to structs
 *    to be stored in QofBook. This is the "preferred" method for
 *    extending QofBook to hold new data types.  This is also
 *    the ideal location to store other arbitrary runtime data
 *    that the application may need.
 *
 *    The book data differs from the book KVP in that the contents
 *    of the book KVP are persistent (are saved and restored to file
 *    or database), whereas the data pointers exist only at runtime.
 */
void qof_book_set_data (QofBook *book, const gchar *key, gpointer data);

/** Same as qof_book_set_data(), except that the callback will be called
 *  when the book is destroyed.  The argument to the callback will be
 *  the book followed by the data pointer.
 */
void qof_book_set_data_fin (QofBook *book, const gchar *key, gpointer data, 
                            QofBookFinalCB);

/** Retrieves arbitrary pointers to structs stored by qof_book_set_data. */
gpointer qof_book_get_data (QofBook *book, const gchar *key);

/** Is the book shutting down? */
gboolean qof_book_shutting_down (QofBook *book);

/** qof_book_not_saved() will return TRUE if any
 *    data in the book hasn't been saved to long-term storage.
 *    (Actually, that's not quite true.  The book doesn't know
 *    anything about saving.  Its just that whenever data is modified,
 *    the 'dirty' flag is set.  This routine returns the value of the
 *    'dirty' flag.  Its up to the backend to periodically reset this
 *    flag, when it actually does save the data.)
 */
gboolean qof_book_not_saved (QofBook *book);

/** The qof_book_mark_saved() routine marks the book as having been
 *    saved (to a file, to a database). Used by backends to mark the
 *    notsaved flag as FALSE just after loading.  Can also be used 
 *    by the frontend when the used has said to abandon any changes.
 */
void qof_book_mark_saved(QofBook *book);
void qof_book_print_dirty (QofBook *book);

/** Call this function when you change the book kvp, to make sure the book
 * is marked 'dirty'. */
void qof_book_kvp_changed (QofBook *book);

/** The qof_book_equal() method returns TRUE if books are equal.
 * XXX this routine is broken, and does not currently compare data.
 */
gboolean qof_book_equal (QofBook *book_1, QofBook *book_2);

/** This will 'get and increment' the named counter for this book.
 * The return value is -1 on error or the incremented counter.
 */
gint64 qof_book_get_counter (QofBook *book, const char *counter_name);

/** deprecated */
#define qof_book_get_guid(X) qof_entity_get_guid (QOF_ENTITY(X))

#endif /* QOF_BOOK_H */
/** @} */
/** @} */
