/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2005-11-18
 * Description : a class to apply ICC color correction to image.
 *
 * Copyright (C) 2005-2006 by F.J. Cruz <fj.cruz@supercable.es>
 * Copyright (C) 2005-2008 by Gilles Caulier <caulier dot gilles at gmail dot com>
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

#include "icctransform.h"

// Qt includes

#include <QDataStream>
#include <QFile>
#include <QtCore/QVarLengthArray>

// KDE includes

#include <kdebug.h>
#include <kconfig.h>
#include <kapplication.h>
#include <kglobal.h>
#include <kconfiggroup.h>

// Lcms includes

#include <lcms.h>
#if LCMS_VERSION < 114
#define cmsTakeCopyright(profile) "Unknown"
#endif // LCMS_VERSION < 114


namespace Digikam
{

class TransformDescription
{
public:

    TransformDescription()
    {
        inputFormat      = 0;
        outputFormat     = 0;
        intent           = 0;
        transformFlags   = 0;
    }

    bool operator==(const TransformDescription &other)
    {
        return inputProfile   == other.inputProfile &&
               inputFormat    == other.inputFormat &&
               outputProfile  == other.outputProfile &&
               outputFormat   == other.outputFormat &&
               intent         == other.intent &&
               transformFlags == other.transformFlags &&
               proofProfile   == other.proofProfile &&
               proofIntent    == other.proofIntent;
    }

    IccProfile inputProfile;
    int        inputFormat;
    IccProfile outputProfile;
    int        outputFormat;
    int        intent;
    int        transformFlags;
    IccProfile proofProfile;
    int        proofIntent;
};

class IccTransformPriv : public QSharedData
{
public:

    IccTransformPriv()
    {
        intent               = INTENT_PERCEPTUAL;
        useBPC               = false;
        checkGamut           = false;

        handle               = 0;
    }

    ~IccTransformPriv()
    {
        close();
    }

    void close()
    {
        if (handle)
        {
            currentDescription = TransformDescription();
            LcmsLock lock();
            cmsDeleteTransform(handle);
            handle = 0;
        }
    }

    int        intent;
    bool       useBPC;
    bool       checkGamut;

    IccProfile embeddedProfile;
    IccProfile inputProfile;
    IccProfile outputProfile;
    IccProfile proofProfile;
    IccProfile builtinProfile;

    IccProfile &sRGB()
    {
        if (builtinProfile.isNull())
            builtinProfile = IccProfile::sRGB();
        return builtinProfile;
    }

    IccProfile &effectiveInputProfile()
    {
        if (!embeddedProfile.isNull())
            return embeddedProfile;
        else if (!inputProfile.isNull())
            return inputProfile;
        else
            return sRGB();
    }

    IccProfile effectiveInputProfileConst() const
    {
        if (!embeddedProfile.isNull())
            return embeddedProfile;
        else if (!inputProfile.isNull())
            return inputProfile;
        else
            return IccProfile::sRGB();
    }

