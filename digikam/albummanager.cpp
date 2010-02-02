/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-06-15
 * Description : Albums manager interface.
 *
 * Copyright (C) 2004 by Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Copyright (C) 2006-2009 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * Copyright (C) 2006-2009 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "albummanager.moc"

// C ANSI includes

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
}

// C++ includes

#include <clocale>
#include <cstdlib>
#include <cstdio>
#include <cerrno>

// Qt includes

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QHash>
#include <QList>
#include <QLabel>
#include <QMultiHash>
#include <QRadioButton>
#include <QTextCodec>
#include <QTimer>

// KDE includes

#include <kconfig.h>
#include <klocale.h>
#include <kdeversion.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kio/netaccess.h>
#include <kio/global.h>
#include <kio/job.h>
#include <kdirwatch.h>
#include <kconfiggroup.h>
#include <kwindowsystem.h>
#include <kdebug.h>

// Local includes

#include "album.h"
#include "albumdb.h"
#include "albumsettings.h"
#include "collectionmanager.h"
#include "collectionlocation.h"
#include "databaseaccess.h"
#include "thumbnaildatabaseaccess.h"
#include "databaseurl.h"
#include "databaseparameters.h"
#include "databasethumbnailinfoprovider.h"
#include "databasewatch.h"
#include "dio.h"
#include "imagelister.h"
#include "scancontroller.h"
#include "setup.h"
#include "thumbnailloadthread.h"
#include "upgradedb_sqlite2tosqlite3.h"
#include "config-digikam.h"
#include "setupcollections.h"
#include "digikamapp.h"

namespace Digikam
{

class PAlbumPath
{
public:

    PAlbumPath()
        : albumRootId(-1)
    {
    }

    PAlbumPath(int albumRootId, const QString& albumPath)
       : albumRootId(albumRootId), albumPath(albumPath)
    {
    }

    PAlbumPath(PAlbum *album)
    {
        if (album->isRoot())
        {
            albumRootId = -1;
        }
        else
        {
            albumRootId = album->albumRootId();
            albumPath   = album->albumPath();
        }
    }

    bool operator==(const PAlbumPath& other) const
    {
        return other.albumRootId == albumRootId && other.albumPath == albumPath;
    }

    int     albumRootId;
    QString albumPath;
};

uint qHash(const PAlbumPath& id)
{
    return ::qHash(id.albumRootId) ^ ::qHash(id.albumPath);
}

class AlbumManagerPriv
{

public:

    AlbumManagerPriv()
    {
        changed            = false;
        hasPriorizedDbPath = false;
        dateListJob        = 0;
        albumListJob       = 0;
        tagListJob         = 0;
        dirWatch           = 0;
        rootPAlbum         = 0;
        rootTAlbum         = 0;
        rootDAlbum         = 0;
        rootSAlbum         = 0;
        currentAlbum       = 0;
        changingDB         = false;
        scanPAlbumsTimer   = 0;
        scanTAlbumsTimer   = 0;
        scanSAlbumsTimer   = 0;
        scanDAlbumsTimer   = 0;
        updatePAlbumsTimer = 0;
        albumItemCountTimer= 0;
        tagItemCountTimer  = 0;
    }

    bool                        changed;
    bool                        hasPriorizedDbPath;

    QString                     dbType;
    QString                     dbName;
    QString                     dbHostName;
    int                         dbPort;
    bool                        dbInternalServer;

    QList<QDateTime>            dbPathModificationDateList;
    QList<QString>              dirWatchBlackList;

    KIO::TransferJob*           albumListJob;
    KIO::TransferJob*           dateListJob;
    KIO::TransferJob*           tagListJob;

    KDirWatch*                  dirWatch;

    PAlbum*                     rootPAlbum;
    TAlbum*                     rootTAlbum;
    DAlbum*                     rootDAlbum;
    SAlbum*                     rootSAlbum;

    QHash<int,Album *>          allAlbumsIdHash;
    QHash<PAlbumPath, PAlbum*>  albumPathHash;
    QHash<int, PAlbum*>         albumRootAlbumHash;

    QMultiHash<Album*, Album**> guardedPointers;

    Album*                      currentAlbum;

    bool                        changingDB;
    QTimer*                     scanPAlbumsTimer;
    QTimer*                     scanTAlbumsTimer;
    QTimer*                     scanSAlbumsTimer;
    QTimer*                     scanDAlbumsTimer;
    QTimer*                     updatePAlbumsTimer;
    QTimer*                     albumItemCountTimer;
    QTimer*                     tagItemCountTimer;
    QSet<int>                   changedPAlbums;


    QList<QDateTime> buildDirectoryModList(const QFileInfo& dbFile)
    {
        // retrieve modification dates
        QList<QDateTime> modList;
        QFileInfoList fileInfoList = dbFile.dir().entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        // build list
        foreach (const QFileInfo& info, fileInfoList)
        {
            // ignore digikam4.db and journal and other temporary files
            if (!dirWatchBlackList.contains(info.fileName()))
            {
                modList << info.lastModified();
            }
        }
        return modList;
    }

    QString labelForAlbumRootAlbum(const CollectionLocation& location)
    {
        QString label = location.label();
        if (label.isEmpty())
            label = location.albumRootPath();
        return label;
    }
};

class ChangingDB
{
public:

