/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2012-02-05
 * Description : film color negative inverter filter
 *
 * Copyright (C) 2012 by Matthias Welwarsky <matthias at welwarsky dot de>
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

#include <QListWidget>

// KDE includes

#include <kdebug.h>
#include <cmath>

// Local includes

#include "filmfilter.h"
#include "invertfilter.h"
#include "bcgfilter.h"

namespace Digikam
{

class FilmProfile
{
public:

    FilmProfile(double rdm=0, double gdm=0, double bdm=0)
        : redDmax(rdm),
          greenDmax(gdm),
          blueDmax(bdm),
          rGamma(1.0),
          gGamma(1.0),
          bGamma(1.0),
          wpRed(1.0),
          wpGreen(1.0),
          wpBlue(1.0)
    {
    }

    double dmax(int channel) const
    {
        switch (channel)
        {
            case RedChannel:    return redDmax;
            case GreenChannel:  return greenDmax;
            case BlueChannel:   return blueDmax;
            default:            return 0.0;
        }
    }

    FilmProfile& setGamma(double rG, double gG, double bG)
    {
        rGamma = rG;
        gGamma = gG;
        bGamma = bG;
        return *this;
    }

    double gamma(int channel) const
    {
        switch (channel)
        {
            case RedChannel:    return rGamma;
            case GreenChannel:  return gGamma;
            case BlueChannel:   return bGamma;
            default:            return 1.0;
        }
    }

    FilmProfile& setWp(double rWp, double gWp, double bWp)
    {
        wpRed   = rWp;
        wpGreen = gWp;
        wpBlue  = bWp;
        return *this;
    }

    double wp(int channel) const
    {
        switch (channel)
        {
            case RedChannel:    return wpRed;
            case GreenChannel:  return wpGreen;
            case BlueChannel:   return wpBlue;
            default:            return 1.0;
        }
    }

private:

    double redDmax;
    double greenDmax;
    double blueDmax;

    double rGamma;
    double gGamma;
    double bGamma;

    double wpRed;
    double wpGreen;
    double wpBlue;
};

// --------------------------------------------------------------------------------------------------------

class FilmContainer::FilmContainerPriv
{
public:

    FilmContainerPriv()
        : gamma(1.0),
          exposure(1.0),
          sixteenBit(false),
          profile(FilmProfile(1.0, 1.0, 1.0)),
          cnType(CNNeutral),
          whitePoint(DColor(QColor("white"), false))
    {
    }

