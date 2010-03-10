/*
 * mainwindow.hpp
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "gnc/Session.hpp"
#include "gnc/AccountItemModel.hpp"

class QAction;
class QMenu;
class QPlainTextEdit;
class QTextEdit;
class QTabWidget;
class QUndoStack;

namespace Ui
{
class MainWindow;
}

namespace gnc
{

class RecentFileMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

public slots:
    void accountItemActivated(const QModelIndex & index);
    void loadFileMaybe(const QString &fileName);
    void documentCleanStateChanged(bool clean);

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void newFile();
    void on_actionOpen_triggered();
    bool on_actionSave_triggered();
    void on_actionAbout_triggered();
    bool on_actionSave_as_triggered();
    void on_tabWidget_tabCloseRequested(int index);
    void on_tabWidget_currentChanged(int index);
    void on_textBrowser_anchorClicked(const QUrl &);
    void on_actionViewAccountTree_triggered(bool checked);
    void on_actionViewAccountList_triggered(bool checked);
    void on_actionViewWelcomepage_triggered(bool checked);
    void on_actionViewClose_triggered();
    void documentWasModified();

private:
    void createActions();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    bool maybeSave();
    void loadFile(const QString &fileName);
    bool saveFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    QString strippedName(const QString &fullFileName);
    void viewOrHideTab(bool checkedView, QWidget *widget);
    void reallyRemoveTab(int index);
    void updateWindowTitle();

    Ui::MainWindow *ui;

    QString curFile;

    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    RecentFileMenu *menuRecentFiles;
    QUndoStack *m_undoStack;

    Session m_session;
    AccountListModel *m_accountListModel;
    AccountTreeModel *m_accountTreeModel;
};

} // END namespace gnc

#endif
