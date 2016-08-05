/*
 *  Copyright (c) 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_dlg_brush_hud_config.h"
#include "ui_kis_dlg_brush_hud_config.h"

#include "kis_paintop_preset.h"
#include "kis_paintop_settings.h"

#include "kis_brush_hud_properties_config.h"


struct KisDlgConfigureBrushHud::Private
{
    KisPaintOpPresetSP preset;
    QList<KisUniformPaintOpPropertySP> properties;
};

KisDlgConfigureBrushHud::KisDlgConfigureBrushHud(KisPaintOpPresetSP preset, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KisDlgConfigureBrushHud),
    m_d(new Private)
{
    ui->setupUi(this);

    m_d->preset = preset;
    m_d->properties = preset->settings()->uniformProperties();

    QList<KisUniformPaintOpPropertySP> available;
    QList<KisUniformPaintOpPropertySP> chosen;

    KisBrushHudPropertiesConfig cfg;
    cfg.filterProperties(preset->paintOp().id(),
                         m_d->properties, &chosen, &available);

    ui->lstAvailable->addProperties(available);
    ui->lstCurrent->addProperties(chosen);

    connect(this, SIGNAL(accepted()), SLOT(slotConfigAccepted()));

    connect(ui->btnAdd, SIGNAL(clicked()), SLOT(slotMoveRight()));
    connect(ui->btnRemove, SIGNAL(clicked()), SLOT(slotMoveLeft()));

    connect(ui->btnDown, SIGNAL(clicked()), SLOT(slotMoveDown()));
    connect(ui->btnUp, SIGNAL(clicked()), SLOT(slotMoveUp()));
}

KisDlgConfigureBrushHud::~KisDlgConfigureBrushHud()
{
    delete ui;
}

void KisDlgConfigureBrushHud::slotConfigAccepted()
{
    KisBrushHudPropertiesConfig cfg;
    cfg.setSelectedProperties(m_d->preset->paintOp().id(), ui->lstCurrent->selectedPropertiesIds());
}

void KisDlgConfigureBrushHud::slotMoveRight()
{
    QListWidgetItem *item = ui->lstAvailable->currentItem();
    if (!item) return;

    const int prevPosition = ui->lstAvailable->row(item) - 1;
    const int newPosition = ui->lstCurrent->currentRow() + 1;

    ui->lstAvailable->takeItem(ui->lstAvailable->row(item));
    ui->lstAvailable->setCurrentRow(qMax(0, prevPosition));
    ui->lstCurrent->insertItem(newPosition, item);
    ui->lstCurrent->setCurrentItem(item);
}

void KisDlgConfigureBrushHud::slotMoveLeft()
{
    QListWidgetItem *item = ui->lstCurrent->currentItem();
    if (!item) return;

    const int prevPosition = ui->lstCurrent->row(item) - 1;
    const int newPosition = ui->lstAvailable->currentRow() + 1;

    ui->lstCurrent->takeItem(ui->lstCurrent->row(item));
    ui->lstCurrent->setCurrentRow(qMax(0, prevPosition));
    ui->lstAvailable->insertItem(newPosition, item);
    ui->lstAvailable->setCurrentItem(item);
}

void KisDlgConfigureBrushHud::slotMoveUp()
{
    QListWidgetItem *item = ui->lstCurrent->currentItem();
    if (!item) return;

    int position = ui->lstCurrent->row(item);

    if (position <= 0) return;

    ui->lstCurrent->takeItem(ui->lstCurrent->row(item));
    ui->lstCurrent->insertItem(position - 1, item);
    ui->lstCurrent->setCurrentItem(item);
}

void KisDlgConfigureBrushHud::slotMoveDown()
{
    QListWidgetItem *item = ui->lstCurrent->currentItem();
    if (!item) return;

    int position = ui->lstCurrent->row(item);

    if (position >= ui->lstCurrent->count() - 1) return;

    ui->lstCurrent->takeItem(ui->lstCurrent->row(item));
    ui->lstCurrent->insertItem(position + 1, item);
    ui->lstCurrent->setCurrentItem(item);
}

