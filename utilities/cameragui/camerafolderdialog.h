/* ============================================================
 * Author:  Caulier Gilles <caulier dot gilles at kdemail dot net>
 * Date   : 2006-07-24
 * Description : a dialog to select a camera folders.
 * 
 * Copyright 2006 by Gilles Caulier
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

#ifndef CAMERAFOLDERDIALOG_H
#define CAMERAFOLDERDIALOG_H

// Qt includes.

#include <qstring.h>

// KDE includes.

#include <kdialogbase.h>

class CameraFolderView;

namespace Digikam
{

class CameraFolderDialog : public KDialogBase
{
public:

    CameraFolderDialog(QWidget *parent, const QStringList& cameraFolderList,
                       const QString& cameraName);
    ~CameraFolderDialog();

    QString selectedFolderPath();

private:
    
    CameraFolderView *m_folderView;
};

} // namespace Digikam

#endif /* CAMERAFOLDERDIALOG_H */
