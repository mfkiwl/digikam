/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2009-12-11
 * Description : test cases for the various album models
 *
 * Copyright (C) 2009 by Johannes Wienke <languitar at semipol dot de>
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

#include "albummodeltest.moc"

// Qt includes

#include <qdir.h>
#include <qtest.h>

// KDE includes

#include <kdebug.h>
#include <kio/netaccess.h>
#include <qtest_kde.h>

// Local includes

#include "albumdb.h"
#include "albumfiltermodel.h"
#include "albummanager.h"
#include "albummodel.h"
#include "albumsettings.h"
#include "albumthumbnailloader.h"
#include "collectionlocation.h"
#include "collectionmanager.h"
#include "config-digikam.h"
#include "loadingcacheinterface.h"
#include "modeltest.h"
#include "scancontroller.h"
#include "thumbnailloadthread.h"

using namespace Digikam;

const QString IMAGE_PATH(KDESRCDIR"albummodeltestimages");

QTEST_KDEMAIN(AlbumModelTest, GUI)

AlbumModelTest::AlbumModelTest() :
    albumCategory("DummyCategory")
{
}

AlbumModelTest::~AlbumModelTest()
{
}

void AlbumModelTest::initTestCase()
{

    tempSuffix = "albummodeltest-" + QTime::currentTime().toString();
    dbPath = QDir::temp().absolutePath() + QString("/") + tempSuffix;
    ;
    if (QDir::temp().exists(tempSuffix))
    {
        QString msg = QString("Error creating temp path") + dbPath;
        QVERIFY2(false, msg.toAscii().data());
    }

    QDir::temp().mkdir(tempSuffix);

    kDebug() << "Using database path for test: " << dbPath;

    AlbumSettings::instance()->setShowFolderTreeViewItemsCount(true);

    // use a testing database
    AlbumManager::instance();

    // catch palbum counts for waiting
    connect(AlbumManager::instance(), SIGNAL(signalPAlbumsDirty(const QMap<int, int>&)),
            this, SLOT(setLastPAlbumCountMap(const QMap<int, int>&)));

    AlbumManager::checkDatabaseDirsAfterFirstRun(QDir::temp().absoluteFilePath(
                    tempSuffix), QDir::temp().absoluteFilePath(tempSuffix));
    bool dbChangeGood = AlbumManager::instance()->setDatabase(
                    QDir::temp().absoluteFilePath(tempSuffix), false,
                    QDir::temp().absoluteFilePath(tempSuffix));
    QVERIFY2(dbChangeGood, "Could not set temp album db");
    QList<CollectionLocation> locs =
                    CollectionManager::instance()->allAvailableLocations();
    QVERIFY2(locs.size(), "Failed to auto-create one collection in setDatabase");

    ScanController::instance()->completeCollectionScan();
    AlbumManager::instance()->startScan();

    QVERIFY2(AlbumManager::instance()->allPAlbums().size() == 2,
                    "Failed to scan empty directory and have one root and one album root album");
}

void AlbumModelTest::cleanupTestCase()
{

    ScanController::instance()->shutDown();
    AlbumManager::instance()->cleanUp();
    ThumbnailLoadThread::cleanUp();
    AlbumThumbnailLoader::instance()->cleanUp();
    LoadingCacheInterface::cleanUp();

    KUrl deleteUrl = KUrl::fromPath(dbPath);
    KIO::NetAccess::del(deleteUrl, 0);
    kDebug() << "deleted test folder " << deleteUrl;

}

// Qt test doesn't use exceptions, so using assertion macros in methods called
// from a test slot doesn't stop the test method and may result in inconsistent
// data or segfaults. Therefore use macros for these functions.

#define safeCreatePAlbum(parent, name, result) \
{ \
    QString error; \
    result = AlbumManager::instance()->createPAlbum(parent, name, name, \
                    QDate::currentDate(), albumCategory, error); \
    QVERIFY2(result, QString(QString("Error creating PAlbum for test: ") + error).toAscii()); \
}

#define safeCreateTAlbum(parent, name, result) \
{ \
    QString error; \
    result = AlbumManager::instance()->createTAlbum(parent, name, "", error); \
    QVERIFY2(result, QString(QString("Error creating TAlbum for test: ") + error).toAscii()); \
}

