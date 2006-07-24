/* ============================================================
 * Authors: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 *          Caulier Gilles <caulier dot gilles at kdemail dot net>
 * Date   : 2003-01-23
 * Description : A widget to display a camera folder.
 * 
 * Copyright 2003-2005 by Renchi Raju
 * Copyright 2006 by Gilles Caulier
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

#ifndef CAMERAFOLDERITEM_H
#define CAMERAFOLDERITEM_H

// Qt includes.

#include <qstring.h>

// KDE includes.

#include <klistview.h>

namespace Digikam
{

class CameraFolderItem : public KListViewItem
{
public:

    CameraFolderItem(KListView* parent,
                     const QString& name);

    CameraFolderItem(KListViewItem* parent,
                     const QString& folderName,
                     const QString& folderPath);

    ~CameraFolderItem();

    QString folderName();
    QString folderPath();
    bool    isVirtualFolder();
    void    changeCount(int val);
    void    setCount(int val);
    int     count();
    
private:

    QString folderName_;
    QString folderPath_;
    QString name_;
    bool    virtualFolder_;
    int     count_;
};

} // namespace Digikam

#endif /* CAMERAFOLDERITEM_H */
