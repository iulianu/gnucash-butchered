/********************************************************************\
 * qofbackend.h: api for data storage backend                       *
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/
/** @addtogroup Engine
    @{ */
/** @file qofbackend.h
    @brief api for data storage Backend
    *
    The 'backend' is a pseudo-object providing an interface between the
    engine and a persistant data store (e.g. a server, a database, or
    a file).  There are no backend functions that are 'public' to
    users of the engine.  The backend can, however, report errors to
    the GUI & other front-end users.  This file defines these errors.
    *
    @author Copyright (C) 2000-2001 Linas Vepstas <linas@linas.org>
*/

#ifndef QOF_BACKEND_H
#define QOF_BACKEND_H

/** \brief The errors that can be reported to the GUI & other front-end users
 *  \warning If you modify QofBackendError, please update 
 *   src/engine/gw-engine-spec.scm 
*/
typedef enum {
  ERR_BACKEND_NO_ERR = 0,
  ERR_BACKEND_NO_BACKEND,   /**< Backend * pointer was null the err routine 
                               or no backend handler (ENOSYS) */
  ERR_BACKEND_BAD_URL,      /**< Can't parse url */
  ERR_BACKEND_NO_SUCH_DB,   /**< the named database doesn't exist */
  ERR_BACKEND_CANT_CONNECT, /**< bad dbname/login/passwd or network failure */
  ERR_BACKEND_CONN_LOST,    /**< Lost connection to server */
  ERR_BACKEND_LOCKED,       /**< in use by another user (ETXTBSY) */
  ERR_BACKEND_READONLY,	    /**< cannot write to file/directory */
  ERR_BACKEND_TOO_NEW,      /**< file/db version newer than what we can read */
  ERR_BACKEND_DATA_CORRUPT, /**< data in db is corrupt */
  ERR_BACKEND_SERVER_ERR,   /**< error in response from server */
  ERR_BACKEND_ALLOC,        /**< internal memory allocation failure */
  ERR_BACKEND_PERM,         /**< user login successful, but no permissions 
                               to access the desired object */
  ERR_BACKEND_MODIFIED,     /**< commit of object update failed because 
                               another user has modified the object */
  ERR_BACKEND_MOD_DESTROY,  /**< commit of object update failed because 
                               another user has deleted the object */
  ERR_BACKEND_MISC,         /**< undetermined error */
  
  /* fileio errors */
  ERR_FILEIO_FILE_BAD_READ = 1000,  /**< read failed or file prematurely truncated */
  ERR_FILEIO_FILE_EMPTY,     /**< file exists, is readable, but is empty */
  ERR_FILEIO_FILE_LOCKERR,   /**< mangled locks (unspecified error) */
  ERR_FILEIO_FILE_NOT_FOUND, /**< not found / no such file */
  ERR_FILEIO_FILE_TOO_OLD,   /**< file version so old we can't read it */
  ERR_FILEIO_UNKNOWN_FILE_TYPE, /**< didn't recognize the file type */
  ERR_FILEIO_PARSE_ERROR,    /**< couldn't parse the data in the file */
  ERR_FILEIO_BACKUP_ERROR,   /**< couldn't make a backup of the file */

  /* network errors */
  ERR_NETIO_SHORT_READ = 2000,  /**< not enough bytes received */
  ERR_NETIO_WRONG_CONTENT_TYPE, /**< wrong kind of server, wrong data served */
  ERR_NETIO_NOT_GNCXML,         /**< whatever it is, we can't parse it. */
 
  /* database errors */
  ERR_SQL_MISSING_DATA = 3000,  /**< database doesn't contain expected data */
  ERR_SQL_DB_TOO_OLD,           /**< database is old and needs upgrading */
  ERR_SQL_DB_BUSY,              /**< database is busy, cannot upgrade version */
  
  /* RPC errors */
  ERR_RPC_HOST_UNK = 4000,      /**< Host unknown */
  ERR_RPC_CANT_BIND,            /**< can't bind to address */
  ERR_RPC_CANT_ACCEPT,          /**< can't accept connection */
  ERR_RPC_NO_CONNECTION,        /**< no connection to server */
  ERR_RPC_BAD_VERSION,          /**< RPC Version Mismatch */
  ERR_RPC_FAILED,               /**< Operation failed */
  ERR_RPC_NOT_ADDED,            /**< object not added */
} QofBackendError;

/** \brief Pseudo-object providing an interface between the
 * engine and a persistant data store (e.g. a server, a database, 
 * or a file).  
 *
 * There are no backend functions that are 'public' to users of the
 * engine.  The backend can, however, report errors to the GUI & other
 * front-end users.  
 */
typedef struct _QofBackend QofBackend;

/** \brief DOCUMENT ME! */
typedef void (*QofBePercentageFunc) (const char *message, double percent);

#endif /* QOF_BACKEND_H */
/**@}*/