void AlbumModelTest::init()
{

    palbumCountMap.clear();

    // create a model to check that model work is done correctly while scanning
    addedIds.clear();
    startModel = new AlbumModel;
    startModel->setShowCount(true);
    connect(startModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            this, SLOT(slotStartModelRowsInserted(const QModelIndex&, int, int)));
    connect(startModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex &)),
            this, SLOT(slotStartModelDataChanged(const QModelIndex&, const QModelIndex &)));
    kDebug() << "Created startModel" << startModel;

    // ensure that this model is empty in the beginning except for the root
    // album and the collection
    QCOMPARE(startModel->rowCount(), 1);
    QModelIndex rootIndex = startModel->index(0, 0);
    QCOMPARE(startModel->rowCount(rootIndex), 1);
    QModelIndex collectionIndex = startModel->index(0, 0, rootIndex);
    QCOMPARE(startModel->rowCount(collectionIndex), 0);

    // insert some test data

    // physical albums

    // create two of them by creating directories and scanning
    QDir dir(dbPath);
    dir.mkdir("root0");
    dir.mkdir("root1");

    ScanController::instance()->completeCollectionScan();
    AlbumManager::instance()->refresh();

    QCOMPARE(AlbumManager::instance()->allPAlbums().size(), 4);

    QString error;
    palbumRoot0 = AlbumManager::instance()->findPAlbum(KUrl::fromPath(dbPath
                    + "/root0"));
    QVERIFY2(palbumRoot0, "Error having PAlbum root0 in AlbumManager");
    palbumRoot1 = AlbumManager::instance()->findPAlbum(KUrl::fromPath(dbPath
                    + "/root1"));
    QVERIFY2(palbumRoot1, "Error having PAlbum root1 in AlbumManager");

    // Create some more through AlbumManager
    palbumRoot2 = AlbumManager::instance()->createPAlbum(dbPath, "root2",
                    "root album 2", QDate::currentDate(), albumCategory, error);
    QVERIFY2(palbumRoot2, QString(QString("Error creating PAlbum for test: ") + error).toAscii());

    safeCreatePAlbum(palbumRoot0, "root0child0", palbumChild0Root0);
    safeCreatePAlbum(palbumRoot0, "root0child1", palbumChild1Root0);
    const QString sameName = "sameName Album";
    safeCreatePAlbum(palbumRoot0, sameName, palbumChild2Root0);

    safeCreatePAlbum(palbumRoot1, sameName, palbumChild0Root1);

    kDebug() << "AlbumManager now knows these PAlbums:";
    foreach(Album *a, AlbumManager::instance()->allPAlbums())
    {
    	kDebug() << "\t" << a->title();
    }

    // tags

    rootTag = AlbumManager::instance()->findTAlbum(0);
    QVERIFY(rootTag);

    safeCreateTAlbum(rootTag, "root0", talbumRoot0);
    safeCreateTAlbum(rootTag, "root1", talbumRoot1);

    safeCreateTAlbum(talbumRoot0, "child0 root 0", talbumChild0Root0);
    safeCreateTAlbum(talbumRoot0, "child1 root 0", talbumChild1Root0);

    safeCreateTAlbum(talbumChild1Root0, sameName, talbumChild0Child1Root0);

    safeCreateTAlbum(talbumRoot1, sameName, talbumChild0Root1);

    kDebug() << "created tags";

    // add some images for having date albums

    QDir imageDir(IMAGE_PATH);
    imageDir.setNameFilters(QStringList("*.jpg"));
    QStringList imageFiles = imageDir.entryList();

    kDebug() << "copying images " << imageFiles << " to "
             << palbumChild0Root0->fileUrl();

    foreach (const QString &imageFile, imageFiles)
    {
        QString src = IMAGE_PATH + "/" + imageFile;
        QString dst = palbumChild0Root0->fileUrl().toLocalFile() + "/" + imageFile;
        bool copied = QFile::copy(src, dst);
        QVERIFY2(copied, "Test images must be copied");
    }

    ScanController::instance()->completeCollectionScan();

    if (AlbumManager::instance()->allDAlbums().count() <= 1)
        ensureItemCounts();

    kDebug() << "date albums: " << AlbumManager::instance()->allDAlbums();

    // root + 2 years + 2 and 3 months per year + (1997 as test year for date ordering with 12 months) = 21
    QCOMPARE(AlbumManager::instance()->allDAlbums().size(), 21);
    // ensure that there is a root date album
    DAlbum *rootFromAlbumManager = AlbumManager::instance()->findDAlbum(0);
    QVERIFY(rootFromAlbumManager);
    DAlbum *rootFromList = 0;
    foreach(Album *album, AlbumManager::instance()->allDAlbums())
    {
        DAlbum *dAlbum = dynamic_cast<DAlbum*> (album);
        QVERIFY(dAlbum);
        if (dAlbum->isRoot())
        {
            rootFromList = dAlbum;
        }
    }
    QVERIFY(rootFromList);
    QVERIFY(rootFromList == rootFromAlbumManager);

}