    ChangingDB(AlbumManagerPriv *d)
        : d(d)
    {
        d->changingDB = true;
    }
    ~ChangingDB()
    {
        d->changingDB = false;
    }
    AlbumManagerPriv* const d;
};

class AlbumManagerCreator { public: AlbumManager object; };
K_GLOBAL_STATIC(AlbumManagerCreator, creator)

// A friend-class shortcut to circumvent accessing this from within the destructor
AlbumManager *AlbumManager::internalInstance = 0;

AlbumManager* AlbumManager::instance()
{
    return &creator->object;
}

AlbumManager::AlbumManager()
            : d(new AlbumManagerPriv)
{
    internalInstance = this;

    // these operations are pretty fast, no need for long queuing
    d->scanPAlbumsTimer = new QTimer(this);
    d->scanPAlbumsTimer->setInterval(50);
    d->scanPAlbumsTimer->setSingleShot(true);
    connect(d->scanPAlbumsTimer, SIGNAL(timeout()), this, SLOT(scanPAlbums()));

    d->scanTAlbumsTimer = new QTimer(this);
    d->scanTAlbumsTimer->setInterval(50);
    d->scanTAlbumsTimer->setSingleShot(true);
    connect(d->scanTAlbumsTimer, SIGNAL(timeout()), this, SLOT(scanTAlbums()));

    d->scanSAlbumsTimer = new QTimer(this);
    d->scanSAlbumsTimer->setInterval(50);
    d->scanSAlbumsTimer->setSingleShot(true);
    connect(d->scanSAlbumsTimer, SIGNAL(timeout()), this, SLOT(scanSAlbums()));

    d->updatePAlbumsTimer = new QTimer(this);
    d->updatePAlbumsTimer->setInterval(50);
    d->updatePAlbumsTimer->setSingleShot(true);
    connect(d->updatePAlbumsTimer, SIGNAL(timeout()), this, SLOT(updateChangedPAlbums()));

    // this operation is much more expensive than the other scan methods
    d->scanDAlbumsTimer = new QTimer(this);
    d->scanDAlbumsTimer->setInterval(5000);
    d->scanDAlbumsTimer->setSingleShot(true);
    connect(d->scanDAlbumsTimer, SIGNAL(timeout()), this, SLOT(scanDAlbums()));

    // moderately expensive
    d->albumItemCountTimer = new QTimer(this);
    d->albumItemCountTimer->setInterval(1000);
    d->albumItemCountTimer->setSingleShot(true);
    connect(d->albumItemCountTimer, SIGNAL(timeout()), this, SLOT(getAlbumItemsCount()));

    // more expensive
    d->tagItemCountTimer = new QTimer(this);
    d->tagItemCountTimer->setInterval(2500);
    d->tagItemCountTimer->setSingleShot(true);
    connect(d->tagItemCountTimer, SIGNAL(timeout()), this, SLOT(getTagItemsCount()));
}

AlbumManager::~AlbumManager()
{
    delete d->rootPAlbum;
    delete d->rootTAlbum;
    delete d->rootDAlbum;
    delete d->rootSAlbum;

    internalInstance = 0;
    delete d;
}

void AlbumManager::cleanUp()
{
    // This is what we prefer to do before KApplication destruction

    if (d->dateListJob)
    {
        d->dateListJob->kill();
        d->dateListJob = 0;
    }

    if (d->albumListJob)
    {
        d->albumListJob->kill();
        d->albumListJob = 0;
    }

    if (d->tagListJob)
    {
        d->tagListJob->kill();
        d->tagListJob = 0;
    }

    delete d->dirWatch;
    d->dirWatch = 0;
}

bool AlbumManager::databaseEqual(const QString& dbType, const QString& dbName, const QString& dbHostName, int dbPort, bool dbInternalServer) const
{
    return d->dbType == dbType
           && d->dbName == dbName
           && d->dbHostName == dbHostName
           && d->dbPort == dbPort
           && d->dbInternalServer == dbInternalServer;
}

static bool moveToBackup(const QFileInfo& info)
{
    if (info.exists())
    {
        QFileInfo backup(info.dir(), info.fileName() + "-backup-" + QDateTime::currentDateTime().toString(Qt::ISODate));
        KIO::Job *job = KIO::file_move(info.filePath(), backup.filePath(), -1, KIO::Overwrite | KIO::HideProgressInfo);
        if (!KIO::NetAccess::synchronousRun(job, 0))
        {
            KMessageBox::error(0, i18n("Failed to backup the existing database file (\"%1\")."
                                       "Refusing to replace file without backup, using the existing file.",
                                        info.filePath()));
            return false;
        }
    }
    return true;
}

static bool copyToNewLocation(const QFileInfo& oldFile, const QFileInfo& newFile, const QString otherMessage = QString())
{
    QString message = otherMessage;
    if (message.isNull())
        message = i18n("Failed to copy the old database file (\"%1\") "
                       "to its new location (\"%2\"). "
                       "Starting with an empty database.",
                       oldFile.filePath(), newFile.filePath());

    KIO::Job *job = KIO::file_copy(oldFile.filePath(), newFile.filePath(), -1, KIO::Overwrite /*| KIO::HideProgressInfo*/);
    if (!KIO::NetAccess::synchronousRun(job, 0))
    {
        KMessageBox::error(0, message);
        return false;
    }
    return true;
}

void AlbumManager::checkDatabaseDirsAfterFirstRun(const QString& dbPath, const QString& albumPath)
{
    // for bug #193522
    QDir newDir(dbPath);
    QDir albumDir(albumPath);
    DatabaseParameters newParams = DatabaseParameters::parametersForSQLiteDefaultFile(newDir.path());
    QFileInfo digikam4DB(newParams.SQLiteDatabaseFile());

    if (!digikam4DB.exists())
    {
        QFileInfo digikam3DB(newDir, "digikam3.db");
        QFileInfo digikamVeryOldDB(newDir, "digikam.db");

        if (digikam3DB.exists() || digikamVeryOldDB.exists())
        {
            KGuiItem startFresh(i18n("Create New Database"), "document-new");
            KGuiItem upgrade(i18n("Upgrade Database"), "view-refresh");
            int result = KMessageBox::warningYesNo(0,
                                i18n("<p>You have chosen the folder \"%1\" as the place to store the database. "
                                     "A database file from an older version of digiKam is found in this folder.</p> "
                                     "<p>Would you like to upgrade the old database file - confirming "
                                     "that this database file was indeed created for the pictures located in the folder \"%2\" - "
                                     "or ignore the old file and start with a new database?</p> ",
                                    newDir.path(), albumDir.path()),
                                i18n("Database Folder"),
                                upgrade, startFresh);

            if (result == KMessageBox::Yes)
            {
                // SchemaUpdater expects Album Path to point to the album root of the 0.9 db file.
                // Restore this situation.
                KSharedConfigPtr config = KGlobal::config();
                KConfigGroup group = config->group("Album Settings");
                group.writeEntry("Album Path", albumDir.path());
                group.sync();
            }
            else if (result == KMessageBox::No)
            {
                moveToBackup(digikam3DB);
                moveToBackup(digikamVeryOldDB);
            }
        }
    }
}

void AlbumManager::changeDatabase(const QString& dbType, const QString& dbName, const QString& dbThumbnailsName, const QString& dbHostName, int dbPort, const QString &dbUser, const QString &dbPasswd, const QString &dbConnectOptions, bool internalServer)
{
    // if there is no file at the new place, copy old one
    DatabaseParameters params = DatabaseAccess::parameters();

    // New database type SQLITE
    if (dbType == "SQLITE"){
        QDir newDir(dbName);
        QFileInfo newFile(newDir, QString("digikam4.db"));

        if (!newFile.exists())
            {
                QFileInfo digikam3DB(newDir, "digikam3.db");
                QFileInfo digikamVeryOldDB(newDir, "digikam.db");

                if (digikam3DB.exists() || digikamVeryOldDB.exists())
                {
                    KGuiItem copyCurrent(i18n("Copy Current Database"), "edit-copy");
                    KGuiItem startFresh(i18n("Create New Database"), "document-new");
                    KGuiItem upgrade(i18n("Upgrade Database"), "view-refresh");
                    int result = -1;
                    if (params.isSQLite())
                    {
                        result = KMessageBox::warningYesNoCancel(0,
                                            i18n("<p>You have chosen the folder \"%1\" as the new place to store the database. "
                                                 "A database file from an older version of digiKam is found in this folder.</p> "
                                                "<p>Would you like to upgrade the old database file, start with a new database, "
                                                "or copy the current database to this location and continue using it?</p> ",
                                                newDir.path()),
                                            i18n("New database folder"),
                                            upgrade, startFresh, copyCurrent);
                    }else{
                        result = KMessageBox::warningYesNo(0,
                                            i18n("<p>You have chosen the folder \"%1\" as the new place to store the database. "
                                                 "A database file from an older version of digiKam is found in this folder.</p> "
                                                "<p>Would you like to upgrade the old database file or start with a new database?</p>",
                                                newDir.path()),
                                            i18n("New database folder"),
                                            upgrade, startFresh);
                    }

                    if (result == KMessageBox::Yes)
                    {
                        // SchemaUpdater expects Album Path to point to the album root of the 0.9 db file.
                        // Restore this situation.
                        KSharedConfigPtr config = KGlobal::config();
                        KConfigGroup group = config->group("Album Settings");
                        group.writeEntry("Album Path", newDir.path());
                        group.sync();
                    }
                    else if (result == KMessageBox::No)
                    {
                        moveToBackup(digikam3DB);
                        moveToBackup(digikamVeryOldDB);
                    }
                    else if (result == KMessageBox::Cancel)
                    {
                        QDir oldDir(d->dbName);
                        QFileInfo oldFile(params.SQLiteDatabaseFile());
                        copyToNewLocation(oldFile, newFile, i18n("Failed to copy the old database file (\"%1\") "
                                                                 "to its new location (\"%2\"). "
                                                                 "Trying to upgrade old databases.",
                                                                 oldFile.filePath(), newFile.filePath()));
                    }
                }
                else
                {
                    int result = KMessageBox::Yes;
                    if (params.isSQLite())
                    {
                    KGuiItem copyCurrent(i18n("Copy Current Database"), "edit-copy");
                    KGuiItem startFresh(i18n("Create New Database"), "document-new");
                        result = KMessageBox::warningYesNo(0,
                                            i18n("<p>You have chosen the folder \"%1\" as the new place to store the database.</p>"
                                                "<p>Would you like to copy the current database to this location "
                                                "and continue using it, or start with a new database?</p> ",
                                                newDir.path()),
                                            i18n("New database folder"),
                                            startFresh, copyCurrent);
                    }

                    if (result == KMessageBox::No)
                    {
                        QDir oldDir(d->dbName);
                        QFileInfo oldFile(params.SQLiteDatabaseFile());
                        copyToNewLocation(oldFile, newFile);
                    }
                }
            }
            else
            {
                int result = KMessageBox::No;
                if (params.isSQLite())
                {
                    KGuiItem replaceItem(i18n("Copy Current Database"), "edit-copy");
                    KGuiItem useExistingItem(i18n("Use Existing File"), "document-open");
                    result = KMessageBox::warningYesNo(0,
                                        i18n("<p>You have chosen the folder \"%1\" as the new place to store the database. "
                                             "There is already a database file in this location.</p> "
                                             "<p>Would you like to use this existing file as the new database, or remove it "
                                             "and copy the current database to this place?</p> ",
                                              newDir.path()),
                                        i18n("New database folder"),
                                        replaceItem, useExistingItem);
                }
                if (result == KMessageBox::Yes)
                {
                    // first backup
                    if (moveToBackup(newFile))
                    {
                        QDir oldDir(d->dbName);
                        QFileInfo oldFile(params.SQLiteDatabaseFile());

                        // then copy
                       copyToNewLocation(oldFile, newFile);
                    }
                }
            }
    }

    if (setDatabase(dbType, dbName, dbThumbnailsName, dbHostName, dbPort, dbUser, dbPasswd, dbConnectOptions, internalServer, false))
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        startScan();
        QApplication::restoreOverrideCursor();
        ScanController::instance()->completeCollectionScan();
    }
}