    cmsHTRANSFORM handle;
    TransformDescription currentDescription;
};

IccTransform::IccTransform()
            : d(new IccTransformPriv)
{
    cmsErrorAction(LCMS_ERROR_SHOW);
}

IccTransform::IccTransform(const IccTransform& other)
            : d(other.d)
{
}

IccTransform &IccTransform::operator=(const IccTransform& other)
{
    d = other.d;
    return *this;
}

IccTransform::~IccTransform()
{
    // close() is done in ~IccTransformPriv
}

void IccTransform::setInputProfile(const IccProfile &profile)
{
    d->inputProfile = profile;
}

void IccTransform::setEmbeddedProfile(const DImg& image)
{
    d->embeddedProfile = image.getIccProfile();
}

void IccTransform::setOutputProfile(const IccProfile &profile)
{
    d->outputProfile = profile;
}

void IccTransform::setProofProfile(const IccProfile &profile)
{
    d->proofProfile = profile;
}

IccProfile IccTransform::embeddedProfile() const
{
    return d->embeddedProfile;
}

IccProfile IccTransform::inputProfile() const
{
    return d->inputProfile;
}

IccProfile IccTransform::outputProfile() const
{
    return d->outputProfile;
}

IccProfile IccTransform::proofProfile() const
{
    return d->proofProfile;
}

IccProfile IccTransform::effectiveInputProfile() const
{
    return d->effectiveInputProfileConst();
}

void IccTransform::setIntent(RenderingIntent intent)
{
    switch (intent)
    {
        case Perceptual:
            d->intent = INTENT_PERCEPTUAL;
            break;
        case RelativeColorimetric:
            d->intent = INTENT_RELATIVE_COLORIMETRIC;
            break;
        case Saturation:
            d->intent = INTENT_SATURATION;
            break;
        case AbsoluteColorimetric:
            d->intent = INTENT_ABSOLUTE_COLORIMETRIC;
            break;
    }
}

void IccTransform::setUseBlackPointCompensation(bool useBPC)
{
    d->useBPC = useBPC;
}

void IccTransform::setCheckGamut(bool checkGamut)
{
    d->checkGamut = checkGamut;
}

void IccTransform::readFromConfig()
{
    KSharedConfig::Ptr config = KGlobal::config();
    KConfigGroup group = config->group(QString("Color Management"));

    int intent = group.readEntry("RenderingIntent", 0);
    bool useBPC = group.readEntry("BPCAlgorithm", false);

    setIntent(intent);
    setUseBlackPointCompensation(useBPC);
}

bool IccTransform::willHaveEffect()
{
    return !d->effectiveInputProfile().isSameProfileAs(d->outputProfile);
}

TransformDescription IccTransform::getDescription(const DImg& image)
{
    TransformDescription description;

    description.inputProfile = d->effectiveInputProfile();
    description.outputProfile = d->outputProfile;
    description.intent = d->intent;

    if (d->useBPC)
    {
        description.transformFlags |= cmsFLAGS_WHITEBLACKCOMPENSATION;
    }

    LcmsLock lock();
    // Do not use TYPE_BGR_ - this implies 3 bytes per pixel, but even if !image.hasAlpha(),
    // our image data has 4 bytes per pixel with the fourth byte filled with 0xFF.
    if (image.sixteenBit())
    {
        switch (cmsGetColorSpace(description.inputProfile))
        {
            case icSigGrayData:
                description.inputFormat = TYPE_GRAYA_16;
                break;
            case icSigCmykData:
                description.inputFormat = TYPE_CMYK_16;
                break;
            default:
                description.inputFormat = TYPE_BGRA_16;
        }

        description.outputFormat = TYPE_BGRA_16;
    }
    else
    {
        switch (cmsGetColorSpace(description.inputProfile))
        {
            case icSigGrayData:
                description.inputFormat = TYPE_GRAYA_8;
                break;
            case icSigCmykData:
                description.inputFormat = TYPE_CMYK_8;
                break;
            default:
                description.inputFormat = TYPE_BGRA_8;
        }

        description.outputFormat = TYPE_BGRA_8;
    }
    return description;
}

TransformDescription IccTransform::getProofingDescription(const DImg& image)
{
    TransformDescription description = getDescription(image);

    description.proofProfile = d->proofProfile;
    description.proofIntent  = d->intent;

    description.transformFlags |= cmsFLAGS_SOFTPROOFING;
    if (d->checkGamut)
    {
        cmsSetAlarmCodes(126, 255, 255);
        description.transformFlags |= cmsFLAGS_GAMUTCHECK;
    }

    return description;
}

bool IccTransform::open(TransformDescription &description)
{
    if (d->handle)
    {
        if (d->currentDescription == description)
        {
            return true;
        }
        else
        {
            close();
        }
    }

    d->currentDescription = description;

    LcmsLock lock();
    d->handle = cmsCreateTransform( description.inputProfile,
                                    description.inputFormat,
                                    description.outputProfile,
                                    description.outputFormat,
                                    description.intent,
                                    description.transformFlags);

    if (!d->handle)
    {
        kDebug(50003) << "LCMS internal error: cannot create a color transform instance";
        return false;
    }

    return true;
}

bool IccTransform::openProofing(TransformDescription &description)
{
    if (d->handle)
    {
        if (d->currentDescription == description)
        {
            return true;
        }
        else
        {
            close();
        }
    }

    d->currentDescription = description;

    LcmsLock lock();
    d->handle = cmsCreateProofingTransform( description.inputProfile,
                                    description.inputFormat,
                                    description.outputProfile,
                                    description.outputFormat,
                                    description.proofProfile,
                                    description.intent,
                                    description.proofIntent,
                                    description.transformFlags);

    if (!d->handle)
    {
        kDebug(50003) << "LCMS internal error: cannot create a color transform instance";
        return false;
    }

    return true;
}

bool IccTransform::apply(DImg& image)
{
    if (!d->effectiveInputProfile().open())
    {
        kError(50003) << "Cannot open embedded profile";
        return false;
    }

    if (!d->outputProfile.open())
    {
        kError(50003) << "Cannot open output profile";
        return false;
    }

    if (!d->proofProfile.isNull())
    {
        if (!d->proofProfile.open())
        {
            kError(50003) << "Cannot open proofing profile";
            return false;
        }
    }

    TransformDescription description;
    if (d->proofProfile.isNull())
    {
        description = getDescription(image);
        if (!open(description))
            return false;
    }
    else
    {
        description = getProofingDescription(image);
        if (!openProofing(description))
            return false;
    }

    transform(image, description);

    return true;
}

    //kDebug(50003) << "Transform flags are: " << transformFlags;

void IccTransform::transform(const DImg& image, const TransformDescription& description)
{
    const int bytesDepth = image.bytesDepth();
    const int pixels = image.width() * image.height();
    // convert ten scanlines in a batch
    const int pixelsPerStep = image.width() * 10;
    uchar *data = image.bits();

    // it is safe to use the same input and output buffer if the format is the same
    if (description.inputFormat == description.outputFormat)
    {
        for (int p=pixels; p > 0; p -= pixelsPerStep)
        {
            int pixelsThisStep = qMin(p, pixelsPerStep);
            int size = pixelsThisStep * bytesDepth;
            LcmsLock lock();
            cmsDoTransform(d->handle, data, data, pixelsThisStep);
            data += size;
        }
    }
    else
    {
        QVarLengthArray<uchar> buffer(pixelsPerStep * bytesDepth);
        for (int p=pixels; p > 0; p -= pixelsPerStep)
        {
            int pixelsThisStep = qMin(p, pixelsPerStep);
            int size = pixelsThisStep * bytesDepth;
            LcmsLock lock();
            memcpy(buffer.data(), data, size);
            cmsDoTransform(d->handle, buffer.data(), data, pixelsThisStep);
            data += size;
        }
    }
}

void IccTransform::close()
{
    d->close();
}

/*void IccTransform::closeProfiles()
{
    d->inputProfile.close();
    d->outputProfile.close();
    d->proofProfile.close();
    d->embeddedProfile.close();
}*/

}  // namespace Digikam