    double        gamma;
    double        exposure;
    bool          sixteenBit;
    FilmProfile   profile;
    CNFilmProfile cnType;
    DColor        whitePoint;
};

FilmContainer::FilmContainer() :
    d(QSharedPointer<FilmContainerPriv>(new FilmContainerPriv))
{
}

FilmContainer::FilmContainer(CNFilmProfile profile, double gamma, bool sixteenBit)
    : d(QSharedPointer<FilmContainerPriv>(new FilmContainerPriv))
{
    d->gamma      = gamma;
    d->sixteenBit = sixteenBit;
    d->whitePoint = DColor(QColor("white"), sixteenBit);
    setCNType(profile);
}

void FilmContainer::setWhitePoint(const DColor& wp)
{
    d->whitePoint = wp;
}

DColor FilmContainer::whitePoint() const
{
    return d->whitePoint;
}

void FilmContainer::setExposure(double strength)
{
    d->exposure = strength;
}

double FilmContainer::exposure() const
{
    return d->exposure;
}

void FilmContainer::setSixteenBit(bool val)
{
    d->sixteenBit = val;
}

void FilmContainer::setGamma(double val)
{
    d->gamma = val;
}
double FilmContainer::gamma() const
{
    return d->gamma;
}

void FilmContainer::setCNType(CNFilmProfile profile)
{
    d->cnType = profile;

    switch (profile)
    {
        default:
            d->profile = FilmProfile(1.0, 1.0, 1.0);
            d->cnType = CNNeutral;
            break;
        case CNKodakGold100:
            d->profile = FilmProfile(1.53, 2.00, 2.40); // check
            break;
        case CNKodakGold200:
            d->profile = FilmProfile(1.55, 2.02, 2.42)
                              .setWp(1.00, 1.00, 1.00)
                           .setGamma(1.00, 1.01, 1.10); // ok
            break;
        case CNKodakEktar100:
            d->profile = FilmProfile(1.40, 1.85, 2.34)
                              .setWp(1, 1, 0.88)
                           .setGamma(1, 1.05, 1.25); // check
            break;
        case CNKodakProfessionalPortra160NC:
            d->profile = FilmProfile(1.49, 1.96, 2.46); // check
            break;
        case CNKodakProfessionalPortra160VC:
            d->profile = FilmProfile(1.56, 2.03, 2.55); // check
            break;
        case CNKodakProfessionalPortra400NC:
            d->profile = FilmProfile(1.69, 2.15, 2.69); // check
            break;
        case CNKodakProfessionalPortra400VC:
            d->profile = FilmProfile(1.78, 2.21, 2.77); // check
            break;
        case CNKodakProfessionalPortra800Box:
            d->profile = FilmProfile(1.89, 2.29, 2.89); // check
            break;
        case CNKodakProfessionalPortra800P1:
            d->profile = FilmProfile(1.53, 2.01, 2.46); // check
            break;
        case CNKodakProfessionalPortra800P2:
            d->profile = FilmProfile(1.74, 2.22, 2.64); // check
            break;
        case CNKodakProfessionalNewPortra160:
            d->profile = FilmProfile(1.41, 1.88, 2.32)
                              .setWp(1, 1, 0.92)
                           .setGamma(1, 1.05, 1.25); // check
            break;
        case CNKodakProfessionalNewPortra400:
            d->profile = FilmProfile(1.69, 2.15, 2.68); // check
            break;
        case CNKodakFarbwelt100:
            d->profile = FilmProfile(1.86, 2.33, 2.77); // fix, check
            break;
        case CNKodakFarbwelt200:
            d->profile = FilmProfile(1.82, 2.29, 2.72); // fix, check
            break;
        case CNKodakFarbwelt400:
            d->profile = FilmProfile(1.93, 2.43, 2.95); // fix, check
            break;
        case CNKodakRoyalGold400:
            d->profile = FilmProfile(2.24, 2.76, 3.27); // fix, check
            break;
        case CNAgfaphotoVistaPlus200:
            d->profile = FilmProfile(1.89, 2.38, 2.67); // fix, check
            break;
        case CNAgfaphotoVistaPlus400:
            d->profile = FilmProfile(2.08, 2.56, 2.85); // fix, check
            break;
        case CNFujicolorPro160S:
            d->profile = FilmProfile(1.73, 2.27, 2.53); // fix, check
            break;
        case CNFujicolorPro160C:
            d->profile = FilmProfile(1.96, 2.46, 2.69); // fix, check
            break;
        case CNFujicolorNPL160:
            d->profile = FilmProfile(2.13, 2.36, 2.92); // fix, check
            break;
        case CNFujicolorPro400H:
            d->profile = FilmProfile(1.95, 2.37, 2.62); // fix, check
            break;
        case CNFujicolorPro800Z:
            d->profile = FilmProfile(2.12, 2.37, 2.56); // fix, check
            break;
        case CNFujicolorSuperiaReala:
            d->profile = FilmProfile(2.07, 2.36, 2.73); // fix, check
            break;
        case CNFujicolorSuperia100:
            d->profile = FilmProfile(2.37, 2.77, 3.14); // fix, check
            break;
        case CNFujicolorSuperia200:
            d->profile = FilmProfile(2.33, 2.71, 2.97); // fix, check
            break;
        case CNFujicolorSuperiaXtra400:
            d->profile = FilmProfile(2.11, 2.58, 2.96); // fix, check
            break;
        case CNFujicolorSuperiaXtra800:
            d->profile = FilmProfile(2.44, 2.83, 3.18); // fix, check
            break;
        case CNFujicolorTrueDefinition400:
            d->profile = FilmProfile(1.93, 2.21, 2.39); // fix, check
            break;
        case CNFujicolorSuperia1600:
            d->profile = FilmProfile(2.35, 2.68, 2.96); // fix, check
            break;
    }
}

FilmContainer::CNFilmProfile FilmContainer::cnType() const
{
    return d->cnType;
}

int FilmContainer::whitePointForChannel(int ch) const
{
    int max = d->sixteenBit ? 65535 : 255;

    switch (ch) {
    case RedChannel:    return d->whitePoint.red();
    case GreenChannel:  return d->whitePoint.green();
    case BlueChannel:   return d->whitePoint.blue();
    default:            return max;
    }

    /* not reached */
    return max;
}

double FilmContainer::blackPointForChannel(int ch) const
{
    if (ch == LuminosityChannel || ch == AlphaChannel)
        return 0.0;

    return pow(10, -d->profile.dmax(ch)) * (d->sixteenBit ? 65535 : 255);
}

double FilmContainer::gammaForChannel(int ch) const
{
    return d->profile.gamma(ch);
}

LevelsContainer FilmContainer::toLevels() const
{
    LevelsContainer l;
    int max = d->sixteenBit ? 65535 : 255;

    for (int i = LuminosityChannel; i <= AlphaChannel; i++)
    {
        l.lInput[i]  = blackPointForChannel(i) * d->exposure;
        l.hInput[i]  = whitePointForChannel(i) * d->profile.wp(i);
        l.lOutput[i] = 0;
        l.hOutput[i] = max;
        l.gamma[i]   = d->profile.gamma(i);
    }

    return l;
}

BCGContainer FilmContainer::toBCG() const
{
    BCGContainer bcg;

    bcg.brightness = 0.0;
    bcg.contrast   = 1.0;
    bcg.gamma      = d->gamma;

    return bcg;
}

QList<FilmContainer::ListItem*> FilmContainer::profileItemList(QListWidget* view)
{
    QList<FilmContainer::ListItem*> itemList;
    QMap<int, QString>::ConstIterator it;

    for (it = profileMap.constBegin(); it != profileMap.constEnd(); it++)
        itemList << new ListItem(it.value(), view, (CNFilmProfile)it.key());

    return itemList;
}

QMap<int, QString> FilmContainer::profileMapInitializer()
{
    QMap<int, QString> profileMap;

    profileMap[CNNeutral]                       = QString("Neutral");
    profileMap[CNKodakGold100]                  = QString("Kodak Gold 100");
    profileMap[CNKodakGold200]                  = QString("Kodak Gold 200");
    profileMap[CNKodakProfessionalNewPortra160] = QString("Kodak Professional New Portra 160");
    profileMap[CNKodakProfessionalNewPortra400] = QString("Kodak Professional New Portra 400");
    profileMap[CNKodakEktar100]                 = QString("Kodak Ektar 100");
    profileMap[CNKodakFarbwelt100]              = QString("Kodak Farbwelt 100");
    profileMap[CNKodakFarbwelt200]              = QString("Kodak Farbwelt 200");
    profileMap[CNKodakFarbwelt400]              = QString("Kodak Farbwelt 400");
    profileMap[CNKodakProfessionalPortra160NC]  = QString("Kodak Professional Portra 160NC");
    profileMap[CNKodakProfessionalPortra160VC]  = QString("Kodak Professional Portra 160VC");
    profileMap[CNKodakProfessionalPortra400NC]  = QString("Kodak Professional Portra 400NC");
    profileMap[CNKodakProfessionalPortra400VC]  = QString("Kodak Professional Portra 400VC");
    profileMap[CNKodakProfessionalPortra800Box] = QString("Kodak Professional Portra 800 (Box Speed");
    profileMap[CNKodakProfessionalPortra800P1]  = QString("Kodak Professional Portra 800 (Push 1 stop");
    profileMap[CNKodakProfessionalPortra800P2]  = QString("Kodak Professional Portra 800 (Push 2 stop");
    profileMap[CNKodakRoyalGold400]             = QString("Kodak Royal Gold 400");
    profileMap[CNAgfaphotoVistaPlus200]         = QString("Agfaphoto Vista Plus 200");
    profileMap[CNAgfaphotoVistaPlus400]         = QString("Agfaphoto Vista Plus 400");
    profileMap[CNFujicolorPro160S]              = QString("Fujicolor Pro 160S");
    profileMap[CNFujicolorPro160C]              = QString("Fujicolor Pro 160C");
    profileMap[CNFujicolorNPL160]               = QString("Fujicolor NPL 160");
    profileMap[CNFujicolorPro400H]              = QString("Fujicolor Pro 400H");
    profileMap[CNFujicolorPro800Z]              = QString("Fujicolor Pro 800Z");
    profileMap[CNFujicolorSuperiaReala]         = QString("Fujicolor Superia Reala");
    profileMap[CNFujicolorSuperia100]           = QString("Fujicolor Superia 100");
    profileMap[CNFujicolorSuperia200]           = QString("Fujicolor Superia 200");
    profileMap[CNFujicolorSuperiaXtra400]       = QString("Fujicolor Superia X-Tra 400");
    profileMap[CNFujicolorSuperiaXtra800]       = QString("Fujicolor Superia X-Tra 800");
    profileMap[CNFujicolorTrueDefinition400]    = QString("Fujicolor Superia True Definition 400");
    profileMap[CNFujicolorSuperia1600]          = QString("Fujicolor Superia 1600");

    return profileMap;
}

const QMap<int, QString> FilmContainer::profileMap = FilmContainer::profileMapInitializer();

// ------------------------------------------------------------------

class FilmFilter::FilmFilterPriv
{
public:

    FilmFilterPriv()
    {
    }

    FilmContainer film;
};

FilmFilter::FilmFilter(QObject* const parent)
    : DImgThreadedFilter(parent, "FilmFilter"),
      d(new FilmFilterPriv())
{
    d->film = FilmContainer();
    initFilter();
}

FilmFilter::FilmFilter(DImg* const orgImage, QObject* const parent, const FilmContainer& settings)
    : DImgThreadedFilter(orgImage, parent, "FilmFilter"),
      d(new FilmFilterPriv())
{
    d->film = settings;
    initFilter();
}

FilmFilter::~FilmFilter()
{
    cancelFilter();
    delete d;
}

void FilmFilter::filterImage()
{
    DImg tmpLevel;
    DImg tmpBCG;

    LevelsContainer l = d->film.toLevels();
    BCGContainer bcg  = d->film.toBCG();

    bcg.channel = LuminosityChannel;

    // level the image first, this removes the orange mask and corrects
    // colors according to the density ranges of the film profile
    LevelsFilter(l, this, m_orgImage, tmpLevel, 0, 60);

    // in case of a linear raw scan, gamma needs to be
    // applied after leveling the image, otherwise the image will
    // look too bright. The standard value is 2.2, but 1.8 is also
    // frequently found in literature
    tmpBCG = tmpLevel;
    BCGFilter(bcg, this, tmpLevel, tmpBCG, 60, 90);

    // final step is inverting the image to have a positive image
    InvertFilter(this, tmpBCG, m_destImage, 90, 100);
}

FilterAction FilmFilter::filterAction()
{
    FilterAction action(FilterIdentifier(), CurrentVersion());
    action.setDisplayableName(DisplayableName());

    action.addParameter(QString("CNType"),               d->film.cnType());
    action.addParameter(QString("ProfileName"),          FilmContainer::profileMap[d->film.cnType()]);
    action.addParameter(QString("Exposure"),             d->film.exposure());
    action.addParameter(QString("Gamma"),                d->film.gamma());
    action.addParameter(QString("WhitePointRed"),        d->film.whitePoint().red());
    action.addParameter(QString("WhitePointGreen"),      d->film.whitePoint().green());
    action.addParameter(QString("WhitePointBlue"),       d->film.whitePoint().blue());
    action.addParameter(QString("WhitePointAlpha"),      d->film.whitePoint().alpha());
    action.addParameter(QString("WhitePointSixteenBit"), d->film.whitePoint().sixteenBit());

    return action;
}

void FilmFilter::readParameters(const FilterAction& action)
{
    double red   = action.parameter(QString("WhitePointRed")).toDouble();
    double green = action.parameter(QString("WhitePointGreen")).toDouble();
    double blue  = action.parameter(QString("WhitePointBlue")).toDouble();
    double alpha = action.parameter(QString("WhitePointAlpha")).toDouble();
    bool sb      = action.parameter(QString("WhitePointSixteenBit")).toBool();

    d->film.setWhitePoint(DColor(red, green, blue, alpha, sb));
    d->film.setExposure(action.parameter(QString("Exposure")).toDouble());
    d->film.setGamma(action.parameter(QString("Gamma")).toDouble());
    d->film.setCNType((FilmContainer::CNFilmProfile)(action.parameter(QString("CNType")).toInt()));
}

} // namespace Digikam