bool AlbumManager::setDatabase(const QString dbType, const QString dbName, const QString dbThumbnailsName, const QString dbHostName, int dbPort, const QString dbUser, const QString dbPasswd, const QString dbConnectOptions, bool internalServer, bool priority, const QString suggestedAlbumRoot)
{
    // ensure, embedded database is loaded
    if (internalServer)
    {
        DigikamApp::instance()->startInternalDatabase();
    }else
    {
        DigikamApp::instance()->stopInternalDatabase();
    }

    // This is to ensure that the setup does not overrule the command line.
    // Replace with a better solution?
    /*
    if (priority)
    {
        d->hasPriorizedDbPath = true;
    }
    else if (d->hasPriorizedDbPath && !d->dbName.isNull())
    {
        // ignore change without priority
        // true means, don't exit()
        return true;
    }
    */

    // shutdown possibly running collection scans. Must call resumeCollectionScan further down.
    ScanController::instance()->cancelAllAndSuspendCollectionScan();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    d->dbType     = dbType;
    d->dbName     = dbName;
    d->dbHostName = dbHostName;
    d->dbPort     = dbPort;
    d->dbInternalServer = internalServer;

    d->changed = true;

    disconnect(CollectionManager::instance(), 0, this, 0);
    if (DatabaseAccess::databaseWatch())
        disconnect(DatabaseAccess::databaseWatch(), 0, this, 0);
    d->dbPathModificationDateList.clear();

    if (d->dateListJob)
    {
        d->dateListJob->kill();
        d->dateListJob = 0;
    }

    if (d->albumListJob)
    {
        d->albumListJob->kill();
        d->albumListJob = 0;
    }

    if (d->tagListJob)
    {
        d->tagListJob->kill();
        d->tagListJob = 0;
    }

    delete d->dirWatch;
    d->dirWatch = 0;

    QDBusConnection::sessionBus().disconnect(QString(), QString(), "org.kde.KDirNotify", "FileMoved", 0, 0);
    QDBusConnection::sessionBus().disconnect(QString(), QString(), "org.kde.KDirNotify", "FilesAdded", 0, 0);
    QDBusConnection::sessionBus().disconnect(QString(), QString(), "org.kde.KDirNotify", "FilesRemoved", 0, 0);

    d->currentAlbum = 0;
    emit signalAlbumCurrentChanged(0);
    emit signalAlbumsCleared();

    d->albumPathHash.clear();
    d->allAlbumsIdHash.clear();
    d->albumRootAlbumHash.clear();

    // deletes all child albums as well
    delete d->rootPAlbum;
    delete d->rootTAlbum;
    delete d->rootDAlbum;
    delete d->rootSAlbum;

    d->rootPAlbum = 0;
    d->rootTAlbum = 0;
    d->rootDAlbum = 0;
    d->rootSAlbum = 0;

    // -- Database initialization -------------------------------------------------

    QString databaseName = dbName;
    QString thumbnailDatabaseName = dbThumbnailsName;

    // SQLite specifics
    if (AlbumSettings::instance()->getDatabaseType().isEmpty())
        AlbumSettings::instance()->setDatabaseType("QSQLITE");

    if (AlbumSettings::instance()->getDatabaseType() == "QSQLITE")
    {
        if (AlbumSettings::instance()->getDatabaseName().isEmpty())
            databaseName = dbName;
        QString databasePath = databaseName;
        databaseName = QDir::cleanPath(databasePath + '/' + "digikam4.db");
        thumbnailDatabaseName = QDir::cleanPath(databasePath + '/' + "thumbnails-digikam.db");
    }
    kDebug(50003) << "Using database settings: DBType ["<< dbType <<"] DBName ["<< dbName <<"] DBThumbnailsName ["<< dbThumbnailsName <<"] DBHostName ["<< dbHostName <<"] DBPort ["<< dbPort <<"] DBUser ["<< dbUser <<"] DBConnectOptions ["<< dbConnectOptions <<"]";
    DatabaseAccess::setParameters(DatabaseParameters::parametersFromConfig(dbType,
                                                                           databaseName,
                                                                           dbHostName,
                                                                           dbPort,
                                                                           dbUser,
                                                                           dbPasswd,
                                                                           dbConnectOptions),
                                                                           DatabaseAccess::MainApplication);

    DatabaseGUIErrorHandler *handler = new DatabaseGUIErrorHandler(DatabaseAccess::parameters());
    DatabaseAccess::initDatabaseErrorHandler(handler);

    // still suspended from above
    ScanController::instance()->resumeCollectionScan();

    ScanController::Advice advice = ScanController::instance()->databaseInitialization();

    QApplication::restoreOverrideCursor();

    switch (advice)
    {
        case ScanController::Success:
            break;
        case ScanController::ContinueWithoutDatabase:
        {
            QString errorMsg = DatabaseAccess().lastError();
            if (errorMsg.isEmpty())
            {
                KMessageBox::error(0, i18n("<p>Failed to open the database. "
                                        "</p><p>You cannot use digiKam without a working database. "
                                        "digiKam will attempt to start now, but it will <b>not</b> be functional. "
                                        "Please check the database settings in the <b>configuration menu</b>.</p>"
                                        ));
            }
            else
            {
                KMessageBox::error(0, i18n("<p>Failed to open the database. Error message from database:</p>"
                                           "<p><b>%1</b></p>"
                                           "</p><p>You cannot use digiKam without a working database. "
                                           "digiKam will attempt to start now, but it will <b>not</b> be functional. "
                                           "Please check the database settings in the <b>configuration menu</b>.</p>",
                                           errorMsg));
            }
            return true;
        }
        case ScanController::AbortImmediately:
            return false;
    }

    /*
    bool abort=false;
    do
    {
        if (advice==ScanController::ContinueWithoutDatabase)
        {
            QApplication::restoreOverrideCursor();
            int result = KMessageBox::warningYesNoCancel(0, i18n("Error while opening the database."
                                                                "<p>Error was: %1</p>",
                                                                DatabaseAccess().lastError()),
                                                            i18n("Database Connection Error"),
                                                        KGuiItem(i18n("Retry")),
                                                        KGuiItem(i18n("DB Setup")),
                                                        KGuiItem(i18n("Abort")));

            if (result == KMessageBox::Cancel)
            {
                advice = ScanController::AbortImmediately;
            }else if (result == KMessageBox::No)
            {
                if ( Setup::execSinglePage(0, Setup::DatabasePage) == false)
                {
                    abort=true;
                }
            }
            QApplication::setOverrideCursor(Qt::WaitCursor);
        }
        else
        {
            // In this case the user can't do anything and we must abort
            abort=true;
        }
    } while(abort==false && advice==ScanController::ContinueWithoutDatabase);
    */
    // -- Locale Checking ---------------------------------------------------------

    QString currLocale(QTextCodec::codecForLocale()->name());
    QString dbLocale = DatabaseAccess().db()->getSetting("Locale");

    // guilty until proven innocent
    bool localeChanged = true;

    if (dbLocale.isNull())
    {
        kDebug() << "No locale found in database";

        // Copy an existing locale from the settings file (used < 0.8)
        // to the database.
        KSharedConfig::Ptr config = KGlobal::config();
        KConfigGroup group = config->group("General Settings");
        if (group.hasKey("Locale"))
        {
            kDebug() << "Locale found in configfile";
            dbLocale = group.readEntry("Locale", QString());

            // this hack is necessary, as we used to store the entire
            // locale info LC_ALL (for eg: en_US.UTF-8) earlier,
            // we now save only the encoding (UTF-8)

            QString oldConfigLocale = ::setlocale(0, 0);

            if (oldConfigLocale == dbLocale)
            {
                dbLocale = currLocale;
                localeChanged = false;
                DatabaseAccess().db()->setSetting("Locale", dbLocale);
            }
        }
        else
        {
            kDebug() << "No locale found in config file";
            dbLocale = currLocale;

            localeChanged = false;
            DatabaseAccess().db()->setSetting("Locale",dbLocale);
        }
    }
    else
    {
        if (dbLocale == currLocale)
            localeChanged = false;
    }

    if (localeChanged)
    {
        // TODO it would be better to replace all yes/no confirmation dialogs with ones that has custom
        // buttons that denote the actions directly, i.e.:  ["Ignore and Continue"]  ["Adjust locale"]
        int result =
            KMessageBox::warningYesNo(0,
                                      i18n("Your locale has changed since this "
                                           "album was last opened.\n"
                                           "Old Locale : %1, New Locale : %2\n"
                                           "If you have recently changed your locale, you need not be concerned.\n"
                                           "Please note that if you switched to a locale "
                                           "that does not support some of the filenames in your collection, "
                                           "these files may no longer be found in the collection. "
                                           "If you are sure that you want to "
                                           "continue, click 'Yes'. "
                                           "Otherwise, click 'No' and correct your "
                                           "locale setting before restarting digiKam.",
                                           dbLocale, currLocale));
        if (result != KMessageBox::Yes)
            exit(0);

        DatabaseAccess().db()->setSetting("Locale",currLocale);
    }

    // -- UUID Checking ---------------------------------------------------------

    QList<CollectionLocation> disappearedLocations = CollectionManager::instance()->checkHardWiredLocations();
    foreach (const CollectionLocation &loc, disappearedLocations)
    {
        QString locDescription;
        QStringList candidateIds, candidateDescriptions;
        CollectionManager::instance()->migrationCandidates(loc, &locDescription, &candidateIds, &candidateDescriptions);
        kDebug() << "Migration candidates for" << locDescription << ":" << candidateIds << candidateDescriptions;

        KDialog *dialog = new KDialog;

        QWidget *widget = new QWidget;
        QGridLayout *mainLayout = new QGridLayout;
        mainLayout->setColumnStretch(1, 1);

        QLabel *deviceIconLabel = new QLabel;
        deviceIconLabel->setPixmap(KIconLoader::global()->loadIcon("drive-harddisk", KIconLoader::NoGroup, KIconLoader::SizeHuge));
        mainLayout->addWidget(deviceIconLabel, 0, 0);

        QLabel *mainLabel = new QLabel(
                i18n("<p>The collection </p><p><b>%1</b><br/>(%2)</p><p> is currently not found on your system.<br/> "
                     "Please choose the most appropriate option to handle this situation:</p>",
                      loc.label(), locDescription));
        mainLabel->setWordWrap(true);
        mainLayout->addWidget(mainLabel, 0, 1);

        QGroupBox *groupBox = new QGroupBox;
        mainLayout->addWidget(groupBox, 1, 0, 1, 2);

        QGridLayout *layout = new QGridLayout;
        layout->setColumnStretch(1, 1);

        QRadioButton *migrateButton = 0;
        QComboBox *migrateChoices = 0;
        if (!candidateIds.isEmpty())
        {
            migrateButton = new QRadioButton;
            QLabel *migrateLabel = new QLabel(
                    i18n("<p>The collection is still available, but the identifier changed.<br/>"
                        "This can be caused by restoring a backup, changing the partition layout "
                        "or the file system settings.<br/>"
                        "The collection is now located at this place:</p>"));
            migrateLabel->setWordWrap(true);

            migrateChoices = new QComboBox;
            for (int i=0; i<candidateIds.size(); ++i)
                migrateChoices->addItem(candidateDescriptions[i], candidateIds[i]);

            layout->addWidget(migrateButton, 0, 0, Qt::AlignTop);
            layout->addWidget(migrateLabel, 0, 1);
            layout->addWidget(migrateChoices, 1, 1);
        }

        QRadioButton *isRemovableButton = new QRadioButton;
        QLabel *isRemovableLabel = new QLabel(
                i18n("The collection is located on a storage device which is not always attached. "
                     "Mark the collection as a removable collection."));
        isRemovableLabel->setWordWrap(true);
        layout->addWidget(isRemovableButton, 2, 0, Qt::AlignTop);
        layout->addWidget(isRemovableLabel, 2, 1);

        QRadioButton *solveManuallyButton = new QRadioButton;
        QLabel *solveManuallyLabel = new QLabel(
                i18n("Take no action now. I would like to solve the problem "
                     "later using the setup dialog"));
        solveManuallyLabel->setWordWrap(true);
        layout->addWidget(solveManuallyButton, 3, 0, Qt::AlignTop);
        layout->addWidget(solveManuallyLabel, 3, 1);

        groupBox->setLayout(layout);

        widget->setLayout(mainLayout);
        dialog->setCaption(i18n("Collection not found"));
        dialog->setMainWidget(widget);
        dialog->setButtons(KDialog::Ok);

        // Default option: If there is only one candidate, default to migration.
        // Otherwise default to do nothing now.
        if (migrateButton && candidateIds.size() == 1)
            migrateButton->setChecked(true);
        else
            solveManuallyButton->setChecked(true);

        if (dialog->exec())
        {
            if (migrateButton && migrateButton->isChecked())
            {
                CollectionManager::instance()->migrateToVolume(loc, migrateChoices->itemData(migrateChoices->currentIndex()).toString());
            }
            else if (isRemovableButton->isChecked())
            {
                CollectionManager::instance()->changeType(loc, CollectionLocation::TypeVolumeRemovable);
            }
        }

        delete dialog;
    }

    // -- ---------------------------------------------------------

    // check that we have one album root
    if (CollectionManager::instance()->allLocations().isEmpty())
    {
        if (suggestedAlbumRoot.isEmpty())
            Setup::execSinglePage(Setup::CollectionsPage);
        else
        {
            CollectionManager::instance()->addLocation(suggestedAlbumRoot);
            // Not needed? See bug #188959
            //ScanController::instance()->completeCollectionScan();
        }
    }

    // -- ---------------------------------------------------------

#ifdef USE_THUMBS_DB
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (dbType=="QSQLITE")
    {
        d->dirWatchBlackList << "thumbnails-digikam.db" << "thumbnails-digikam.db-journal";
    }

    ThumbnailLoadThread::initializeThumbnailDatabase(dbType,
            thumbnailDatabaseName,
            dbHostName,
            dbPort,
            dbUser,
            dbPasswd,
            dbConnectOptions,
            new DatabaseThumbnailInfoProvider());

    DatabaseGUIErrorHandler *thumbnailsDBHandler = new DatabaseGUIErrorHandler(ThumbnailDatabaseAccess::parameters());
    ThumbnailDatabaseAccess::initDatabaseErrorHandler(thumbnailsDBHandler);

    QApplication::restoreOverrideCursor();
#endif

    // -- ---------------------------------------------------------

    // measures to filter out KDirWatch signals caused by database operations
    d->dirWatchBlackList.clear();
    DatabaseParameters params = DatabaseAccess::parameters();
    if (params.isSQLite())
    {
        QFileInfo dbFile(params.SQLiteDatabaseFile());
        d->dirWatchBlackList << dbFile.fileName() << dbFile.fileName() + "-journal";

        // ensure this is done after setting up the black list
        d->dbPathModificationDateList = d->buildDirectoryModList(dbFile);
    }

    // -- ---------------------------------------------------------

#ifdef HAVE_NEPOMUK
    if (checkNepomukService())
    {
        QDBusInterface serviceInterface("org.kde.nepomuk.services.digikamnepomukservice",
                                        "/digikamnepomukservice", "org.kde.digikam.DigikamNepomukService");
        kDebug() << "nepomuk service available" << serviceInterface.isValid();
        if (serviceInterface.isValid())
        {
            DatabaseParameters parameters = DatabaseAccess::parameters();
            KUrl url;
            parameters.insertInUrl(url);
            serviceInterface.call(QDBus::NoBlock, "setDatabase", url.url());
        }
    }
#endif // HAVE_NEPOMUK

    return true;
}