void AlbumModelTest::testStartAlbumModel()
{

    // verify that the start album model got all these changes

    // one root
    QCOMPARE(startModel->rowCount(), 1);
    // one collection
    QModelIndex rootIndex = startModel->index(0, 0);
    QCOMPARE(startModel->rowCount(rootIndex), 1);
    // two albums in the collection
    QModelIndex collectionIndex = startModel->index(0, 0, rootIndex);
    QCOMPARE(startModel->rowCount(collectionIndex), 3);
    // this is should be enough for now

    // We must have received an added notation for everything except album root
    // and collection
    QCOMPARE(addedIds.size(), 7);

}

void AlbumModelTest::ensureItemCounts()
{
    // trigger listing job
    QEventLoop dAlbumLoop;
    connect(AlbumManager::instance(), SIGNAL(signalAllDAlbumsLoaded()),
            &dAlbumLoop, SLOT(quit()));
    AlbumManager::instance()->prepareItemCounts();
    kDebug() << "Waiting for AlbumManager and the IOSlave to create DAlbums...";
    dAlbumLoop.exec();
    kDebug() << "DAlbums were created";
    while (palbumCountMap.size() < 8)
    {
        QEventLoop pAlbumLoop;
        connect(AlbumManager::instance(), SIGNAL(signalPAlbumsDirty(const QMap<int, int>&)),
                &pAlbumLoop, SLOT(quit()));
        kDebug() << "Waiting for first PAlbum count map";
        pAlbumLoop.exec();
        kDebug() << "Got new PAlbum count map";
    }
}


void AlbumModelTest::slotStartModelRowsInserted(const QModelIndex &parent, int start, int end)
{
	kDebug() << "called, parent:" << parent << ", start:" << start << ", end:" << end;
	for (int row = start; row <= end; ++row)
	{
		QModelIndex child = startModel->index(row, 0, parent);
		QVERIFY(child.isValid());
		Album *album = startModel->albumForIndex(child);
		const int id = child.data(AbstractAlbumModel::AlbumIdRole).toInt();
		QVERIFY(album);
		kDebug() << "added album with id"
				 << id
				 << "and name" << album->title();
		addedIds << id;
	}
}

void AlbumModelTest::slotStartModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {

        QModelIndex index = startModel->index(row, topLeft.column(), topLeft.parent());
        if (!index.isValid())
        {
        	QFAIL("Illegal index received");
            continue;
        }

        int albumId = index.data(AbstractAlbumModel::AlbumIdRole).toInt();

        if (!addedIds.contains(albumId))
        {
        	QString message = "Album id " + QString::number(albumId)
					        + " was changed before adding signal was received";
			QFAIL(message.toAscii().data());
			kError() << message;
        }

    }

}

void AlbumModelTest::deletePAlbum(PAlbum *album)
{
    KUrl u;
    u.setProtocol("file");
    u.setPath(album->folderPath());
    KIO::NetAccess::del(u, 0);
}

void AlbumModelTest::setLastPAlbumCountMap(const QMap<int, int> &map)
{
    kDebug() << "Receiving new count map "<< map;
    palbumCountMap = map;
}

