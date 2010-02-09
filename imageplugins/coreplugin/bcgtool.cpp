/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-06-05
 * Description : digiKam image editor to adjust Brightness,
                 Contrast, and Gamma of picture.
 *
 * Copyright (C) 2004 by Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Copyright (C) 2005-2010 by Gilles Caulier <caulier dot gilles at gmail dot com>
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

#include "bcgtool.moc"

// Qt includes

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>

// KDE includes

#include <kapplication.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kcursor.h>
#include <kglobal.h>
#include <kicon.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kvbox.h>

// LibKDcraw includes

#include <libkdcraw/rnuminput.h>

// Local includes

#include "bcgmodifier.h"
#include "colorgradientwidget.h"
#include "editortoolsettings.h"
#include "histogrambox.h"
#include "histogramwidget.h"
#include "imageiface.h"
#include "imageregionwidget.h"

using namespace KDcrawIface;
using namespace Digikam;

namespace DigikamImagesPluginCore
{

class BCGToolPriv
{
public:

    BCGToolPriv() :
        configGroupName("bcgadjust Tool"),
        configHistogramChannelEntry("Histogram Channel"),
        configHistogramScaleEntry("Histogram Scale"),
        configBrightnessAdjustmentEntry("BrightnessAdjustment"),
        configContrastAdjustmentEntry("ContrastAdjustment"),
        configGammaAdjustmentEntry("GammaAdjustment"),

        destinationPreviewData(0),
        bInput(0),
        cInput(0),
        gInput(0),
        previewWidget(0),
        gboxSettings(0)
        {}

    const QString       configGroupName;
    const QString       configHistogramChannelEntry;
    const QString       configHistogramScaleEntry;
    const QString       configBrightnessAdjustmentEntry;
    const QString       configContrastAdjustmentEntry;
    const QString       configGammaAdjustmentEntry;

    uchar*              destinationPreviewData;

    RIntNumInput*       bInput;
    RIntNumInput*       cInput;

    RDoubleNumInput*    gInput;

    ImageRegionWidget*  previewWidget;
    EditorToolSettings* gboxSettings;
};

BCGTool::BCGTool(QObject* parent)
       : EditorToolThreaded(parent),
         d(new BCGToolPriv)
{
    setObjectName("bcgadjust");
    setToolName(i18n("Brightness / Contrast / Gamma"));
    setToolIcon(SmallIcon("contrast"));
    setToolHelp("bcgadjusttool.anchor");

    d->destinationPreviewData = 0;

    d->previewWidget = new ImageRegionWidget;
    d->previewWidget->setWhatsThis(i18n("The image brightness-contrast-gamma adjustment preview "
                                        "is shown here. "
                                        "Picking a color on the image will show the "
                                        "corresponding color level on the histogram."));
    setToolView(d->previewWidget);
    setPreviewModeMask(PreviewToolBar::AllPreviewModes);

    // -------------------------------------------------------------

    d->gboxSettings = new EditorToolSettings;
    d->gboxSettings->setTools(EditorToolSettings::Histogram);
    d->gboxSettings->setButtons(EditorToolSettings::Default|
                                EditorToolSettings::Ok|
                                EditorToolSettings::Cancel|
                                EditorToolSettings::Try);

    // -------------------------------------------------------------

    QLabel *label2 = new QLabel(i18n("Brightness:"));
    d->bInput      = new RIntNumInput();
    d->bInput->setRange(-100, 100, 1);
    d->bInput->setSliderEnabled(true);
    d->bInput->setDefaultValue(0);
    d->bInput->setWhatsThis( i18n("Set here the brightness adjustment of the image."));

    QLabel *label3 = new QLabel(i18n("Contrast:"));
    d->cInput      = new RIntNumInput();
    d->cInput->setRange(-100, 100, 1);
    d->cInput->setSliderEnabled(true);
    d->cInput->setDefaultValue(0);
    d->cInput->setWhatsThis( i18n("Set here the contrast adjustment of the image."));

    QLabel *label4 = new QLabel(i18n("Gamma:"));
    d->gInput      = new RDoubleNumInput();
    d->gInput->setDecimals(2);
    d->gInput->input()->setRange(0.1, 3.0, 0.01, true);
    d->gInput->setDefaultValue(1.0);
    d->gInput->setWhatsThis( i18n("Set here the gamma adjustment of the image."));

    // -------------------------------------------------------------

    QGridLayout* mainLayout = new QGridLayout();
    mainLayout->addWidget(label2,    0, 0, 1, 5);
    mainLayout->addWidget(d->bInput, 1, 0, 1, 5);
    mainLayout->addWidget(label3,    2, 0, 1, 5);
    mainLayout->addWidget(d->cInput, 3, 0, 1, 5);
    mainLayout->addWidget(label4,    4, 0, 1, 5);
    mainLayout->addWidget(d->gInput, 5, 0, 1, 5);
    mainLayout->setRowStretch(6, 10);
    mainLayout->setMargin(d->gboxSettings->spacingHint());
    mainLayout->setSpacing(d->gboxSettings->spacingHint());
    d->gboxSettings->plainPage()->setLayout(mainLayout);

    // -------------------------------------------------------------

    setToolSettings(d->gboxSettings);
    init();

    // -------------------------------------------------------------

    connect(d->bInput, SIGNAL(valueChanged(int)),
            this, SLOT(slotTimer()));

    connect(d->cInput, SIGNAL(valueChanged(int)),
            this, SLOT(slotTimer()));

    connect(d->gInput, SIGNAL(valueChanged(double)),
            this, SLOT(slotTimer()));

    connect(d->previewWidget, SIGNAL(signalResized()),
            this, SLOT(slotEffect()));

    // -------------------------------------------------------------

    d->gboxSettings->enableButton(EditorToolSettings::Ok, false);
}

BCGTool::~BCGTool()
{
    if (d->destinationPreviewData)
       delete [] d->destinationPreviewData;

    delete d;
}

void BCGTool::readSettings()
{
    KSharedConfig::Ptr config = KGlobal::config();
    KConfigGroup group        = config->group(d->configGroupName);

    d->gboxSettings->histogramBox()->setChannel((ChannelType)group.readEntry(d->configHistogramChannelEntry,
                                                (int)LuminosityChannel));
    d->gboxSettings->histogramBox()->setScale((HistogramScale)group.readEntry(d->configHistogramScaleEntry,
                                              (int)LogScaleHistogram));

    d->bInput->setValue(group.readEntry(d->configBrightnessAdjustmentEntry, d->bInput->defaultValue()));
    d->cInput->setValue(group.readEntry(d->configContrastAdjustmentEntry,   d->cInput->defaultValue()));
    d->gInput->setValue(group.readEntry(d->configGammaAdjustmentEntry,      d->gInput->defaultValue()));
}

void BCGTool::writeSettings()
{
    KSharedConfig::Ptr config = KGlobal::config();
    KConfigGroup group        = config->group(d->configGroupName);

    group.writeEntry(d->configHistogramChannelEntry,     (int)d->gboxSettings->histogramBox()->channel());
    group.writeEntry(d->configHistogramScaleEntry,       (int)d->gboxSettings->histogramBox()->scale());
    group.writeEntry(d->configBrightnessAdjustmentEntry, d->bInput->value());
    group.writeEntry(d->configContrastAdjustmentEntry,   d->cInput->value());
    group.writeEntry(d->configGammaAdjustmentEntry,      d->gInput->value());
    config->sync();
}

void BCGTool::slotResetSettings()
{
    d->bInput->blockSignals(true);
    d->cInput->blockSignals(true);
    d->gInput->blockSignals(true);

    d->bInput->slotReset();
    d->cInput->slotReset();
    d->gInput->slotReset();

    d->bInput->blockSignals(false);
    d->cInput->blockSignals(false);
    d->gInput->blockSignals(false);
    slotEffect();
}

void BCGTool::prepareEffect()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d->bInput->setEnabled(false);
    d->cInput->setEnabled(false);
    d->gInput->setEnabled(false);
    toolView()->setEnabled(false);