bool AlbumManager::checkNepomukService()
{
    bool hasNepomuk = false;

#ifdef HAVE_NEPOMUK
    QDBusInterface serviceInterface("org.kde.nepomuk.services.digikamnepomukservice",
                                    "/digikamnepomukservice", "org.kde.digikam.DigikamNepomukService");

    // already running? (normal)
    if (serviceInterface.isValid())
        return true;

    // start service
    QDBusInterface nepomukInterface("org.kde.NepomukServer",
                                    "/servicemanager", "org.kde.nepomuk.ServiceManager");
    if (!nepomukInterface.isValid())
    {
        kDebug() << "Nepomuk server is not reachable. Cannot start Digikam Nepomuk Service";
        return false;
    }

    QDBusReply<QStringList> availableServicesReply = nepomukInterface.call("availableServices");
    if (!availableServicesReply.isValid() || !availableServicesReply.value().contains("digikamnepomukservice"))
    {
        kDebug() << "digikamnepomukservice is not available in NepomukServer";
        return false;
    }

    /*
    QEventLoop loop;

    if (!connect(&nepomukInterface, SIGNAL(serviceInitialized(const QString &)),
                 &loop, SLOT(quit())))
    {
        kDebug() << "Could not connect to Nepomuk server signal";
        return false;
    }

    QTimer::singleShot(1000, &loop, SLOT(quit()));
    */

    kDebug() << "Trying to start up digikamnepomukservice";
    nepomukInterface.call(QDBus::NoBlock, "startService", "digikamnepomukservice");

    /*
    // wait (at most 1sec) for service to start up
    loop.exec();
    */
    hasNepomuk = true;
#endif // HAVE_NEPOMUK

    return hasNepomuk;
}

void AlbumManager::startScan()
{
    if (!d->changed)
        return;
    d->changed = false;

    // create dir watch
    d->dirWatch = new KDirWatch(this);
    connect(d->dirWatch, SIGNAL(dirty(const QString&)),
            this, SLOT(slotDirWatchDirty(const QString&)));

    KDirWatch::Method m = d->dirWatch->internalMethod();
    QString mName("FAM");
    if (m == KDirWatch::DNotify)
        mName = QString("DNotify");
    else if (m == KDirWatch::Stat)
        mName = QString("Stat");
    else if (m == KDirWatch::INotify)
        mName = QString("INotify");
    kDebug() << "KDirWatch method = " << mName;

    // connect to KDirNotify

    QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify", "FileMoved",
                                          this, SLOT(slotKioFileMoved(const QString&, const QString&)));
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify", "FilesAdded",
                                          this, SLOT(slotKioFilesAdded(const QString&)));
    QDBusConnection::sessionBus().connect(QString(), QString(), "org.kde.KDirNotify", "FilesRemoved",
                                          this, SLOT(slotKioFilesDeleted(const QStringList&)));

    // create root albums
    d->rootPAlbum = new PAlbum(i18n("My Albums"));
    insertPAlbum(d->rootPAlbum, 0);

    d->rootTAlbum = new TAlbum(i18n("My Tags"), 0, true);
    insertTAlbum(d->rootTAlbum, 0);

    d->rootSAlbum = new SAlbum(i18n("My Searches"), 0, true);
    emit signalAlbumAboutToBeAdded(d->rootSAlbum, 0, 0);
    d->allAlbumsIdHash[d->rootSAlbum->globalID()] = d->rootSAlbum;
    emit signalAlbumAdded(d->rootSAlbum);

    d->rootDAlbum = new DAlbum(QDate(), true);
    emit signalAlbumAboutToBeAdded(d->rootDAlbum, 0, 0);
    d->allAlbumsIdHash[d->rootDAlbum->globalID()] = d->rootDAlbum;
    emit signalAlbumAdded(d->rootDAlbum);

    // create albums for album roots
    QList<CollectionLocation> locations = CollectionManager::instance()->allAvailableLocations();
    foreach(const CollectionLocation& location, locations)
        addAlbumRoot(location);

    // listen to location status changes
    connect(CollectionManager::instance(), SIGNAL(locationStatusChanged(const CollectionLocation &, int)),
            this, SLOT(slotCollectionLocationStatusChanged(const CollectionLocation &, int)));
    connect(CollectionManager::instance(), SIGNAL(locationPropertiesChanged(const CollectionLocation &)),
            this, SLOT(slotCollectionLocationPropertiesChanged(const CollectionLocation &)));

    // reload albums
    refresh();

    // listen to album database changes
    connect(DatabaseAccess::databaseWatch(), SIGNAL(albumChange(const AlbumChangeset &)),
            this, SLOT(slotAlbumChange(const AlbumChangeset &)));
    connect(DatabaseAccess::databaseWatch(), SIGNAL(tagChange(const TagChangeset &)),
            this, SLOT(slotTagChange(const TagChangeset &)));
    connect(DatabaseAccess::databaseWatch(), SIGNAL(searchChange(const SearchChangeset &)),
            this, SLOT(slotSearchChange(const SearchChangeset &)));
    // listen to collection image changes
    connect(DatabaseAccess::databaseWatch(), SIGNAL(collectionImageChange(const CollectionImageChangeset &)),
            this, SLOT(slotCollectionImageChange(const CollectionImageChangeset &)));
    connect(DatabaseAccess::databaseWatch(), SIGNAL(imageTagChange(const ImageTagChangeset &)),
            this, SLOT(slotImageTagChange(const ImageTagChangeset &)));

    emit signalAllAlbumsLoaded();
}

void AlbumManager::slotCollectionLocationStatusChanged(const CollectionLocation& location, int oldStatus)
{
    // not before initialization
    if (!d->rootPAlbum)
        return;

    if (location.status() == CollectionLocation::LocationAvailable
        && oldStatus != CollectionLocation::LocationAvailable)
    {
        addAlbumRoot(location);
        // New albums have possibly appeared
        scanPAlbums();
    }
    else if (oldStatus == CollectionLocation::LocationAvailable
             && location.status() != CollectionLocation::LocationAvailable)
    {
        removeAlbumRoot(location);
        // Albums have possibly disappeared
        scanPAlbums();
    }
}

void AlbumManager::slotCollectionLocationPropertiesChanged(const CollectionLocation& location)
{
    PAlbum *album = d->albumRootAlbumHash.value(location.id());
    if (album)
    {
        QString newLabel = d->labelForAlbumRootAlbum(location);
        if (album->title() != newLabel)
        {
            album->setTitle(newLabel);
            emit signalAlbumRenamed(album);
        }
    }
}

void AlbumManager::addAlbumRoot(const CollectionLocation& location)
{
    if (!d->dirWatch->contains(location.albumRootPath()))
        d->dirWatch->addDir(location.albumRootPath(), KDirWatch::WatchSubDirs);

    PAlbum *album = d->albumRootAlbumHash.value(location.id());
    if (!album)
    {
        // Create a PAlbum for the Album Root.
        QString label = d->labelForAlbumRootAlbum(location);
        album = new PAlbum(location.id(), label);

        // insert album root created into hash
        d->albumRootAlbumHash.insert(location.id(), album);
    }
}

void AlbumManager::removeAlbumRoot(const CollectionLocation& location)
{
    d->dirWatch->removeDir(location.albumRootPath());
    // retrieve and remove from hash
    PAlbum *album = d->albumRootAlbumHash.take(location.id());
    if (album)
    {
        // delete album and all its children
        removePAlbum(album);
    }
}

void AlbumManager::refresh()
{
    scanPAlbums();
    scanTAlbums();
    scanSAlbums();
    scanDAlbums();
}

void AlbumManager::prepareItemCounts()
{
    // There is no way to find out if any data we had collected
    // previously is still valid - recompute
    scanDAlbums();
    getAlbumItemsCount();
    getTagItemsCount();
}

void AlbumManager::scanPAlbums()
{
    d->scanPAlbumsTimer->stop();

    // first insert all the current normal PAlbums into a map for quick lookup
    QHash<int, PAlbum *> oldAlbums;
    AlbumIterator it(d->rootPAlbum);
    while (it.current())
    {
        PAlbum* a = (PAlbum*)(*it);
        oldAlbums[a->id()] = a;
        ++it;
    }

    // scan db and get a list of all albums
    QList<AlbumInfo> currentAlbums = DatabaseAccess().db()->scanAlbums();

    // sort by relative path so that parents are created before children
    qSort(currentAlbums);

    QList<AlbumInfo> newAlbums;

    // go through all the Albums and see which ones are already present
    foreach (const AlbumInfo& info, currentAlbums)
    {
        // check that location of album is available
        if (CollectionManager::instance()->locationForAlbumRootId(info.albumRootId).isAvailable())
        {
            if (oldAlbums.contains(info.id))
                oldAlbums.remove(info.id);
            else
                newAlbums << info;
        }
    }

    // now oldAlbums contains all the deleted albums and
    // newAlbums contains all the new albums

    // delete old albums, informing all frontends

    // The albums have to be removed with children being removed first,
    // removePAlbum takes care of that.
    // So we only feed it the albums from oldAlbums topmost in hierarchy.
    QSet<PAlbum *> topMostOldAlbums;
    foreach (PAlbum *album, oldAlbums)
    {
        if (!album->parent() || !oldAlbums.contains(album->parent()->id()))
            topMostOldAlbums << album;
    }

    foreach(PAlbum *album, topMostOldAlbums)
    {
        // this might look like there is memory leak here, since removePAlbum
        // doesn't delete albums and looks like child Albums don't get deleted.
        // But when the parent album gets deleted, the children are also deleted.
        removePAlbum(album);
    }

    // sort by relative path so that parents are created before children
    qSort(newAlbums);

    // create all new albums
    foreach (const AlbumInfo& info, newAlbums)
    {
        if (info.relativePath.isEmpty())
            continue;

        PAlbum *album, *parent;
        if (info.relativePath == "/")
        {
            // Albums that represent the root directory of an album root
            // We have them as here new albums first time after their creation

            parent = d->rootPAlbum;
            album  = d->albumRootAlbumHash.value(info.albumRootId);

            if (!album)
            {
                kError() << "Did not find album root album in hash";
                continue;
            }

            // it has been created from the collection location
            // with album root id, parentPath "/" and a name, but no album id yet.
            album->m_id = info.id;
        }
        else
        {
            // last section, no slash
            QString name = info.relativePath.section('/', -1, -1);
            // all but last sections, leading slash, no trailing slash
            QString parentPath = info.relativePath.section('/', 0, -2);

            if (parentPath.isEmpty())
                parent = d->albumRootAlbumHash.value(info.albumRootId);
            else
                parent = d->albumPathHash.value(PAlbumPath(info.albumRootId, parentPath));

            if (!parent)
            {
                kError() <<  "Could not find parent with url: "
                              << parentPath << " for: " << info.relativePath;
                continue;
            }

            // Create the new album
            album       = new PAlbum(info.albumRootId, parentPath, name, info.id);
        }

        album->m_caption  = info.caption;
        album->m_category = info.category;
        album->m_date     = info.date;

        if (info.iconAlbumRootId)
        {
            QString albumRootPath = CollectionManager::instance()->albumRootPath(info.iconAlbumRootId);
            if (!albumRootPath.isNull())
                album->m_icon = albumRootPath + info.iconRelativePath;
        }

        insertPAlbum(album, parent);
    }

    getAlbumItemsCount();
}