void AlbumModelTest::cleanup()
{

	if (startModel)
	{
		disconnect(startModel);
	}
	delete startModel;
	addedIds.clear();

    // remove all test data

    AlbumManager::instance()->refresh();

    // remove all palbums' directories
    deletePAlbum(palbumRoot0);
    deletePAlbum(palbumRoot1);
    deletePAlbum(palbumRoot2);

    // take over changes to database
    ScanController::instance()->completeCollectionScan();

    // reread from database
    AlbumManager::instance()->refresh();

    // root + one collection
    QCOMPARE(AlbumManager::instance()->allPAlbums().size(), 2);

    // remove all tags

    QString error;
    bool removed = AlbumManager::instance()->deleteTAlbum(talbumRoot0, error);
    QVERIFY2(removed, QString("Error removing a tag: " + error).toAscii());
    removed = AlbumManager::instance()->deleteTAlbum(talbumRoot1, error);
    QVERIFY2(removed, QString("Error removing a tag: " + error).toAscii());

    QCOMPARE(AlbumManager::instance()->allTAlbums().size(), 1);

}

void AlbumModelTest::testPAlbumModel()
{

    AlbumModel *albumModel = new AlbumModel();
    ModelTest *test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;

    albumModel = new AlbumModel(AbstractAlbumModel::IgnoreRootAlbum);
    test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;

}

void AlbumModelTest::testDisablePAlbumCount()
{

    AlbumModel albumModel;
    albumModel.setCountMap(palbumCountMap);
    albumModel.setShowCount(true);

    QRegExp countRegEx(".+ \\(\\d+\\)");
    countRegEx.setMinimal(true);
    QVERIFY(countRegEx.exactMatch("test (10)"));
    QVERIFY(countRegEx.exactMatch("te st (10)"));
    QVERIFY(countRegEx.exactMatch("te st (0)"));
    QVERIFY(!countRegEx.exactMatch("te st ()"));
    QVERIFY(!countRegEx.exactMatch("te st"));
    QVERIFY(!countRegEx.exactMatch("te st (10) bla"));

    // ensure that all albums except the root album have a count attached
    QModelIndex rootIndex = albumModel.index(0, 0, QModelIndex());
    QString rootTitle = albumModel.data(rootIndex, Qt::DisplayRole).toString();
    QVERIFY(!countRegEx.exactMatch(rootTitle));
    for (int collectionRow = 0; collectionRow < albumModel.rowCount(rootIndex); ++collectionRow)
    {
        QModelIndex collectionIndex = albumModel.index(collectionRow, 0, rootIndex);
        QString collectionTitle = albumModel.data(collectionIndex, Qt::DisplayRole).toString();
        QVERIFY2(countRegEx.exactMatch(collectionTitle), QString("'" + collectionTitle + "' matching error").toAscii().data());

        for (int albumRow = 0; albumRow < albumModel.rowCount(collectionIndex); ++albumRow)
        {
            QModelIndex albumIndex = albumModel.index(albumRow, 0, collectionIndex);
            QString albumTitle = albumModel.data(albumIndex, Qt::DisplayRole).toString();
            QVERIFY2(countRegEx.exactMatch(albumTitle), QString("'" + albumTitle + "' matching error").toAscii().data());
        }

    }

    // now disable showing the count
    albumModel.setShowCount(false);

    // ensure that no album has a count attached
    rootTitle = albumModel.data(rootIndex, Qt::DisplayRole).toString();
    QVERIFY(!countRegEx.exactMatch(rootTitle));
    for (int collectionRow = 0; collectionRow < albumModel.rowCount(rootIndex); ++collectionRow)
    {
        QModelIndex collectionIndex = albumModel.index(collectionRow, 0, rootIndex);
        QString collectionTitle = albumModel.data(collectionIndex, Qt::DisplayRole).toString();
        QVERIFY2(!countRegEx.exactMatch(collectionTitle), QString("'" + collectionTitle + "' matching error").toAscii().data());

        for (int albumRow = 0; albumRow < albumModel.rowCount(collectionIndex); ++albumRow)
        {
            QModelIndex albumIndex = albumModel.index(albumRow, 0, collectionIndex);
            QString albumTitle = albumModel.data(albumIndex, Qt::DisplayRole).toString();
            QVERIFY2(!countRegEx.exactMatch(albumTitle), QString("'" + albumTitle + "' matching error").toAscii().data());
        }

    }

}

void AlbumModelTest::testDAlbumModel()
{
    DateAlbumModel *albumModel = new DateAlbumModel();
    ensureItemCounts();
    ModelTest *test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;
}

