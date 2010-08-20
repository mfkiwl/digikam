/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2010-07-20
 * Description : GPS searck marker tiler
 *
 * Copyright (C) 2010 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * Copyright (C) 2010 by Gabriel Voicu <ping dot gabi at gmail dot com>
 * Copyright (C) 2010 by Michael G. Hansen <mike at mghansen dot de>
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

#include "gpsmarkertiler.moc"

// Qt includes

#include <QPair>
#include <QRectF>

namespace Digikam
{

class EntryFromDatabase
{
    public:
        qlonglong                   id;
        int                         rating;
        QDateTime                   creationDate;    
        KMap::GeoCoordinates coordinate;
};

class InternalJobs
{
public:

    InternalJobs()
        : dataFromDatabase()
    {
        kioJob = 0;
        level = 0;
    }

    KIO::Job* kioJob;
    int level;


    QList<EntryFromDatabase> dataFromDatabase;
};

/**
 * @class GPSMarkerTiler
 * 
 * @brief Marker model for storing data needed to display markers on the map. The data is retrieved from Digikam's database.
 */

class GPSMarkerTiler::GPSMarkerTilerPrivate
{
public:

    GPSMarkerTilerPrivate()
        : returnedImageInfo(),
          levelList(),
          jobsList(),
          dataFromDatabaseList(),
          activeState(true)
    {
        mapImagesJob = 0;
    }

