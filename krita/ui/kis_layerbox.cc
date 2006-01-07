/*
 *  kis_layerbox.cc - part of Krita aka Krayon aka KimageShop
 *
 *  Copyright (c) 2002 Patrick Julien <freak@codepimps.org>
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

#include <qbutton.h>
#include <qtoolbutton.h>
#include <qbrush.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qhbox.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qrect.h>
#include <qstring.h>
#include <qstyle.h>
#include <qtooltip.h>
#include <qwidget.h>
#include <qcombobox.h>
#include <qcheckbox.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kpopupmenu.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <kiconloader.h>
#include <kicontheme.h>
#include <klocale.h>
#include <knuminput.h>

#include "kis_layerlist.h"
#include "kis_cmb_composite.h"
#include "wdglayerbox.h"
#include "kis_colorspace.h"
#include "kis_paint_device_impl.h"
#include "kis_layer.h"
#include "kis_group_layer.h"
#include "kis_image.h"

#include "kis_populate_visitor.h"

#include "kis_layerbox.h"

KisLayerBox::KisLayerBox(QWidget *parent, const char *name)
    : super(parent, name), m_image(0)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox -> setAutoAdd(true);

    m_lst = new WdgLayerBox(this);
    setMinimumSize(m_lst -> minimumSizeHint());

    QToolTip::add(m_lst -> bnAdd, i18n("Create new layer"));

    QToolTip::add(m_lst -> bnDelete, i18n("Remove current layer"));

    QToolTip::add(m_lst -> bnRaise, i18n("Upper current layer"));
    m_lst -> bnRaise -> setEnabled(false);

    m_lst -> bnLower -> setEnabled(false);
    QToolTip::add(m_lst -> bnLower, i18n("Lower current layer"));

    QToolTip::add(m_lst -> bnProperties, i18n("Properties for layer"));

    KIconLoader il( "krita" );

    list() -> setFoldersCanBeActive(true);

    list() -> addProperty("visible", i18n("Visible"), SmallIconSet("button_ok", KIcon::SizeSmallMedium), true);

    list() -> addProperty("locked", i18n("Locked"), SmallIconSet("lock", KIcon::SizeSmallMedium));


    connect(list() -> contextMenu(), SIGNAL(aboutToShow()), SLOT(slotAboutToShow()));
    connect(list(), SIGNAL(activated(LayerItem*)),
                    SLOT(slotLayerActivated(LayerItem*)));
    connect(list(), SIGNAL(displayNameChanged(LayerItem*, const QString&)),
                    SLOT(slotLayerDisplayNameChanged(LayerItem*, const QString&)));
    connect(list(), SIGNAL(propertyChanged(LayerItem*, const QString&, bool)),
                    SLOT(slotLayerPropertyChanged(LayerItem*, const QString&, bool)));
    connect(list(), SIGNAL(layerMoved(LayerItem*, LayerItem*, LayerItem*)),
                    SLOT(slotLayerMoved(LayerItem*, LayerItem*, LayerItem*)));
    connect(list(), SIGNAL(requestNewLayer(LayerItem*, LayerItem*)),
                    SLOT(slotRequestNewLayer(LayerItem*, LayerItem*)));
    connect(list(), SIGNAL(requestNewFolder(LayerItem*, LayerItem*)),
                    SLOT(slotRequestNewFolder(LayerItem*, LayerItem*)));
    connect(list(), SIGNAL(requestNewObjectLayer(LayerItem*, LayerItem*, const KoDocumentEntry&)),
                    SLOT(slotRequestNewObjectLayer(LayerItem*, LayerItem*, const KoDocumentEntry&)));
    connect(list(), SIGNAL(requestRemoveLayer(LayerItem*)),
                    SLOT(slotRequestRemoveLayer(LayerItem*)));
    connect(list(), SIGNAL(requestLayerProperties(LayerItem*)),
                    SLOT(slotRequestLayerProperties(LayerItem*)));

    connect(m_lst -> bnAdd, SIGNAL(clicked()), SLOT(slotAddClicked()));
    connect(m_lst -> bnDelete, SIGNAL(clicked()), SLOT(slotRmClicked()));
    connect(m_lst -> bnRaise, SIGNAL(clicked()), SLOT(slotRaiseClicked()));
    connect(m_lst -> bnLower, SIGNAL(clicked()), SLOT(slotLowerClicked()));
    connect(m_lst -> bnProperties, SIGNAL(clicked()), SLOT(slotPropertiesClicked()));
    connect(m_lst -> intOpacity, SIGNAL(valueChanged(int)), SIGNAL(sigOpacityChanged(int)));
    connect(m_lst -> cmbComposite, SIGNAL(activated(const KisCompositeOp&)), SIGNAL(sigItemComposite(const KisCompositeOp&)));
    connect(m_lst -> chkActLayerVis, SIGNAL(stateChanged(int)), SIGNAL(sigActLayerVisChanged(int)));
}

KisLayerBox::~KisLayerBox()
{
}

KisLayerList* KisLayerBox::list() const
{
    return m_lst -> listLayers;
}

void KisLayerBox::setImage(KisImageSP img)
{
    if (m_image)
        m_image -> disconnect(this);

    m_image = img;

    if (img)
    {
        connect(img, SIGNAL(sigLayerActivated(KisLayerSP)), this, SLOT(slotLayerActivated(KisLayerSP)));
        connect(img, SIGNAL(sigLayerAdded(KisLayerSP)), this, SLOT(slotLayerAdded(KisLayerSP)));
        connect(img, SIGNAL(sigLayerRemoved(KisLayerSP, KisGroupLayerSP, KisLayerSP)),
                this, SLOT(slotLayerRemoved(KisLayerSP, KisGroupLayerSP, KisLayerSP)));
        connect(img, SIGNAL(sigLayerPropertiesChanged(KisLayerSP)),
                this, SLOT(slotLayerPropertiesChanged(KisLayerSP)));
        connect(img, SIGNAL(sigLayerMoved(KisLayerSP, KisGroupLayerSP, KisLayerSP)),
                this, SLOT(slotLayerMoved(KisLayerSP, KisGroupLayerSP, KisLayerSP)));
        connect(img, SIGNAL(sigLayersChanged(KisGroupLayerSP)), this, SLOT(slotLayersChanged(KisGroupLayerSP)));
        slotLayersChanged(img -> rootLayer());
    }
    else
        clear();
}

void KisLayerBox::slotLayerActivated(KisLayerSP layer)
{
    if (layer)
        list() -> setActiveLayer(layer -> id());
    else
        list() -> setActiveLayer(-1);
    updateUI();
}

void KisLayerBox::slotLayerAdded(KisLayerSP layer)
{
    if (layer.data() == m_image -> rootLayer().data() || list() -> layer(layer -> id()))
        return;
    if (layer -> parent() == m_image -> rootLayer())
    {
        KisPopulateVisitor visitor(list());
        layer -> accept(visitor);
    }
    else
    {
        KisPopulateVisitor visitor(list() -> layer(layer -> parent() -> id()));
        layer -> accept(visitor);
    }

    updateUI();
}

void KisLayerBox::slotLayerRemoved(KisLayerSP layer, KisGroupLayerSP, KisLayerSP)
{
    list() -> removeLayer(layer -> id());
    updateUI();
}

void KisLayerBox::slotLayerMoved(KisLayerSP layer, KisGroupLayerSP, KisLayerSP)
{
    int parentID = layer -> parent() -> id();
    if (layer -> parent() == m_image -> rootLayer())
        parentID = -1;

    int siblingID = -1;
    if (layer -> prevSibling())
        siblingID = layer -> prevSibling() -> id();

    list() -> moveLayer(layer -> id(), parentID, siblingID);
    updateUI();
}

void KisLayerBox::slotLayerPropertiesChanged(KisLayerSP layer)
{
    list() -> setLayerDisplayName(layer -> id(), layer -> name());
    updateUI();
}

void KisLayerBox::slotLayersChanged(KisGroupLayerSP rootLayer)
{
    list() -> clear();
    KisPopulateVisitor visitor(list());
    for (KisLayerSP layer = rootLayer -> firstChild(); layer; layer = layer -> nextSibling())
        layer -> accept(visitor);
    updateUI();
}

void KisLayerBox::slotLayerActivated(LayerItem* item)
{
    if (item)
        m_image -> activate(m_image -> findLayer(item -> id()));
    else
        m_image -> activate(0);
    updateUI();
}

void KisLayerBox::slotLayerDisplayNameChanged(LayerItem* item, const QString& displayName)
{
    if(KisLayerSP layer = m_image -> findLayer(item -> id()))
        layer -> setName(displayName);
    updateUI();
}

void KisLayerBox::slotLayerPropertyChanged(LayerItem* item, const QString& name, bool on)
{
    if (KisLayerSP layer = m_image -> findLayer(item -> id()))
    {
        if (name == "visible")
            layer -> setVisible(on);
        else if (name == "locked")
            layer -> setLocked(on);
    }
    updateUI();
}

void KisLayerBox::slotLayerMoved(LayerItem* item, LayerItem* p, LayerItem*)
{
    KisLayerSP layer = m_image -> findLayer(item -> id());
    KisLayer* l = m_image -> rootLayer().data();
    if (p)
        l = m_image -> findLayer(p -> id()).data();
    KisGroupLayerSP parent = dynamic_cast<KisGroupLayer*>(l);
    KisLayerSP above = 0;
    if (item -> nextSibling())
        above = m_image -> findLayer(item -> nextSibling() -> id());
    if (layer)
        m_image -> moveLayer(layer, parent.data(), above);
    updateUI();
}

void KisLayerBox::slotRequestNewLayer(LayerItem* p, LayerItem* after)
{
    KisLayer* l = m_image -> rootLayer().data();
    if (p)
        l = m_image -> findLayer(p -> id()).data();
    KisGroupLayerSP parent = dynamic_cast<KisGroupLayer*>(l);

    KisLayerSP above = 0;
    if (after && after -> nextSibling())
        above = m_image -> findLayer(after -> nextSibling() -> id());
    else if (after)
        above = 0;
    else if (p && p -> firstChild())
        above = parent -> firstChild();
    else if (!p && m_image -> rootLayer() -> childCount())
        above = m_image -> rootLayer() -> firstChild();
    emit sigRequestLayer(parent, above);
}

void KisLayerBox::slotRequestNewFolder(LayerItem* p, LayerItem* after)
{
    KisLayer* l = m_image -> rootLayer().data();
    if (p)
        l = m_image -> findLayer(p -> id()).data();
    KisGroupLayerSP parent = dynamic_cast<KisGroupLayer*>(l);

    KisLayerSP above = 0;
    if (after && after -> nextSibling())
        above = m_image -> findLayer(after -> nextSibling() -> id());
    else if (after)
        above = 0;
    else if (p && p -> firstChild())
        above = parent -> firstChild();
    else if (!p && m_image -> rootLayer() -> childCount())
        above = m_image -> rootLayer() -> firstChild();
    emit sigRequestGroupLayer(parent, above);
}

void KisLayerBox::slotRequestNewObjectLayer(LayerItem* p, LayerItem* after, const KoDocumentEntry& entry)
{
    KisLayer* l = m_image -> rootLayer().data();
    if (p)
        l = m_image -> findLayer(p -> id()).data();
    KisGroupLayerSP parent = dynamic_cast<KisGroupLayer*>(l);

    KisLayerSP above = 0;
    if (after && after -> nextSibling())
        above = m_image -> findLayer(after -> nextSibling() -> id());
    else if (after)
        above = 0;
    else if (p && p -> firstChild())
        above = parent -> firstChild();
    else if (!p && m_image -> rootLayer() -> childCount())
        above = m_image -> rootLayer() -> firstChild();
    emit sigRequestPartLayer(parent, above, entry);
}

void KisLayerBox::slotRequestRemoveLayer(LayerItem* item)
{
    if (KisLayerSP layer = m_image -> findLayer(item -> id()))
        m_image -> removeLayer(layer);
    updateUI();
}

void KisLayerBox::slotRequestLayerProperties(LayerItem* item)
{
    if (KisLayerSP layer = m_image -> findLayer(item -> id()))
        emit sigRequestLayerProperties(layer);
}

void KisLayerBox::updateUI()
{
    m_lst -> bnDelete -> setEnabled(list() -> activeLayer());
    m_lst -> bnRaise -> setEnabled(list() -> activeLayer() && list() -> activeLayer() -> prevSibling());
    m_lst -> bnLower -> setEnabled(list() -> activeLayer() && list() -> activeLayer() -> nextSibling());
    m_lst -> intOpacity -> setEnabled(list() -> activeLayer());
    m_lst -> cmbComposite -> setEnabled(list() -> activeLayer());
    if (m_image)
        if (KisLayerSP active = m_image -> activeLayer())
        {
            if (m_image -> activeDevice())
                slotSetColorSpace(m_image -> activeDevice() -> colorSpace());
            else
                slotSetColorSpace(m_image -> colorSpace());

            m_lst -> intOpacity -> setValue(int(float(active -> opacity() * 100) / 255 + 0.5));
            m_lst -> cmbComposite -> setCurrentItem(active -> compositeOp());
        }
}

void KisLayerBox::slotAboutToShow()
{
}

void KisLayerBox::slotSetCompositeOp(const KisCompositeOp& compositeOp)
{
    m_lst -> cmbComposite -> blockSignals(true);
    m_lst -> cmbComposite -> setCurrentItem(compositeOp);
    m_lst -> cmbComposite -> blockSignals(false);
}

void KisLayerBox::slotSetColorSpace(const KisColorSpace * colorSpace)
{
    m_lst -> cmbComposite -> blockSignals(true);
    m_lst -> cmbComposite -> setCompositeOpList(colorSpace -> userVisiblecompositeOps());
    m_lst -> cmbComposite -> blockSignals(false);
}

// range: 0-100
void KisLayerBox::slotSetOpacity(int opacity)
{
    m_lst -> intOpacity -> blockSignals(true);
    m_lst -> intOpacity -> setValue(opacity);
    m_lst -> intOpacity -> blockSignals(false);
}

void KisLayerBox::clear()
{
    list() -> clear();
    updateUI();
}

void KisLayerBox::slotAddClicked()
{
    KisGroupLayerSP root = dynamic_cast<KisGroupLayer*>(m_image -> rootLayer().data());
    if (KisLayerSP active = m_image -> activeLayer())
    {
        KisGroupLayerSP p = root;
        if (active -> parent())
            p = active -> parent();
        emit sigRequestLayer(p, active);
    }
    else
        emit sigRequestLayer(root, m_image -> rootLayer() -> firstChild());
}

void KisLayerBox::slotRmClicked()
{
    QValueList<int> l = list() -> selectedLayerIDs();
    if (l.count() < 2 && list() -> activeLayer() && !l.contains(list() -> activeLayer() -> id()))
    {
        l.clear();
        l.append(list() -> activeLayer() -> id());
    }

    for (int i = 0, n = l.count(); i < n; ++i)
        m_image -> removeLayer(m_image -> findLayer(l[i]));
}

void KisLayerBox::slotRaiseClicked()
{
    QValueList<int> l = list() -> selectedLayerIDs();
    if (l.count() < 2 && list() -> activeLayer() && !l.contains(list() -> activeLayer() -> id()))
    {
        l.clear();
        l.append(list() -> activeLayer() -> id());
    }

    for (int i = 0, n = l.count(); i < n; ++i)
        if (KisLayerSP layer = m_image -> findLayer(l[i]))
            if (layer -> prevSibling())
                m_image -> moveLayer(layer, layer -> parent().data(), layer -> prevSibling());
}

void KisLayerBox::slotLowerClicked()
{
    QValueList<LayerItem*> l = list() -> selectedLayers();
    if (l.count() < 2 && list() -> activeLayer() && !l.contains(list() -> activeLayer()))
    {
        l.clear();
        l.append(list() -> activeLayer());
    }

    for (int i = 0, n = l.count(); i < n; ++i)
        if (LayerItem *layer = l[i])
            if (layer -> nextSibling())
                list() -> moveLayer(layer, layer -> parent(), layer -> nextSibling());
}

void KisLayerBox::slotPropertiesClicked()
{
    if (KisLayerSP active = m_image -> activeLayer())
        emit sigRequestLayerProperties(active);
}

void KisLayerBox::setUpdatesAndSignalsEnabled(bool enable)
{
    setUpdatesEnabled(enable);
    m_lst -> intOpacity -> setUpdatesEnabled(enable);
    m_lst -> cmbComposite -> setUpdatesEnabled(enable);

    list() -> blockSignals(!enable);
    m_lst -> intOpacity -> blockSignals(!enable);
    m_lst -> cmbComposite -> blockSignals(!enable);
}

void KisLayerBox::updateAll()
{
    update();
    m_lst -> intOpacity -> update();
    m_lst -> cmbComposite -> update();
    updateUI();
}

QIconSet KisLayerBox::loadIconSet(const QString& filename, KIconLoader&
                                    il, int size)
{
    QIconSet icon = il.loadIconSet(filename, KIcon::NoGroup, size);

    if (icon.isNull())
        KMessageBox::error(0, i18n("Cannot find %1.").arg(filename),
                           i18n("Canvas"));

    return icon;
}

#include "kis_layerbox.moc"