void AlbumModelTest::testDAlbumContainsAlbums()
{

    DateAlbumModel *albumModel = new DateAlbumModel();
    ensureItemCounts();

    QVERIFY(albumModel->rootAlbum());

    foreach (Album *album, AlbumManager::instance()->allDAlbums())
    {

        DAlbum *dAlbum = dynamic_cast<DAlbum*> (album);
        QVERIFY(dAlbum);

        kDebug() << "checking album for date " << dAlbum->date() << ", range = " << dAlbum->range();

        QModelIndex index = albumModel->indexForAlbum(dAlbum);
        if (!dAlbum->isRoot())
        {
            QVERIFY(index.isValid());
        }

        if (dAlbum->isRoot())
        {
            // root album
            QVERIFY(dAlbum->isRoot());
            QCOMPARE(albumModel->rowCount(index), 3);
            QCOMPARE(index, albumModel->rootAlbumIndex());
        }
        else if (dAlbum->range() == DAlbum::Year && dAlbum->date().year() == 2007)
        {
            QCOMPARE(albumModel->rowCount(index), 2);
        }
        else if (dAlbum->range() == DAlbum::Year && dAlbum->date().year() == 2009)
        {
            QCOMPARE(albumModel->rowCount(index), 3);
        }
        else if (dAlbum->range() == DAlbum::Month && dAlbum->date().year() == 2007 && dAlbum->date().month() == 3)
        {
            QCOMPARE(albumModel->rowCount(index), 0);
        }
        else if (dAlbum->range() == DAlbum::Month && dAlbum->date().year() == 2007 && dAlbum->date().month() == 4)
        {
            QCOMPARE(albumModel->rowCount(index), 0);
        }
        else if (dAlbum->range() == DAlbum::Month && dAlbum->date().year() == 2009 && dAlbum->date().month() == 3)
        {
            QCOMPARE(albumModel->rowCount(index), 0);
        }
        else if (dAlbum->range() == DAlbum::Month && dAlbum->date().year() == 2009 && dAlbum->date().month() == 4)
        {
            QCOMPARE(albumModel->rowCount(index), 0);
        }
        else if (dAlbum->range() == DAlbum::Month && dAlbum->date().year() == 2009 && dAlbum->date().month() == 5)
        {
            QCOMPARE(albumModel->rowCount(index), 0);
        }
        else if (dAlbum->date().year() == 1997)
        {
            // Ignore these albums for order testing
        }
        else
        {
            kError() << "Unexpected album: " << dAlbum->title();
            QFAIL("Unexpected album returned from model");
        }

    }

    delete albumModel;

}

void AlbumModelTest::testDAlbumSorting()
{

    DateAlbumModel dateAlbumModel;
    AlbumFilterModel albumModel;
    albumModel.setSourceAlbumModel(&dateAlbumModel);

    // first check ascending order
    albumModel.sort(0, Qt::AscendingOrder);
    int previousYear = 0;
    for (int yearRow = 0; yearRow < albumModel.rowCount(); ++yearRow)
    {

        QModelIndex yearIndex = albumModel.index(yearRow, 0);
        DAlbum *yearAlbum = dynamic_cast<DAlbum*> (albumModel.albumForIndex(yearIndex));
        QVERIFY(yearAlbum);

        QVERIFY(yearAlbum->date().year() > previousYear);
        previousYear = yearAlbum->date().year();

        int previousMonth = 0;
        for (int monthRow = 0; monthRow < albumModel.rowCount(yearIndex); ++monthRow)
        {

            QModelIndex monthIndex = albumModel.index(monthRow, 0, yearIndex);
            DAlbum *monthAlbum = dynamic_cast<DAlbum*> (albumModel.albumForIndex(monthIndex));
            QVERIFY(monthAlbum);

            QVERIFY(monthAlbum->date().month() > previousMonth);
            previousMonth = monthAlbum->date().month();

        }

    }

    // then check descending order
    albumModel.sort(0, Qt::DescendingOrder);
    previousYear = 1000000;
    for (int yearRow = 0; yearRow < albumModel.rowCount(); ++yearRow)
    {

        QModelIndex yearIndex = albumModel.index(yearRow, 0);
        DAlbum *yearAlbum = dynamic_cast<DAlbum*> (albumModel.albumForIndex(yearIndex));
        QVERIFY(yearAlbum);

        QVERIFY(yearAlbum->date().year() < previousYear);
        previousYear = yearAlbum->date().year();

        int previousMonth = 13;
        for (int monthRow = 0; monthRow < albumModel.rowCount(yearIndex); ++monthRow)
        {

            QModelIndex monthIndex = albumModel.index(monthRow, 0, yearIndex);
            DAlbum *monthAlbum = dynamic_cast<DAlbum*> (albumModel.albumForIndex(monthIndex));
            QVERIFY(monthAlbum);

            QVERIFY(monthAlbum->date().month() < previousMonth);
            previousMonth = monthAlbum->date().month();

        }

    }

}

