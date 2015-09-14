/*
 * Copyright (c) 2005-2009 Thomas Zander <zander@kde.org>
 * Copyright (c) 2009 Peter Simonsson <peter.simonsson@gmail.com>
 * Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "KoModeBoxDocker_p.h"
#include "KoModeBox_p.h"
#include <KoCanvasController.h>

#include <WidgetsDebug.h>


KoModeBoxDocker::KoModeBoxDocker(KoModeBox *modeBox)
    : m_modeBox(modeBox)
{
    setWidget(modeBox);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setWindowTitle("");
    setObjectName("ModeBox");

    connect(this, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(locationChanged(Qt::DockWidgetArea)));
}

void KoModeBoxDocker::setCanvas(KoCanvasBase *canvas)
{
    setEnabled(canvas != 0);
    m_modeBox->setCanvas(canvas);
}

void KoModeBoxDocker::unsetCanvas()
{
    setEnabled(false);
    m_modeBox->unsetCanvas();
}

void KoModeBoxDocker::locationChanged(Qt::DockWidgetArea area)
{
    m_modeBox->locationChanged(area);
}