void AlbumManager::updateChangedPAlbums()
{
    d->updatePAlbumsTimer->stop();

    // scan db and get a list of all albums
    QList<AlbumInfo> currentAlbums = DatabaseAccess().db()->scanAlbums();

    bool needScanPAlbums = false;

    // Find the AlbumInfo for each id in changedPAlbums
    foreach (int id, d->changedPAlbums)
    {
        foreach (const AlbumInfo& info, currentAlbums)
        {
            if (info.id == id)
            {
                d->changedPAlbums.remove(info.id);

                PAlbum *album = findPAlbum(info.id);
                if (album)
                {
                    // Renamed?
                    if (info.relativePath != "/")
                    {
                        // Handle rename of album name
                        // last section, no slash
                        QString name = info.relativePath.section('/', -1, -1);
                        QString parentPath = info.relativePath;
                        parentPath.chop(name.length());
                        if (parentPath != album->m_parentPath || info.albumRootId != album->albumRootId())
                        {
                            // Handle actual move operations: trigger ScanPAlbums
                            needScanPAlbums = true;
                            removePAlbum(album);
                            break;
                        }
                        else if (name != album->title())
                        {
                            album->setTitle(name);
                            updateAlbumPathHash();
                            emit signalAlbumRenamed(album);
                        }
                    }

                    // Update caption, collection, date
                    album->m_caption = info.caption;
                    album->m_category  = info.category;
                    album->m_date    = info.date;

                    // Icon changed?
                    QString icon;
                    if (info.iconAlbumRootId)
                    {
                        QString albumRootPath = CollectionManager::instance()->albumRootPath(info.iconAlbumRootId);
                        if (!albumRootPath.isNull())
                            icon = albumRootPath + info.iconRelativePath;
                    }
                    if (icon != album->m_icon)
                    {
                        album->m_icon = icon;
                        emit signalAlbumIconChanged(album);
                    }
                }
            }
        }
    }

    if (needScanPAlbums)
        scanPAlbums();
}

void AlbumManager::getAlbumItemsCount()
{
    d->albumItemCountTimer->stop();

    if (!AlbumSettings::instance()->getShowFolderTreeViewItemsCount())
        return;

    // List albums using kioslave

    if (d->albumListJob)
    {
        d->albumListJob->kill();
        d->albumListJob = 0;
    }

    DatabaseUrl u = DatabaseUrl::albumUrl();

    d->albumListJob = ImageLister::startListJob(u);
    d->albumListJob->addMetaData("folders", "true");

    connect(d->albumListJob, SIGNAL(result(KJob*)),
            this, SLOT(slotAlbumsJobResult(KJob*)));

    connect(d->albumListJob, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(slotAlbumsJobData(KIO::Job*, const QByteArray&)));
}

void AlbumManager::scanTAlbums()
{
    d->scanTAlbumsTimer->stop();

    // list TAlbums directly from the db
    // first insert all the current TAlbums into a map for quick lookup
    typedef QMap<int, TAlbum*> TagMap;
    TagMap tmap;

    tmap.insert(0, d->rootTAlbum);

    AlbumIterator it(d->rootTAlbum);
    while (it.current())
    {
        TAlbum* t = (TAlbum*)(*it);
        tmap.insert(t->id(), t);
        ++it;
    }

    // Retrieve the list of tags from the database
    TagInfo::List tList = DatabaseAccess().db()->scanTags();

    // sort the list. needed because we want the tags can be read in any order,
    // but we want to make sure that we are ensure to find the parent TAlbum
    // for a new TAlbum

    {
        QHash<int, TAlbum*> tagHash;

        // insert items into a dict for quick lookup
        for (TagInfo::List::const_iterator iter = tList.constBegin(); iter != tList.constEnd(); ++iter)
        {
            TagInfo info  = *iter;
            TAlbum* album = new TAlbum(info.name, info.id);
            if (info.icon.isNull())
            {
                // album image icon
                QString albumRootPath = CollectionManager::instance()->albumRootPath(info.iconAlbumRootId);
                album->m_icon         = albumRootPath + info.iconRelativePath;
            }
            else
            {
                // system icon
                album->m_icon = info.icon;
            }
            album->m_pid = info.pid;
            tagHash.insert(info.id, album);
        }
        tList.clear();

        // also add root tag
        TAlbum* rootTag = new TAlbum("root", 0, true);
        tagHash.insert(0, rootTag);

        // build tree
        for (QHash<int, TAlbum*>::const_iterator iter = tagHash.constBegin();
             iter != tagHash.constEnd(); ++iter )
        {
            TAlbum* album = *iter;
            if (album->m_id == 0)
                continue;

            TAlbum* parent = tagHash.value(album->m_pid);
            if (parent)
            {
                album->setParent(parent);
            }
            else
            {
                kWarning() << "Failed to find parent tag for tag "
                                << album->m_title
                                << " with pid "
                                << album->m_pid;
            }
        }

        // now insert the items into the list. becomes sorted
        AlbumIterator it(rootTag);
        while (it.current())
        {
            TagInfo info;
            TAlbum* album = (TAlbum*)it.current();
            info.id       = album->m_id;
            info.pid      = album->m_pid;
            info.name     = album->m_title;
            info.icon     = album->m_icon;
            tList.append(info);
            ++it;
        }

        // this will also delete all child albums
        delete rootTag;
    }

    for (TagInfo::List::const_iterator it = tList.constBegin(); it != tList.constEnd(); ++it)
    {
        TagInfo info = *it;

        // check if we have already added this tag
        if (tmap.contains(info.id))
            continue;

        // Its a new album. Find the parent of the album
        TagMap::iterator iter = tmap.find(info.pid);
        if (iter == tmap.end())
        {
            kWarning() << "Failed to find parent tag for tag "
                            << info.name
                            << " with pid "
                            << info.pid;
            continue;
        }

        TAlbum* parent = iter.value();

        // Create the new TAlbum
        TAlbum* album = new TAlbum(info.name, info.id, false);
        album->m_icon = info.icon;
        insertTAlbum(album, parent);

        // also insert it in the map we are doing lookup of parent tags
        tmap.insert(info.id, album);
    }

    getTagItemsCount();
}

void AlbumManager::getTagItemsCount()
{
    d->tagItemCountTimer->stop();

    if (!AlbumSettings::instance()->getShowFolderTreeViewItemsCount())
        return;

    // List tags using kioslave

    if (d->tagListJob)
    {
        d->tagListJob->kill();
        d->tagListJob = 0;
    }

    DatabaseUrl u = DatabaseUrl::fromTagIds(QList<int>());

    d->tagListJob = ImageLister::startListJob(u);
    d->tagListJob->addMetaData("folders", "true");

    connect(d->tagListJob, SIGNAL(result(KJob*)),
            this, SLOT(slotTagsJobResult(KJob*)));

    connect(d->tagListJob, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(slotTagsJobData(KIO::Job*, const QByteArray&)));
}

void AlbumManager::scanSAlbums()
{
    d->scanSAlbumsTimer->stop();

    // list SAlbums directly from the db
    // first insert all the current SAlbums into a map for quick lookup
    QMap<int, SAlbum*> oldSearches;

    AlbumIterator it(d->rootSAlbum);
    while (it.current())
    {
        SAlbum* search = (SAlbum*)(*it);
        oldSearches[search->id()] = search;
        ++it;
    }

    // scan db and get a list of all albums
    QList<SearchInfo> currentSearches = DatabaseAccess().db()->scanSearches();

    QList<SearchInfo> newSearches;

    // go through all the Albums and see which ones are already present
    foreach (const SearchInfo& info, currentSearches)
    {
        if (oldSearches.contains(info.id))
        {
            SAlbum *album = oldSearches[info.id];
            if (info.name != album->title()
                || info.type != album->searchType()
                || info.query != album->query())
            {
                QString oldName = album->title();

                album->setSearch(info.type, info.query);
                album->setTitle(info.name);
                if (oldName != album->title())
                    emit signalAlbumRenamed(album);
                emit signalSearchUpdated(album);
            }

            oldSearches.remove(info.id);
        }
        else
            newSearches << info;
    }

    // remove old albums that have been deleted
    foreach (SAlbum *album, oldSearches)
    {
        emit signalAlbumAboutToBeDeleted(album);
        d->allAlbumsIdHash.remove(album->globalID());
        emit signalAlbumDeleted(album);
        delete album;
        emit signalAlbumHasBeenDeleted(album);
    }

    // add new albums
    foreach (const SearchInfo& info, newSearches)
    {
        SAlbum* album = new SAlbum(info.name, info.id);
        album->setSearch(info.type, info.query);
        emit signalAlbumAboutToBeAdded(album, d->rootSAlbum, d->rootSAlbum->lastChild());
        album->setParent(d->rootSAlbum);
        d->allAlbumsIdHash[album->globalID()] = album;
        emit signalAlbumAdded(album);
    }
}

void AlbumManager::scanDAlbums()
{
    d->scanDAlbumsTimer->stop();

    // List dates using kioslave:
    // The kioslave has a special mode listing the dates
    // for which there are images in the DB.

    if (d->dateListJob)
    {
        d->dateListJob->kill();
        d->dateListJob = 0;
    }

    DatabaseUrl u = DatabaseUrl::dateUrl();

    d->dateListJob = ImageLister::startListJob(u);
    d->dateListJob->addMetaData("folders", "true");

    connect(d->dateListJob, SIGNAL(result(KJob*)),
            this, SLOT(slotDatesJobResult(KJob*)));

    connect(d->dateListJob, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(slotDatesJobData(KIO::Job*, const QByteArray&)));
}

AlbumList AlbumManager::allPAlbums() const
{
    AlbumList list;
    if (d->rootPAlbum)
        list.append(d->rootPAlbum);

    AlbumIterator it(d->rootPAlbum);
    while (it.current())
    {
        list.append(*it);
        ++it;
    }

    return list;
}

AlbumList AlbumManager::allTAlbums() const
{
    AlbumList list;
    if (d->rootTAlbum)
        list.append(d->rootTAlbum);

    AlbumIterator it(d->rootTAlbum);
    while (it.current())
    {
        list.append(*it);
        ++it;
    }

    return list;
}

AlbumList AlbumManager::allSAlbums() const
{
    AlbumList list;
    if (d->rootSAlbum)
        list.append(d->rootSAlbum);

    AlbumIterator it(d->rootSAlbum);
    while (it.current())
    {
        list.append(*it);
        ++it;
    }

    return list;
}

