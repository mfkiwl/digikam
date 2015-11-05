/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2007-06-07
 * Description : Face database backend
 *
 * Copyright (C) 2007-2010 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * Copyright (C) 2010-2015 by Gilles Caulier <caulier dot gilles at gmail dot com>
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

#ifndef _DATABASE_FACE_BACKEND_H_
#define _DATABASE_FACE_BACKEND_H_

// Local includes

#include "digikam_export.h"
#include "databasecorebackend.h"

using namespace Digikam;

namespace FacesEngine
{

class DatabaseFaceSchemaUpdater;
class DatabaseFaceBackendPrivate;

class DIGIKAM_DATABASE_EXPORT DatabaseFaceBackend : public DatabaseCoreBackend
{
    Q_OBJECT

public:

    explicit DatabaseFaceBackend(DatabaseLocking* const locking, const QString& backendName = QLatin1String("faceDatabase-"));
    ~DatabaseFaceBackend();

    /**
     * Initialize the database schema to the current version,
     * carry out upgrades if necessary.
     * Shall only be called from the thread that called open().
     */
    bool initSchema(DatabaseFaceSchemaUpdater* const updater);

private:

    Q_DECLARE_PRIVATE(DatabaseCoreBackend)
};

} // namespace FacesEngine

#endif // _DATABASE_FACE_BACKEND_H_
