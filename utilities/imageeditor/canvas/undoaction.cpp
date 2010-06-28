/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2005-02-06
 * Description : undo actions manager for image editor.
 *
 * Copyright (C) 2005 by Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Copyright (C) 2005 by Joern Ahrens <joern.ahrens@kdemail.net>
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

#include "undoaction.h"

// KDE includes

#include <kdebug.h>

// Local includes

#include "dimginterface.h"

namespace Digikam
{

UndoAction::UndoAction(DImgInterface* iface)
          : m_iface(iface)
{
    m_title = i18n("unknown");
    m_history = iface->getImageHistory();
}

UndoAction::~UndoAction()
{
}

QString UndoAction::getTitle() const
{
    return m_title;
}

DImageHistory UndoAction::getHistory() const
{
    return m_history;
}

// ---------------------------------------------------------------------------------------------

UndoActionRotate::UndoActionRotate(DImgInterface* iface,
                                   UndoActionRotate::Angle angle)
                : UndoAction(iface), m_angle(angle)
{
    switch(m_angle)
    {
        case(R90):
            m_title = i18n("Rotate 90 Degrees");
            break;
        case(R180):
            m_title = i18n("Rotate 180 Degrees");
            break;
        case(R270):
            m_title = i18n("Rotate 270 Degrees");
            break;
    }
}

UndoActionRotate::~UndoActionRotate()
{
}

void UndoActionRotate::rollBack()
{
    switch(m_angle)
    {
        case(R90):
            m_iface->rotate270(false);
            return;
        case(R180):
            m_iface->rotate180(false);
            return;
        case(R270):
            m_iface->rotate90(false);
            return;
        default:
            kWarning() << "Unknown rotate angle specified";
    }
}

void UndoActionRotate::execute()
{
    switch(m_angle)
    {
        case R90:
            m_iface->rotate90(false);
            return;
        case R180:
            m_iface->rotate180(false);
            return;
        case R270:
            m_iface->rotate270(false);
            return;
        default:
            kWarning() << "Unknown rotate angle specified";
    }
}

// ---------------------------------------------------------------------------------------------

UndoActionFlip::UndoActionFlip(DImgInterface* iface, UndoActionFlip::Direction dir)
              : UndoAction(iface), m_dir(dir)
{
    if (m_dir == Horizontal)
        m_title = i18n("Flip Horizontal");
    else if (m_dir == Vertical)
        m_title = i18n("Flip Vertical");
}

UndoActionFlip::~UndoActionFlip()
{
}

void UndoActionFlip::rollBack()
{
    switch(m_dir)
    {
        case Horizontal:
            m_iface->flipHoriz(false);
            return;
        case Vertical:
            m_iface->flipVert(false);
            return;
        default:
            kWarning() << "Unknown flip direction specified";
    }
}

void UndoActionFlip::execute()
{
    rollBack();
}

// ---------------------------------------------------------------------------------------------

UndoActionIrreversible::UndoActionIrreversible(DImgInterface* iface, const QString& title)
                      : UndoAction(iface)
{
    m_title = title;
}

UndoActionIrreversible::~UndoActionIrreversible()
{
}

void UndoActionIrreversible::rollBack()
{
}

void UndoActionIrreversible::execute()
{
}

}  // namespace Digikam