AlbumList AlbumManager::allDAlbums() const
{
    AlbumList list;
    if (d->rootDAlbum)
        list.append(d->rootDAlbum);

    AlbumIterator it(d->rootDAlbum);
    while (it.current())
    {
        list.append(*it);
        ++it;
    }

    return list;
}

void AlbumManager::setCurrentAlbum(Album *album)
{
    d->currentAlbum = album;
    emit signalAlbumCurrentChanged(album);
}

Album* AlbumManager::currentAlbum() const
{
    return d->currentAlbum;
}

PAlbum* AlbumManager::currentPAlbum() const
{
    return dynamic_cast<PAlbum*> (d->currentAlbum);
}

TAlbum* AlbumManager::currentTAlbum() const
{
    return dynamic_cast<TAlbum*> (d->currentAlbum);
}

PAlbum* AlbumManager::findPAlbum(const KUrl& url) const
{
    CollectionLocation location = CollectionManager::instance()->locationForUrl(url);
    if (location.isNull())
        return 0;
    return d->albumPathHash.value(PAlbumPath(location.id(), CollectionManager::instance()->album(location, url)));
}

PAlbum* AlbumManager::findPAlbum(int id) const
{
    if (!d->rootPAlbum)
        return 0;

    int gid = d->rootPAlbum->globalID() + id;

    return (PAlbum*)(d->allAlbumsIdHash.value(gid));
}

TAlbum* AlbumManager::findTAlbum(int id) const
{
    if (!d->rootTAlbum)
        return 0;

    int gid = d->rootTAlbum->globalID() + id;

    return (TAlbum*)(d->allAlbumsIdHash.value(gid));
}

SAlbum* AlbumManager::findSAlbum(int id) const
{
    if (!d->rootSAlbum)
        return 0;

    int gid = d->rootSAlbum->globalID() + id;

    return (SAlbum*)(d->allAlbumsIdHash.value(gid));
}

DAlbum* AlbumManager::findDAlbum(int id) const
{
    if (!d->rootDAlbum)
        return 0;

    int gid = d->rootDAlbum->globalID() + id;

    return (DAlbum*)(d->allAlbumsIdHash.value(gid));
}

Album* AlbumManager::findAlbum(int gid) const
{
    return d->allAlbumsIdHash.value(gid);
}

TAlbum* AlbumManager::findTAlbum(const QString& tagPath) const
{
    // handle gracefully with or without leading slash
    bool withLeadingSlash = tagPath.startsWith('/');
    AlbumIterator it(d->rootTAlbum);
    while (it.current())
    {
        TAlbum *talbum = static_cast<TAlbum *>(*it);
        if (talbum->tagPath(withLeadingSlash) == tagPath)
            return talbum;
        ++it;
    }
    return 0;

}

SAlbum* AlbumManager::findSAlbum(const QString& name) const
{
    for (Album* album = d->rootSAlbum->firstChild(); album; album = album->next())
    {
        if (album->title() == name)
            return (SAlbum*)album;
    }
    return 0;
}

void AlbumManager::addGuardedPointer(Album *album, Album **pointer)
{
    if (album)
        d->guardedPointers.insert(album, pointer);
}

void AlbumManager::removeGuardedPointer(Album *album, Album **pointer)
{
    if (album)
        d->guardedPointers.remove(album, pointer);
}

void AlbumManager::changeGuardedPointer(Album *oldAlbum, Album *album, Album **pointer)
{
    if (oldAlbum)
        d->guardedPointers.remove(oldAlbum, pointer);
    if (album)
        d->guardedPointers.insert(album, pointer);
}

void AlbumManager::invalidateGuardedPointers(Album *album)
{
    if (!album)
        return;
    QMultiHash<Album*, Album**>::iterator it = d->guardedPointers.find(album);
    for( ; it != d->guardedPointers.end() && it.key() == album; ++it)
    {
        if (it.value())
            *(it.value()) = 0;
    }
}

PAlbum* AlbumManager::createPAlbum(const QString& albumRootPath, const QString& name,
                                   const QString& caption, const QDate& date,
                                   const QString& category,
                                   QString& errMsg)
{
    CollectionLocation location = CollectionManager::instance()->locationForAlbumRootPath(albumRootPath);
    return createPAlbum(location, name, caption, date, category, errMsg);
}

PAlbum* AlbumManager::createPAlbum(const CollectionLocation& location, const QString& name,
                                   const QString& caption, const QDate& date,
                                   const QString& category,
                                   QString& errMsg)
{
    if (location.isNull() || !location.isAvailable())
    {
        errMsg = i18n("The collection location supplied is invalid or currently not available.");
        return 0;
    }

    PAlbum *album = d->albumRootAlbumHash.value(location.id());

    if (!album)
    {
        errMsg = "No album for collection location: Internal error";
        return 0;
    }

    return createPAlbum(album, name, caption, date, category, errMsg);
}


PAlbum* AlbumManager::createPAlbum(PAlbum*        parent,
                                   const QString& name,
                                   const QString& caption,
                                   const QDate&   date,
                                   const QString& category,
                                   QString&       errMsg)
{
    if (!parent)
    {
        errMsg = i18n("No parent found for album.");
        return 0;
    }

    // sanity checks
    if (name.isEmpty())
    {
        errMsg = i18n("Album name cannot be empty.");
        return 0;
    }

    if (name.contains("/"))
    {
        errMsg = i18n("Album name cannot contain '/'.");
        return 0;
    }

    if (parent->isRoot())
    {
        errMsg = i18n("createPAlbum does not accept the root album as parent.");
        return 0;
    }

    QString albumPath = parent->isAlbumRoot() ? ('/' + name) : (parent->albumPath() + '/' + name);
    int albumRootId   = parent->albumRootId();

    // first check if we have a sibling album with the same name
    PAlbum *child = (PAlbum *)parent->m_firstChild;
    while (child)
    {
        if (child->albumRootId() == albumRootId && child->albumPath() == albumPath)
        {
            errMsg = i18n("An existing album has the same name.");
            return 0;
        }
        child = (PAlbum *)child->m_next;
    }

    DatabaseUrl url = parent->databaseUrl();
    url.addPath(name);
    KUrl fileUrl    = url.fileUrl();

    if (!KIO::NetAccess::mkdir(fileUrl, qApp->activeWindow()))
    {
        errMsg = i18n("Failed to create directory,");
        return 0;
    }

    ChangingDB changing(d);
    int id = DatabaseAccess().db()->addAlbum(albumRootId, albumPath, caption, date, category);

    if (id == -1)
    {
        errMsg = i18n("Failed to add album to database");
        return 0;
    }

    QString parentPath;
    if (!parent->isAlbumRoot())
        parentPath = parent->albumPath();

    PAlbum *album    = new PAlbum(albumRootId, parentPath, name, id);
    album->m_caption = caption;
    album->m_category  = category;
    album->m_date    = date;

    insertPAlbum(album, parent);

    return album;
}

bool AlbumManager::renamePAlbum(PAlbum* album, const QString& newName,
                                QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such album");
        return false;
    }

    if (album == d->rootPAlbum)
    {
        errMsg = i18n("Cannot rename root album");
        return false;
    }

    if (album->isAlbumRoot())
    {
        errMsg = i18n("Cannot rename album root album");
        return false;
    }

    if (newName.contains("/"))
    {
        errMsg = i18n("Album name cannot contain '/'");
        return false;
    }

    // first check if we have another sibling with the same name
    Album *sibling = album->m_parent->m_firstChild;
    while (sibling)
    {
        if (sibling->title() == newName)
        {
            errMsg = i18n("Another album with the same name already exists.\n"
                          "Please choose another name.");
            return false;
        }
        sibling = sibling->m_next;
    }

    QString oldAlbumPath = album->albumPath();

    KUrl oldUrl = album->fileUrl();
    album->setTitle(newName);
    album->m_path = newName;
    KUrl newUrl = album->fileUrl();

    QString newAlbumPath = album->albumPath();

    // We use a private shortcut around collection scanner noticing our changes,
    // we rename them directly. Faster.
    ScanController::instance()->suspendCollectionScan();

    KIO::Job *job = KIO::rename(oldUrl, newUrl, KIO::HideProgressInfo);
    if (!KIO::NetAccess::synchronousRun(job, 0))
    {
        errMsg = i18n("Failed to rename Album");
        return false;
    }

    // now rename the album and subalbums in the database

    {
        DatabaseAccess access;
        ChangingDB changing(d);
        access.db()->renameAlbum(album->id(), album->albumRootId(), album->albumPath());

        PAlbum* subAlbum = 0;
        AlbumIterator it(album);
        while ((subAlbum = static_cast<PAlbum*>(it.current())) != 0)
        {
            subAlbum->m_parentPath = newAlbumPath + subAlbum->m_parentPath.mid(oldAlbumPath.length());
            access.db()->renameAlbum(subAlbum->id(), album->albumRootId(), subAlbum->albumPath());
            ++it;
        }
    }

    updateAlbumPathHash();
    emit signalAlbumRenamed(album);

    ScanController::instance()->resumeCollectionScan();

    return true;
}

void AlbumManager::updateAlbumPathHash()
{
    // Update AlbumDict. basically clear it and rebuild from scratch
    {
        d->albumPathHash.clear();
        AlbumIterator it(d->rootPAlbum);
        PAlbum* subAlbum = 0;
        while ((subAlbum = (PAlbum*)it.current()) != 0)
        {
            d->albumPathHash[subAlbum] = subAlbum;
            ++it;
        }
    }

}

bool AlbumManager::updatePAlbumIcon(PAlbum *album, qlonglong iconID, QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such album");
        return false;
    }

    if (album == d->rootPAlbum)
    {
        errMsg = i18n("Cannot edit root album");
        return false;
    }

    {
        DatabaseAccess access;
        ChangingDB changing(d);
        access.db()->setAlbumIcon(album->id(), iconID);
        QString iconRelativePath;
        int iconAlbumRootId;
        if (access.db()->getAlbumIcon(album->id(), &iconAlbumRootId, &iconRelativePath))
        {
            QString albumRootPath = CollectionManager::instance()->albumRootPath(iconAlbumRootId);
            album->m_icon = albumRootPath + iconRelativePath;
        }
        else
            album->m_icon.clear();
    }

    emit signalAlbumIconChanged(album);

    return true;
}

TAlbum* AlbumManager::createTAlbum(TAlbum* parent, const QString& name,
                                   const QString& iconkde, QString& errMsg)
{
    if (!parent)
    {
        errMsg = i18n("No parent found for tag");
        return 0;
    }

    // sanity checks
    if (name.isEmpty())
    {
        errMsg = i18n("Tag name cannot be empty");
        return 0;
    }

    if (name.contains("/"))
    {
        errMsg = i18n("Tag name cannot contain '/'");
        return 0;
    }

    // first check if we have another album with the same name
    Album *child = parent->m_firstChild;
    while (child)
    {
        if (child->title() == name)
        {
            errMsg = i18n("Tag name already exists");
            return 0;
        }
        child = child->m_next;
    }

    ChangingDB changing(d);
    int id = DatabaseAccess().db()->addTag(parent->id(), name, iconkde, 0);
    if (id == -1)
    {
        errMsg = i18n("Failed to add tag to database");
        return 0;
    }

    TAlbum *album = new TAlbum(name, id, false);
    album->m_icon = iconkde;

    insertTAlbum(album, parent);

    return album;
}

