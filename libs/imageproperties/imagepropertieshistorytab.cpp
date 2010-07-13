/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2010-06-23
 * Description : a tab to display image editing history
 *
 * Copyright (C) 2010 by Martin Klapetek <martin dot klapetek at gmail dot com>
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

// Qt includes

#include <QGridLayout>
#include <QTreeView>
#include <QMenu>

// KDE includes

#include <KUrl>
#include <KDebug>

// Local includes

#include "imagepropertieshistorytab.h"
#include "imagefiltershistorymodel.h"
#include "imagefiltershistorytreeitem.h"
#include "imagefiltershistoryitemdelegate.h"

namespace Digikam {

class ImagePropertiesHistoryTabPriv {
public:
    ImagePropertiesHistoryTabPriv()
    {
        view     = 0;
        model    = 0;
        layout   = 0;
        delegate = 0;
    }
    
    QTreeView *view;
    ImageFiltersHistoryModel *model;
    QGridLayout *layout;
    ImageFiltersHistoryItemDelegate *delegate;
};

RemoveFilterAction::RemoveFilterAction(QString& label, QModelIndex& index, QObject* parent)
                  : QAction(label, parent)
{
    m_index = index;
}

  
ImagePropertiesHistoryTab::ImagePropertiesHistoryTab(QWidget* parent) 
                         : QWidget(parent), d(new ImagePropertiesHistoryTabPriv)
{
    d->layout = new QGridLayout(this);
    d->view = new QTreeView(this);
    d->delegate = new ImageFiltersHistoryItemDelegate(this);
    d->model = new ImageFiltersHistoryModel(0, KUrl());

    d->layout->addWidget(d->view);

    d->view->setItemDelegate(d->delegate);
    d->view->setRootIsDecorated(false);
    d->view->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(d->view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showCustomContextMenu(QPoint)));
}

ImagePropertiesHistoryTab::~ImagePropertiesHistoryTab()
{
    delete d->model;
    delete d->view;
    delete d->delegate;
}

void ImagePropertiesHistoryTab::setCurrentURL(const KUrl& url)
{
     d->model->setUrl(url);
     d->view->setModel(d->model);
     d->view->update();
}

void ImagePropertiesHistoryTab::showCustomContextMenu(const QPoint& position)
{
    QList<QAction *> actions;
    if(d->view->indexAt(position).isValid()) {
        QModelIndex index = d->view->indexAt(position);

        QString s("Remove entry");
        RemoveFilterAction *removeFilterAction = new RemoveFilterAction(s, index, 0);

        if(!index.model()->parent(index).isValid()) {
            actions.append(removeFilterAction);
            connect(removeFilterAction, SIGNAL(triggered()), removeFilterAction, SLOT(triggerSlot()));
            connect(removeFilterAction, SIGNAL(actionTriggered(QModelIndex)), d->model, SLOT(removeEntry(QModelIndex)));
        }
    }
    if(actions.count() > 0)
        QMenu::exec(actions, d->view->mapToGlobal(position));
}


} //namespace Digikam