void AlbumModelTest::testDAlbumCount()
{
    DateAlbumModel *albumModel = new DateAlbumModel();
    albumModel->setShowCount(true);
    ensureItemCounts();

    kDebug() << "iterating over root indices";

    // check year albums
    for (int yearRow = 0; yearRow < albumModel->rowCount(albumModel->rootAlbumIndex()); ++yearRow) {

        QModelIndex yearIndex = albumModel->index(yearRow, 0);
        DAlbum *yearDAlbum = albumModel->albumForIndex(yearIndex);
        QVERIFY(yearDAlbum);

        QVERIFY(yearDAlbum->range() == DAlbum::Year);

        if (yearDAlbum->date().year() == 2007)
        {

            const int imagesInYear = 7;
            albumModel->includeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), imagesInYear);
            albumModel->excludeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), 0);
            albumModel->includeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), imagesInYear);

            for (int monthRow = 0; monthRow < albumModel->rowCount(yearIndex); ++monthRow)
            {

                QModelIndex monthIndex = albumModel->index(monthRow, 0, yearIndex);
                DAlbum *monthDAlbum = albumModel->albumForIndex(monthIndex);
                QVERIFY(monthDAlbum);

                QVERIFY(monthDAlbum->range() == DAlbum::Month);
                QVERIFY(monthDAlbum->date().year() == 2007);

                if (monthDAlbum->date().month() == 3)
                {
                    const int imagesInMonth = 3;
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->excludeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                }
                else if(monthDAlbum->date().month() == 4)
                {
                    const int imagesInMonth = 4;
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->excludeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                }
                else
                {
                    QFAIL("unexpected month album in 2007");
                }

            }

        }
        else if (yearDAlbum->date().year() == 2009)
        {

            const int imagesInYear = 5;
            albumModel->includeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), imagesInYear);
            albumModel->excludeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), 0);
            albumModel->includeChildrenCount(yearIndex);
            QCOMPARE(albumModel->albumCount(yearDAlbum), imagesInYear);

            for (int monthRow = 0; monthRow < albumModel->rowCount(yearIndex); ++monthRow)
            {

                QModelIndex monthIndex = albumModel->index(monthRow, 0, yearIndex);
                DAlbum *monthDAlbum = albumModel->albumForIndex(monthIndex);
                QVERIFY(monthDAlbum);

                QVERIFY(monthDAlbum->range() == DAlbum::Month);
                QVERIFY(monthDAlbum->date().year() == 2009);

                if (monthDAlbum->date().month() == 3)
                {
                    const int imagesInMonth = 2;
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->excludeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                }
                else if(monthDAlbum->date().month() == 4)
                {
                    const int imagesInMonth = 2;
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->excludeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                }
                else if(monthDAlbum->date().month() == 5)
                {
                    const int imagesInMonth = 1;
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->excludeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                    albumModel->includeChildrenCount(monthIndex);
                    QCOMPARE(albumModel->albumCount(monthDAlbum), imagesInMonth);
                }
                else
                {
                    QFAIL("unexpected month album in 2009");
                }

            }

        }
        else if (yearDAlbum->date().year() == 1997)
        {
            // Nothing to do here, ignore the albums for ordering tests
        }
        else
        {
            QFAIL("Received unexpected album from model");
        }

    }

    delete albumModel;
}

void AlbumModelTest::testTAlbumModel()
{

    TagModel *albumModel = new TagModel();
    ModelTest *test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;

    albumModel = new TagModel(AbstractAlbumModel::IgnoreRootAlbum);
    test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;

}

void AlbumModelTest::testSAlbumModel()
{
    SearchModel *albumModel = new SearchModel();
    ModelTest *test = new ModelTest(albumModel, 0);
    delete test;
    delete albumModel;
}