AlbumList AlbumManager::findOrCreateTAlbums(const QStringList& tagPaths)
{
    // find tag ids for tag paths in list, create if they don't exist
    QList<int> tagIDs = DatabaseAccess().db()->getTagsFromTagPaths(tagPaths, true);

    // create TAlbum objects for the newly created tags
    scanTAlbums();

    AlbumList resultList;

    for (QList<int>::const_iterator it = tagIDs.constBegin() ; it != tagIDs.constEnd() ; ++it)
    {
        resultList.append(findTAlbum(*it));
    }

    return resultList;
}

bool AlbumManager::deleteTAlbum(TAlbum* album, QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such album");
        return false;
    }

    if (album == d->rootTAlbum)
    {
        errMsg = i18n("Cannot delete Root Tag");
        return false;
    }

    {
        DatabaseAccess access;
        ChangingDB changing(d);
        access.db()->deleteTag(album->id());

        Album* subAlbum = 0;
        AlbumIterator it(album);
        while ((subAlbum = it.current()) != 0)
        {
            access.db()->deleteTag(subAlbum->id());
            ++it;
        }
    }

    removeTAlbum(album);

    return true;
}

bool AlbumManager::renameTAlbum(TAlbum* album, const QString& name,
                                QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such album");
        return false;
    }

    if (album == d->rootTAlbum)
    {
        errMsg = i18n("Cannot edit root tag");
        return false;
    }

    if (name.contains("/"))
    {
        errMsg = i18n("Tag name cannot contain '/'");
        return false;
    }

    // first check if we have another sibling with the same name
    Album *sibling = album->m_parent->m_firstChild;
    while (sibling)
    {
        if (sibling->title() == name)
        {
            errMsg = i18n("Another tag with the same name already exists.\n"
                          "Please choose another name.");
            return false;
        }
        sibling = sibling->m_next;
    }

    ChangingDB changing(d);
    DatabaseAccess().db()->setTagName(album->id(), name);
    album->setTitle(name);
    emit signalAlbumRenamed(album);

    return true;
}

bool AlbumManager::moveTAlbum(TAlbum* album, TAlbum *newParent, QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such album");
        return false;
    }

    if (album == d->rootTAlbum)
    {
        errMsg = i18n("Cannot move root tag");
        return false;
    }

    emit signalAlbumAboutToBeDeleted(album);
    if (album->parent())
        album->parent()->removeChild(album);
    album->setParent(0);
    emit signalAlbumDeleted(album);
    emit signalAlbumHasBeenDeleted(album);

    emit signalAlbumAboutToBeAdded(album, newParent, newParent ? newParent->lastChild() : 0);
    ChangingDB changing(d);
    DatabaseAccess().db()->setTagParentID(album->id(), newParent->id());
    album->setParent(newParent);
    emit signalAlbumAdded(album);

    emit signalTAlbumMoved(album, newParent);

    return true;
}

bool AlbumManager::updateTAlbumIcon(TAlbum* album, const QString& iconKDE,
                                    qlonglong iconID, QString& errMsg)
{
    if (!album)
    {
        errMsg = i18n("No such tag");
        return false;
    }

    if (album == d->rootTAlbum)
    {
        errMsg = i18n("Cannot edit root tag");
        return false;
    }

    {
        DatabaseAccess access;
        ChangingDB changing(d);
        access.db()->setTagIcon(album->id(), iconKDE, iconID);
        QString albumRelativePath, iconKDE;
        int albumRootId;
        if (access.db()->getTagIcon(album->id(), &albumRootId, &albumRelativePath, &iconKDE))
        {
            if (iconKDE.isEmpty())
            {
                QString albumRootPath = CollectionManager::instance()->albumRootPath(albumRootId);
                album->m_icon = albumRootPath + albumRelativePath;
            }
            else
            {
                album->m_icon = iconKDE;
            }
        }
        else
            album->m_icon.clear();
    }

    emit signalAlbumIconChanged(album);

    return true;
}

AlbumList AlbumManager::getRecentlyAssignedTags() const
{
    QList<int> tagIDs = DatabaseAccess().db()->getRecentlyAssignedTags();

    AlbumList resultList;

    for (QList<int>::const_iterator it = tagIDs.constBegin() ; it != tagIDs.constEnd() ; ++it)
    {
        resultList.append(findTAlbum(*it));
    }

    return resultList;
}

QStringList AlbumManager::tagPaths(const QList<int>& tagIDs, bool leadingSlash) const
{
    QStringList tagPaths;

    for (QList<int>::const_iterator it = tagIDs.constBegin(); it != tagIDs.constEnd(); ++it)
    {
        TAlbum *album = findTAlbum(*it);
        if (album)
        {
            tagPaths.append(album->tagPath(leadingSlash));
        }
    }

    return tagPaths;
}

QStringList AlbumManager::tagNames(const QList<int>& tagIDs) const
{
    QStringList tagNames;

    foreach(int id, tagIDs)
    {
        TAlbum *album = findTAlbum(id);
        if (album)
        {
            tagNames << album->title();
        }
    }

    return tagNames;
}

QHash<int, QString> AlbumManager::tagPaths(bool leadingSlash) const
{
    QHash<int, QString> hash;
    AlbumIterator it(d->rootTAlbum);
    while (it.current())
    {
        TAlbum* t = (TAlbum*)(*it);
        hash.insert(t->id(), t->tagPath(leadingSlash));
        ++it;
    }
    return hash;
}

QHash<int, QString> AlbumManager::tagNames() const
{
    QHash<int, QString> hash;
    AlbumIterator it(d->rootTAlbum);
    while (it.current())
    {
        TAlbum* t = (TAlbum*)(*it);
        hash.insert(t->id(), t->title());
        ++it;
    }
    return hash;
}

QHash<int, QString> AlbumManager::albumTitles() const
{
    QHash<int, QString> hash;
    AlbumIterator it(d->rootPAlbum);
    while (it.current())
    {
        PAlbum* a = (PAlbum*)(*it);
        hash.insert(a->id(), a->title());
        ++it;
    }
    return hash;
}

SAlbum* AlbumManager::createSAlbum(const QString& name, DatabaseSearch::Type type, const QString& query)
{
    // first iterate through all the search albums and see if there's an existing
    // SAlbum with same name. (Remember, SAlbums are arranged in a flat list)
    SAlbum *album = findSAlbum(name);
    ChangingDB changing(d);
    if (album)
    {
        album->setSearch(type, query);
        DatabaseAccess access;
        access.db()->updateSearch(album->id(), album->m_searchType, name, query);
        return album;
    }

    int id = DatabaseAccess().db()->addSearch(type, name, query);

    if (id == -1)
        return 0;

    album = new SAlbum(name, id);
    emit signalAlbumAboutToBeAdded(album, d->rootSAlbum, d->rootSAlbum->lastChild());
    album->setSearch(type, query);
    album->setParent(d->rootSAlbum);

    d->allAlbumsIdHash.insert(album->globalID(), album);
    emit signalAlbumAdded(album);

    return album;
}

bool AlbumManager::updateSAlbum(SAlbum* album, const QString& changedQuery,
                                const QString& changedName, DatabaseSearch::Type type)
{
    if (!album)
        return false;

    QString newName = changedName.isNull() ? album->title() : changedName;
    DatabaseSearch::Type newType = (type == DatabaseSearch::UndefinedType) ? album->searchType() : type;

    ChangingDB changing(d);
    DatabaseAccess().db()->updateSearch(album->id(), newType, newName, changedQuery);

    QString oldName = album->title();

    album->setSearch(newType, changedQuery);
    album->setTitle(newName);
    if (oldName != album->title())
        emit signalAlbumRenamed(album);

    return true;
}

bool AlbumManager::deleteSAlbum(SAlbum* album)
{
    if (!album)
        return false;

    emit signalAlbumAboutToBeDeleted(album);

    ChangingDB changing(d);
    DatabaseAccess().db()->deleteSearch(album->id());

    d->allAlbumsIdHash.remove(album->globalID());
    emit signalAlbumDeleted(album);
    delete album;
    emit signalAlbumHasBeenDeleted(album);

    return true;
}

void AlbumManager::insertPAlbum(PAlbum *album, PAlbum *parent)
{
    if (!album)
        return;

    emit signalAlbumAboutToBeAdded(album, parent, parent ? parent->lastChild() : 0);

    if (parent)
        album->setParent(parent);

    d->albumPathHash[album]  = album;
    d->allAlbumsIdHash[album->globalID()] = album;

    emit signalAlbumAdded(album);
}

void AlbumManager::removePAlbum(PAlbum *album)
{
    if (!album)
        return;

    // remove all children of this album
    Album* child = album->m_firstChild;
    while (child)
    {
        Album *next = child->m_next;
        removePAlbum((PAlbum*)child);
        child = next;
    }

    emit signalAlbumAboutToBeDeleted(album);
    d->albumPathHash.remove(album);
    d->allAlbumsIdHash.remove(album->globalID());

    DatabaseUrl url = album->databaseUrl();

    if (album == d->currentAlbum)
    {
        d->currentAlbum = 0;
        emit signalAlbumCurrentChanged(0);
    }

    emit signalAlbumDeleted(album);
    delete album;
    emit signalAlbumHasBeenDeleted(album);
}

void AlbumManager::insertTAlbum(TAlbum *album, TAlbum *parent)
{
    if (!album)
        return;

    emit signalAlbumAboutToBeAdded(album, parent, parent ? parent->lastChild() : 0);

    if (parent)
        album->setParent(parent);

    d->allAlbumsIdHash.insert(album->globalID(), album);

    emit signalAlbumAdded(album);
}

void AlbumManager::removeTAlbum(TAlbum *album)
{
    if (!album)
        return;

    // remove all children of this album
    Album* child = album->m_firstChild;
    while (child)
    {
        Album *next = child->m_next;
        removeTAlbum((TAlbum*)child);
        child = next;
    }

    emit signalAlbumAboutToBeDeleted(album);
    d->allAlbumsIdHash.remove(album->globalID());

    if (album == d->currentAlbum)
    {
        d->currentAlbum = 0;
        emit signalAlbumCurrentChanged(0);
    }

    emit signalAlbumDeleted(album);
    delete album;
    emit signalAlbumHasBeenDeleted(album);
}

void AlbumManager::notifyAlbumDeletion(Album *album)
{
    invalidateGuardedPointers(album);
}

