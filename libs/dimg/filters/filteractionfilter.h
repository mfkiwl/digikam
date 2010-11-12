/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2010-11-10
 * Description : meta-filter to apply FilterActions
 *
 * Copyright (C) 2010 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
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

#ifndef FILTERACTIONFILTER_H
#define FILTERACTIONFILTER_H

#include "digikam_export.h"
#include "dimgthreadedfilter.h"
#include "filteraction.h"

namespace Digikam
{

class DIGIKAM_EXPORT FilterActionFilter : public DImgThreadedFilter
{
public:

    /**
     * A meta-filter applying other filter according to a list of FilterActions
     */

    FilterActionFilter(QObject* parent = 0);
    ~FilterActionFilter();

    /**
     * Set - or add to existing list - the given filter actions
     */
    void setFilterActions(const QList<FilterAction>& actions);
    void addFilterActions(const QList<FilterAction>& actions);
    void setFilterAction(const FilterAction& action);
    void addFilterAction(const FilterAction& action);

    QList<FilterAction> filterActions() const;

    /**
     * Returns true if all FilterActions are reproducible
     */
    bool isReproducible() const;

    /**
     * Returns true if all FilterActions are reproducible
     * or are ComplexFilters. That means the identical result
     * may not be reproducible, but a sufficiently similar result
     * may be available and apply will probably complete.
     */
    bool isComplexAction() const;

    /**
     * Returns true if all actions are supported.
     */
    bool isSupported() const;

    /**
     * After the thread was run, you can find out if application was successful.
     * A precondition is that at least isComplexAction() and isSupported() returns true.
     * If all filters applied cleanly, completelyApplied() returns true.
     * appliedActions() returns all applied actions, if completelyApplied(),
     * the same as filterActions().
     * If not completelyApplied, failedAction() returns the action that failed,
     * failedActionIndex its index in filterActions(), and failedActionMessage
     * an optional error message.
     * Note that finished(true) does not mean that completelyApplied() is also true.
     */
    bool completelyApplied() const;
    QList<FilterAction> appliedActions();
    FilterAction failedAction() const;
    int failedActionIndex() const;
    QString failedActionMessage() const;

    /**
     * These methods dont make sense here. Use filterActions.
     */
    virtual FilterAction filterAction() { return FilterAction(); }
    virtual void readParameters(const FilterAction&) {}
    virtual QString filterIdentifier() const { return QString(); }

protected:

    virtual void filterImage();

private:

    class FilterActionFilterPriv;
    FilterActionFilterPriv* const d;
};

} // namespace Digikam

#endif // FILTERACTIONFILTER_H