    double b = (double)d->bInput->value()/250.0;
    double c = (double)(d->cInput->value()/100.0) + 1.00;
    double g = d->gInput->value();

    d->gboxSettings->enableButton(EditorToolSettings::Ok, ( b != 0.0 || c != 1.0 || g != 1.0 ));
    d->gboxSettings->histogramBox()->histogram()->stopHistogramComputation();

    DImg preview = d->previewWidget->getOriginalRegionImage();
    setFilter(dynamic_cast<DImgThreadedFilter*>(new BCGFilter(&preview, this, b, c, g)));
}

void BCGTool::putPreviewData()
{
    DImg preview = filter()->getTargetImage();
    d->previewWidget->setPreviewImage(preview);

    // Update histogram.

    if (d->destinationPreviewData)
       delete [] d->destinationPreviewData;

    d->destinationPreviewData = preview.copyBits();
    d->gboxSettings->histogramBox()->histogram()->updateData(d->destinationPreviewData,
                                                             preview.width(), preview.height(), preview.sixteenBit(),
                                                             0, 0, 0, false);
}

void BCGTool::prepareFinal()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d->bInput->setEnabled(false);
    d->cInput->setEnabled(false);
    d->gInput->setEnabled(false);
    toolView()->setEnabled(false);

    double b = (double)d->bInput->value()/250.0;
    double c = (double)(d->cInput->value()/100.0) + 1.00;
    double g = d->gInput->value();

    ImageIface iface(0, 0);
    setFilter(dynamic_cast<DImgThreadedFilter*>(new BCGFilter(iface.getOriginalImg(), this, b, c, g)));
}

void BCGTool::putFinalData()
{
    ImageIface iface(0, 0);
    iface.putOriginalImage(i18n("Brightness / Contrast / Gamma"), filter()->getTargetImage().bits());
}

void BCGTool::renderingFinished()
{
    QApplication::restoreOverrideCursor();
    d->bInput->setEnabled(true);
    d->cInput->setEnabled(true);
    d->gInput->setEnabled(true);
    toolView()->setEnabled(true);
}

}  // namespace DigikamImagesPluginCore