void AlbumManager::slotAlbumsJobResult(KJob* job)
{
    d->albumListJob = 0;

    if (job->error())
    {
        kWarning() << k_funcinfo << "Failed to list albums";
        return;
    }
}

void AlbumManager::slotAlbumsJobData(KIO::Job*, const QByteArray& data)
{
    if (data.isEmpty())
        return;

    QMap<int, int> albumsStatMap;
    QByteArray di(data);
    QDataStream ds(&di, QIODevice::ReadOnly);
    ds >> albumsStatMap;

    emit signalPAlbumsDirty(albumsStatMap);
}

void AlbumManager::slotTagsJobResult(KJob* job)
{
    d->tagListJob = 0;

    if (job->error())
    {
        kWarning() << k_funcinfo << "Failed to list tags";
        return;
    }
}

void AlbumManager::slotTagsJobData(KIO::Job*, const QByteArray& data)
{
    if (data.isEmpty())
        return;

    QMap<int, int> tagsStatMap;
    QByteArray di(data);
    QDataStream ds(&di, QIODevice::ReadOnly);
    ds >> tagsStatMap;

    emit signalTAlbumsDirty(tagsStatMap);
}

void AlbumManager::slotDatesJobResult(KJob* job)
{
    d->dateListJob = 0;

    if (job->error())
    {
        kWarning() << "Failed to list dates";
        return;
    }

    emit signalAllDAlbumsLoaded();
}

void AlbumManager::slotDatesJobData(KIO::Job*, const QByteArray& data)
{
    if (data.isEmpty())
        return;

    // insert all the DAlbums into a qmap for quick access
    QMap<QDate, DAlbum*> mAlbumMap;
    QMap<int, DAlbum*>   yAlbumMap;

    AlbumIterator it(d->rootDAlbum);
    while (it.current())
    {
        DAlbum* a = (DAlbum*)(*it);
        if (a->range() == DAlbum::Month)
            mAlbumMap.insert(a->date(), a);
        else
            yAlbumMap.insert(a->date().year(), a);
        ++it;
    }

    QMap<QDateTime, int> datesStatMap;
    QByteArray di(data);
    QDataStream ds(&di, QIODevice::ReadOnly);
    ds >> datesStatMap;

    QMap<YearMonth, int> yearMonthMap;
    for (QMap<QDateTime, int>::const_iterator it = datesStatMap.constBegin(); it != datesStatMap.constEnd(); ++it)
    {
        YearMonth yearMonth = YearMonth(it.key().date().year(), it.key().date().month());

        QMap<YearMonth, int>::iterator it2 = yearMonthMap.find(yearMonth);
        if ( it2 == yearMonthMap.end() )
        {
            yearMonthMap.insert( yearMonth, *it );
        }
        else
        {
            *it2 += *it;
        }
    }

    int year, month;
    for (QMap<YearMonth, int>::const_iterator iter = yearMonthMap.constBegin();
         iter != yearMonthMap.constEnd(); ++iter)
    {
        year  = iter.key().first;
        month = iter.key().second;

        QDate md(year, month, 1);

        // Do we already have this Month album
        if (mAlbumMap.contains(md))
        {
            // already there. remove Month album from map
            mAlbumMap.remove(md);

            if (yAlbumMap.contains(year))
            {
                // already there. remove from map
                yAlbumMap.remove(year);
            }

            continue;
        }

        // Check if Year Album already exist.
        DAlbum *yAlbum = 0;
        AlbumIterator it(d->rootDAlbum);
        while (it.current())
        {
            DAlbum* a = (DAlbum*)(*it);
            if (a->date() == QDate(year, 1, 1) && a->range() == DAlbum::Year)
            {
                yAlbum = a;
                break;
            }
            ++it;
        }

        // If no, create Year album.
        if (!yAlbum)
        {
            yAlbum = new DAlbum(QDate(year, 1, 1), false, DAlbum::Year);
            emit signalAlbumAboutToBeAdded(yAlbum, d->rootDAlbum, d->rootDAlbum->lastChild());
            yAlbum->setParent(d->rootDAlbum);
            d->allAlbumsIdHash.insert(yAlbum->globalID(), yAlbum);
            emit signalAlbumAdded(yAlbum);
        }

        // Create Month album
        DAlbum *mAlbum = new DAlbum(md);
        emit signalAlbumAboutToBeAdded(mAlbum, yAlbum, yAlbum->lastChild());
        mAlbum->setParent(yAlbum);
        d->allAlbumsIdHash.insert(mAlbum->globalID(), mAlbum);
        emit signalAlbumAdded(mAlbum);
    }

    // Now the items contained in the maps are the ones which
    // have been deleted.
    for (QMap<QDate, DAlbum*>::const_iterator it = mAlbumMap.constBegin();
         it != mAlbumMap.constEnd(); ++it)
    {
        DAlbum* album = it.value();
        emit signalAlbumAboutToBeDeleted(album);
        d->allAlbumsIdHash.remove(album->globalID());
        emit signalAlbumDeleted(album);
        delete album;
        emit signalAlbumHasBeenDeleted(album);
    }

    for (QMap<int, DAlbum*>::const_iterator it = yAlbumMap.constBegin();
         it != yAlbumMap.constEnd(); ++it)
    {
        DAlbum* album = it.value();
        emit signalAlbumAboutToBeDeleted(album);
        d->allAlbumsIdHash.remove(album->globalID());
        emit signalAlbumDeleted(album);
        delete album;
        emit signalAlbumHasBeenDeleted(album);
    }

    emit signalDAlbumsDirty(yearMonthMap);
    emit signalDatesMapDirty(datesStatMap);
}

void AlbumManager::slotAlbumChange(const AlbumChangeset& changeset)
{
    if (d->changingDB || !d->rootPAlbum)
        return;

    switch(changeset.operation())
    {
        case AlbumChangeset::Added:
        case AlbumChangeset::Deleted:
            if (!d->scanPAlbumsTimer->isActive())
                d->scanPAlbumsTimer->start();
            break;
        case AlbumChangeset::Renamed:
        case AlbumChangeset::PropertiesChanged:
            // mark for rescan
            d->changedPAlbums << changeset.albumId();
            if (!d->updatePAlbumsTimer->isActive())
                d->updatePAlbumsTimer->start();
            break;
        case AlbumChangeset::Unknown:
            break;
    }
}

void AlbumManager::slotTagChange(const TagChangeset& changeset)
{
    if (d->changingDB || !d->rootTAlbum)
        return;

    switch(changeset.operation())
    {
        case TagChangeset::Added:
        case TagChangeset::Deleted:
        case TagChangeset::Reparented:
            if (!d->scanTAlbumsTimer->isActive())
                d->scanTAlbumsTimer->start();
            break;
        case TagChangeset::Renamed:
        case TagChangeset::IconChanged:
            /**
             * @todo
             */
            break;
        case TagChangeset::Unknown:
            break;
    }
}

void AlbumManager::slotSearchChange(const SearchChangeset& changeset)
{
    if (d->changingDB || !d->rootSAlbum)
        return;

    switch(changeset.operation())
    {
        case SearchChangeset::Added:
        case SearchChangeset::Deleted:
            if (!d->scanSAlbumsTimer->isActive())
                d->scanSAlbumsTimer->start();
            break;
        case SearchChangeset::Changed:
            break;
        case SearchChangeset::Unknown:
            break;
    }
}

void AlbumManager::slotCollectionImageChange(const CollectionImageChangeset& changeset)
{
    if (!d->rootDAlbum)
        return;

    switch (changeset.operation())
    {
        case CollectionImageChangeset::Added:
        case CollectionImageChangeset::Removed:
        case CollectionImageChangeset::RemovedAll:
            if (!d->scanDAlbumsTimer->isActive())
                d->scanDAlbumsTimer->start();
            if (!d->albumItemCountTimer->isActive())
                d->albumItemCountTimer->start();
            break;
        default:
            break;
    }
}

void AlbumManager::slotImageTagChange(const ImageTagChangeset& changeset)
{
    if (!d->rootTAlbum)
        return;
    switch (changeset.operation())
    {
        case ImageTagChangeset::Added:
        case ImageTagChangeset::Removed:
        case ImageTagChangeset::RemovedAll:
            if (!d->tagItemCountTimer->isActive())
                d->tagItemCountTimer->start();
            break;
        default:
            break;
    }
}

void AlbumManager::slotNotifyFileChange(const QString& path)
{
    //kDebug() << "Detected file change at" << path;
    ScanController::instance()->scheduleCollectionScanRelaxed(path);
}

void AlbumManager::slotDirWatchDirty(const QString& path)
{
    // Filter out dirty signals triggered by changes on the database file
    foreach (const QString &bannedFile, d->dirWatchBlackList)
        if (path.endsWith(bannedFile))
            return;

    DatabaseParameters params = DatabaseAccess::parameters();
    if (params.isSQLite())
    {
        QFileInfo info(path);
        QDir dir;
        if (info.isDir())
            dir = QDir(path);
        else
            dir = info.dir();
        QFileInfo dbFile(params.SQLiteDatabaseFile());

        // Workaround for broken KDirWatch in KDE 4.2.4
        if (path.startsWith(dbFile.filePath()))
            return;

        // is the signal for the directory containing the database file?
        if (dbFile.dir() == dir)
        {
            // retrieve modification dates
            QList<QDateTime> modList = d->buildDirectoryModList(dbFile);

            // check for equality
            if (modList == d->dbPathModificationDateList)
            {
                //kDebug() << "Filtering out db-file-triggered dir watch signal";
                // we can skip the signal
                return;
            }

            // set new list
            d->dbPathModificationDateList = modList;
        }
    }

    kDebug() << "KDirWatch detected change at" << path;

    slotNotifyFileChange(path);
}

void AlbumManager::slotKioFileMoved(const QString& urlFrom, const QString& urlTo)
{
    kDebug() << urlFrom << urlTo;
    handleKioNotification(KUrl(urlFrom));
    handleKioNotification(KUrl(urlTo));
}

void AlbumManager::slotKioFilesAdded(const QString& url)
{
    kDebug() << url;
    handleKioNotification(KUrl(url));
}

void AlbumManager::slotKioFilesDeleted(const QStringList& urls)
{
    kDebug() << urls;
    foreach (const QString& url, urls)
        handleKioNotification(KUrl(url));
}

void AlbumManager::handleKioNotification(const KUrl& url)
{
    if (url.isLocalFile())
    {
        QString path = url.directory();
        //kDebug() << path << !CollectionManager::instance()->albumRootPath(path).isEmpty();
        // check path is in our collection
        if (CollectionManager::instance()->albumRootPath(path).isNull())
            return;

        kDebug() << "KDirNotify detected file change at" << path;

        slotNotifyFileChange(path);
    }
    else
    {
        DatabaseUrl dbUrl(url);
        if (dbUrl.isAlbumUrl())
        {
            QString path = dbUrl.fileUrl().directory();
            kDebug() << "KDirNotify detected file change at" << path;
            slotNotifyFileChange(path);
        }
    }
}


}  // namespace Digikam