    KIO::TransferJob*                                              mapImagesJob;
    QList<GPSMarkerTiler::MyTile::ImageFromTileInfo>               returnedImageInfo;
    QList<int>                                                     levelList;
    QList<KIO::Job*>                                               jobsList;
    QList<QList<QVariant> >                                        dataFromDatabaseList;
    QList<InternalJobs>                                            jobs;
    ThumbnailLoadThread*                                           thumbnailLoadThread;
    QMap<QString, QVariant>                                        thumbnailMap;
    QList<QRectF>                                                  rectList;
    QList<int>                                                     rectLevel;
    bool                                                           activeState; 
};

/**
 * @brief Constructor
 * @param parent Parent object
 */
GPSMarkerTiler::GPSMarkerTiler(QObject* const parent)
              : KMap::AbstractMarkerTiler(parent), d(new GPSMarkerTilerPrivate())
{
    resetRootTile();

    d->thumbnailLoadThread = new ThumbnailLoadThread();

    connect(d->thumbnailLoadThread, SIGNAL(signalThumbnailLoaded(const LoadingDescription&, const QPixmap&)),
            this, SLOT(slotThumbnailLoaded(const LoadingDescription&, const QPixmap&)));
}

/**
 * @brief Destructor
 */
GPSMarkerTiler::~GPSMarkerTiler()
{
    delete d;
}

void GPSMarkerTiler::regenerateTiles()
{
}

/**
 * @brief This function calls the database for the images found inside a rectangle defined by upperLeft and lowerRight points. The images 
 * are returned from database in batches.
 * @param upperLeft The North-West point.
 * @param lowerRight The South-East point.
 * @param level The requested tiling level.
 */
void GPSMarkerTiler::prepareTiles(const KMap::GeoCoordinates& upperLeft,const KMap::GeoCoordinates& lowerRight, int level)
{

    qreal lat1 = upperLeft.lat();
    qreal lng1 = upperLeft.lon();
    qreal lat2 = lowerRight.lat();
    qreal lng2 = lowerRight.lon();
    QRect requestedRect(lat1, lng1, lat2-lat1, lng2-lng1);

    for (int i=0; i<d->rectList.count(); ++i)
    {
        if (level != d->rectLevel.at(i))
            continue;

        qreal rectLat1, rectLng1, rectLat2, rectLng2;
        QRectF currentRect = d->rectList.at(i);
        currentRect.getCoords(&rectLat1, &rectLng1, &rectLat2, &rectLng2);
 
        //do nothing if this rectangle was already requested
        if (currentRect.contains(requestedRect))
            return;

        if (currentRect.contains(lat1,lng1))
        {
            if (currentRect.contains(lat2, lng1))
            {
                lng1 = rectLng2;
                break;
            }
        }
        else if (currentRect.contains(lat2, lng1))
        {
            if (currentRect.contains(lat2, lng2))
            {
                lat2 = rectLng1;
                break;
            }
        }
        else if (currentRect.contains(lat2, lng2))
        {
            if (currentRect.contains(lat1, lng2))
            {
                lng2 = rectLng1;
                break;
            }
        }
        else if (currentRect.contains(lat1, lng2))
        {
            if (currentRect.contains(lat1, lng1))
            {
                lat1 = rectLat2;
                break;
            }
        }

    }

    QRectF newRect(lat1, lng1, lat2-lat1, lng2-lng1);
    d->rectList.append(newRect);
    d->rectLevel.append(level);

    kDebug() << "Listing" << lat1 << lat2 << lng1 << lng2;
    DatabaseUrl u = DatabaseUrl::fromAreaRange(lat1, lat2, lng1, lng2);
    KIO::Job* currentJob = ImageLister::startListJob(u);
    currentJob->addMetaData("wantDirectQuery", "false");

    InternalJobs currentJobInfo;
    currentJobInfo.kioJob           = currentJob;
    currentJobInfo.level            = level;

    d->jobs.append(currentJobInfo);

    connect(currentJob, SIGNAL(result(KJob*)),
            this, SLOT(slotMapImagesJobResult(KJob*)));

    connect(currentJob, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(slotMapImagesJobData(KIO::Job*, const QByteArray&)));
}

/**
 * @brief Returns a pointer to a tile.
 * @param tileIndex The index of a tile.
 * @param stopIfEmpty Determines whether child tiles are also created for empty tiles.
 */
KMap::AbstractMarkerTiler::Tile* GPSMarkerTiler::getTile(const KMap::AbstractMarkerTiler::TileIndex& tileIndex, const bool stopIfEmpty)
{
//   if (isDirty())
//   {
//       regenerateTiles();
//   }

    KMAP_ASSERT(tileIndex.level()<=TileIndex::MaxLevel);

    MyTile* tile = static_cast<MyTile*>(rootTile());
    for (int level = 0; level < tileIndex.indexCount(); ++level)
    {
        const int currentIndex = tileIndex.linearIndex(level);

        MyTile* childTile = 0;
        if (tile->children.isEmpty())
        {
            tile->prepareForChildren(KMap::QIntPair(TileIndex::Tiling, TileIndex::Tiling));

            for (int i=0; i<tile->imagesFromTileInfo.count(); ++i)
            {
                MyTile::ImageFromTileInfo currentImageInfo = tile->imagesFromTileInfo.at(i);
                const TileIndex markerTileIndex = TileIndex::fromCoordinates(currentImageInfo.coordinate, level);
                const int newTileIndex = markerTileIndex.toIntList().last();

                MyTile* newTile = static_cast<MyTile*>(tile->children.at(newTileIndex));

                if (newTile == 0)
                {
                    MyTile* newTile = static_cast<MyTile*>(tileNew());
                    newTile->imagesFromTileInfo.append(currentImageInfo);
                    tile->addChild(newTileIndex, newTile);
                }
                else
                {
                    bool found = false;
                    for (int j=0; j<newTile->imagesFromTileInfo.count(); ++j)
                    {
                        if (newTile->imagesFromTileInfo.at(j).id == currentImageInfo.id)
                            found = true;

                        if (!found)
                            newTile->imagesFromTileInfo.append(currentImageInfo);
                    }
                }
            }

            //return 0;
        }
        childTile = static_cast<MyTile*>(tile->children.at(currentIndex));

        if (childTile==0)
        {
            if (stopIfEmpty)
            {
                // there will be no markers in this tile, therefore stop
                return 0;
            }
            //childTile = new KMap::AbstractMarkerTiler::Tile();
            //tile->addChild(currentIndex, childTile);
        }
        tile = childTile;
    }
    return tile;
}

/**
 * @param tileIndex The index of the current tile.
 * @return The number of markers found inside a tile.
 */
int GPSMarkerTiler::getTileMarkerCount(const KMap::AbstractMarkerTiler::TileIndex& tileIndex)
{
    MyTile* tile = static_cast<MyTile*>(getTile(tileIndex));
    if (tile)
        return tile->imagesFromTileInfo.count();

    return 0;
}

int GPSMarkerTiler::getTileSelectedCount(const KMap::AbstractMarkerTiler::TileIndex& tileIndex)
{
    Q_UNUSED(tileIndex)

    return 0;
}

/**
 @brief This function finds the best representative marker from a group of markers. This is needed to display a thumbnail for a marker group.
 * @param indices A list containing markers.
 * @param sortKey Sets the criteria for selecting the representative thumbnail. When sortKey == 0 the most youngest thumbnail is chosen, when sortKey == 1 the most oldest thumbnail is chosen and when sortKey == 2 the thumbnail with the highest rating is chosen(if 2 thumbnails have the same rating, the youngest one is chosen).
 * @return Returns the index of the marker.
 */
QVariant GPSMarkerTiler::getTileRepresentativeMarker(const KMap::AbstractMarkerTiler::TileIndex& tileIndex, const int sortKey)
{
    //TODO: sort the markers using sortKey 

    MyTile* tile = static_cast<MyTile*>(getTile(tileIndex, true));
    QPair<KMap::AbstractMarkerTiler::TileIndex, int> bestRep;
    QVariant v;

    if (tile != NULL)
    {
        if (tile->imagesFromTileInfo.count() == 0)
            return QVariant();

        MyTile::ImageFromTileInfo bestMarkerInfo =
                                                    tile->imagesFromTileInfo.first();

        for (int i=0; i<tile->imagesFromTileInfo.count(); ++i)
        {
            if (sortKey == SortYoungestFirst)
            {
                if (tile->imagesFromTileInfo.at(i).creationDate < bestMarkerInfo.creationDate)
                {
                    bestMarkerInfo = tile->imagesFromTileInfo.at(i);
                }
            }
            else if (sortKey == SortOldestFirst)
            {
                if (tile->imagesFromTileInfo.at(i).creationDate > bestMarkerInfo.creationDate)
                {
                    bestMarkerInfo = tile->imagesFromTileInfo.at(i);
                }
            }
            else
            {
                if (tile->imagesFromTileInfo.at(i).rating > bestMarkerInfo.rating)
                {
                    bestMarkerInfo = tile->imagesFromTileInfo.at(i);
                }
                else if (tile->imagesFromTileInfo.at(i).rating == bestMarkerInfo.rating)
                {
                    if (tile->imagesFromTileInfo.at(i).creationDate < bestMarkerInfo.creationDate)
                    {
                        bestMarkerInfo = tile->imagesFromTileInfo.at(i);  
                    }
                }
            }
        }

        bestRep.first  = tileIndex;
        bestRep.second = bestMarkerInfo.id;
        const QPair<TileIndex, int> returnedMarker = bestRep;
        v.setValue(bestRep);
        return v;
    }
    return QVariant();
}

/**
 * @brief This function finds the best representative marker from a group of markers. This is needed to display a thumbnail for a marker group.
 * @param indices A list containing markers.
 * @param sortKey Sets the criteria for selecting the representative thumbnail. When sortKey == 0 the most youngest thumbnail is chosen, when sortKey == 1 the most oldest thumbnail is chosen and when sortKey == 2 the thumbnail with the highest rating is chosen(if 2 thumbnails have the same rating, the youngest one is chosen).
 * @return Returns the index of the marker.
 */
QVariant GPSMarkerTiler::bestRepresentativeIndexFromList(const QList<QVariant>& indices, const int sortKey)
{
    QVariant v;
    QPair<TileIndex, int> bestRep;
    MyTile::ImageFromTileInfo bestMarkerInfo;

    for (int i=0; i<indices.count(); ++i)
    {
        QPair<TileIndex, int> currentIndex = indices.at(i).value<QPair<TileIndex, int> >();

        MyTile* tile = static_cast<MyTile*>(getTile(currentIndex.first, true));

        for (int j=0; j<tile->imagesFromTileInfo.count(); ++j)
        {
            if (tile->imagesFromTileInfo.at(j).id == currentIndex.second)
            {
                if (bestMarkerInfo.id == -2)
                {
                    bestRep = currentIndex;
                    bestMarkerInfo = tile->imagesFromTileInfo.at(j);
                }
                else
                {
                    if (sortKey == SortYoungestFirst)
                    {
                        if (tile->imagesFromTileInfo.at(j).creationDate < bestMarkerInfo.creationDate)
                        {
                            bestRep        = currentIndex;
                            bestMarkerInfo = tile->imagesFromTileInfo.at(j);
                        }
                    }
                    else if (sortKey == SortOldestFirst)
                    {
                        if (tile->imagesFromTileInfo.at(j).creationDate > bestMarkerInfo.creationDate)
                        {
                            bestRep        = currentIndex;
                            bestMarkerInfo = tile->imagesFromTileInfo.at(j);
                        }
                    }
                    else
                    {
                        if (tile->imagesFromTileInfo.at(j).rating > bestMarkerInfo.rating)
                        {
                            bestRep        = currentIndex;
                            bestMarkerInfo = tile->imagesFromTileInfo.at(j);
                        }
                        else if (tile->imagesFromTileInfo.at(j).rating == bestMarkerInfo.rating)
                        {
                            if (tile->imagesFromTileInfo.at(j).creationDate < bestMarkerInfo.creationDate)
                            {
                                bestRep        = currentIndex;
                                bestMarkerInfo = tile->imagesFromTileInfo.at(j);
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    v.setValue(bestRep);
    return QVariant(v);
}

/**
 * @brief This function retrieves the thumbnail for an index.
 * @param index The marker's index.
 * @param size The size of the thumbnail.
 * @return If the thumbnail has been loaded in the ThumbnailLoadThread instance, it is returned. If not, a QPixmap is returned and ThumbnailLoadThread's signal named signalThumbnailLoaded is emited when the thumbnail becomes available.
 */
QPixmap GPSMarkerTiler::pixmapFromRepresentativeIndex(const QVariant& index, const QSize& size)
{
    QPair<TileIndex, int> indexForPixmap = index.value<QPair<TileIndex,int> >();

    QPixmap thumbnail;
    ImageInfo info(indexForPixmap.second);
    QString path = info.filePath();
    d->thumbnailMap.insert(path, index);

    if (d->thumbnailLoadThread->find(path, thumbnail, qMax(size.width(), size.height())))
    {
        return thumbnail;
    }
    else
    {
        return QPixmap();
    }
}

/**
 * @brief This function compares two marker indices.
 */
bool GPSMarkerTiler::indicesEqual(const QVariant& a, const QVariant& b) const
{
    QPair<TileIndex, int> firstIndex = a.value<QPair<TileIndex,int> >();
    QPair<TileIndex, int> secondIndex = b.value<QPair<TileIndex,int> >();

    QList<int> aIndicesList = firstIndex.first.toIntList();
    QList<int> bIndicesList = secondIndex.first.toIntList();

    if (firstIndex.second == secondIndex.second && aIndicesList == bIndicesList)   
        return true;

    return false;
}

KMap::WMWSelectionState GPSMarkerTiler::getTileSelectedState(const KMap::AbstractMarkerTiler::TileIndex& tileIndex)
{
    Q_UNUSED(tileIndex)

    return KMap::WMWSelectionState();
}

/**
 * @brief The marker data is returned from the database in batches. This function takes and unites the batches.
 */
void GPSMarkerTiler::slotMapImagesJobData(KIO::Job* job, const QByteArray& data)
{
    if (data.isEmpty())
    {
        return;
    }

    QByteArray di(data);
    QDataStream ds(&di, QIODevice::ReadOnly);

    QList<EntryFromDatabase> newEntries;
    while (!ds.atEnd())
    {
        ImageListerRecord record(ImageListerRecord::ExtraValueFormat);
        ds >> record;

        EntryFromDatabase entry;

        entry.id           = record.imageID;
        entry.rating       = record.rating;
        entry.creationDate = record.creationDate; 
        if (!record.extraValues.count() < 2)
            entry.coordinate.setLatLon(record.extraValues.first().toDouble(), record.extraValues.last().toDouble());

        newEntries << entry;
    }

    for (int i=0; i<d->jobs.count(); ++i)
    {
        if (job == d->jobs.at(i).kioJob)
        {
            d->jobs[i].dataFromDatabase << newEntries;
        }
    }
}

/**
 * @brief Now, all the marker data has been retrieved from the database. Here, the markers are sorted into tiles.
 */
void GPSMarkerTiler::slotMapImagesJobResult(KJob* job)
{
    if (job->error())
    {
        kWarning()<<"Failed to list images in selected area:"<<job->errorString();
        return;
    }

    KIO::Job* currentJob = qobject_cast<KIO::Job*>(job);
    QList<MyTile::ImageFromTileInfo> currentReturnedImageInfo;

    int foundIndex = -1;
    for (int i=0; i<d->jobs.count(); ++i)
    {
        if (currentJob == d->jobs.at(i).kioJob)
        {
            foundIndex = i;

            foreach (const EntryFromDatabase& entry, d->jobs.at(i).dataFromDatabase)
            {
                MyTile::ImageFromTileInfo info;

                info.id               = entry.id;
                info.rating           = entry.rating;
                info.creationDate     = entry.creationDate;
                info.coordinate       = entry.coordinate;

                currentReturnedImageInfo << info;
            }
        }
    }

    if (foundIndex != -1)
    {

    int wantedLevel = d->jobs.at(foundIndex).level;
    QList<MyTile::ImageFromTileInfo> resultedImages = currentReturnedImageInfo;
    currentReturnedImageInfo.clear();

    for (int i=0; i<resultedImages.count(); ++i)
    {
        MyTile* currentTile = static_cast<MyTile*>(rootTile());

        MyTile::ImageFromTileInfo currentImageInfo = resultedImages.at(i);

        for (int currentLevel = 0; currentLevel <= wantedLevel+1; ++currentLevel)
        {
            bool found = false;
            for (int counter = 0; counter < currentTile->imagesFromTileInfo.count(); ++counter)
                if (currentImageInfo.id == currentTile->imagesFromTileInfo.at(counter).id)
                    found = true;

            if (!found)
                currentTile->imagesFromTileInfo.append(currentImageInfo);

            if (currentTile->children.isEmpty())
                currentTile->prepareForChildren(KMap::QIntPair(TileIndex::Tiling, 
								    TileIndex::Tiling));

            const TileIndex markerTileIndex = 
                         TileIndex::fromCoordinates(currentImageInfo.coordinate, currentLevel);
            const int newTileIndex = markerTileIndex.toIntList().last();

            MyTile* newTile = static_cast<MyTile*>(currentTile->children.at(newTileIndex));

            if (newTile == 0)
            {
                newTile = static_cast<MyTile*>(tileNew());

                if (currentLevel == wantedLevel+1)
                {
                    newTile->imagesFromTileInfo.append(currentImageInfo);
                }                

                currentTile->addChild(newTileIndex, newTile);
                currentTile = newTile;
            }
            else
            {
                currentTile = newTile;
            }
        }
    }

    d->jobs[foundIndex].kioJob->kill();
    d->jobs[foundIndex].kioJob = 0;
    d->jobs.removeAt(foundIndex);
    }
}

/**
 * @brief Because of a call to pixmapFromRepresentativeIndex, some thumbnails are not yet loaded at the time of requesting. When each thumbnail loads, this slot is called and emits a signal that announces the map that the thumbnail is available.
 */
void GPSMarkerTiler::slotThumbnailLoaded(const LoadingDescription& loadingDescription, const QPixmap& thumbnail)
{
    QVariant index = d->thumbnailMap.value(loadingDescription.filePath);
    QPair<KMap::AbstractMarkerTiler::TileIndex, int> indexForPixmap = 
                              index.value<QPair<KMap::AbstractMarkerTiler::TileIndex,int> >();    
    emit signalThumbnailAvailableForIndex(index, thumbnail);
}

/**
 * @brief Sets the map active/inactive
 * @param state When is true, the map is active.
 */
void GPSMarkerTiler::setActive(const bool state)
{
    d->activeState = state;
}

KMap::AbstractMarkerTiler::Tile* GPSMarkerTiler::tileNew()
{
    return new MyTile();
}

void GPSMarkerTiler::tileDelete(KMap::AbstractMarkerTiler::Tile* const tile)
{
    delete static_cast<MyTile*>(tile);
}

} // namespace Digikam
