/*
  Copyright (c) 2005 Gábor Lehel <illissius@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include <kiconloader.h>
#include <klocale.h>
#include <kpopupmenu.h>
#include <koPartSelectAction.h>

#include "kis_layerlist.h"


KisLayerList::KisLayerList( QWidget *parent, const char *name )
    : super( parent, name )
{
    m_partLayerAction = new KoPartSelectAction( i18n( "New &Object Layer..." ), "gear",
                                                this, "insert_part_layer" );
}

void KisLayerList::constructMenu( LayerItem *layer )
{
    super::constructMenu( layer );

    contextMenu()->removeItem( MenuItems::NewLayer );
    contextMenu()->removeItem( MenuItems::NewFolder );
    contextMenu()->changeItem( MenuItems::RemoveLayer, i18n( "&Remove Layer" ) );

    if( layer )
    {
        static KPopupMenu submenu;
        submenu.clear();
        submenu.insertItem( SmallIconSet( "file" ), i18n( "&Layer..." ), MenuItems::NewLayer );
        submenu.insertItem( SmallIconSet( "folder" ), i18n( "&Group Layer..." ), MenuItems::NewFolder );
        m_partLayerAction->setText( i18n( "&Object Layer..." ) );
        m_partLayerAction->plug( &submenu );

        contextMenu()->insertItem( SmallIconSet( "filenew" ), i18n( "&New" ), &submenu );
    }
    else
    {
        contextMenu()->insertItem( SmallIconSet( "filenew" ), i18n( "&New Layer..." ), MenuItems::NewLayer );
        contextMenu()->insertItem( SmallIconSet( "foldernew" ), i18n( "New &Group Layer..." ), MenuItems::NewFolder );
        m_partLayerAction->setText( i18n( "New &Object Layer..." ) );
        m_partLayerAction->plug( contextMenu() );
    }
}

void KisLayerList::menuActivated( int id, LayerItem *layer )
{
    const QValueList<LayerItem*> selected = selectedLayers();
    LayerItem *parent = ( layer && layer->isFolder() ) ? layer : 0;
    LayerItem *after = 0;
    if( !parent && layer )
    {
        parent = layer->parent();
        if( parent && after != parent->firstChild() )
            after = parent->firstChild();
        while( after && after->nextSibling() != layer )
            after = after->nextSibling();
    }
    switch( id )
    {
        case MenuItems::NewLayer:
            emit requestNewLayer( parent, after );
            emit requestNewLayer( parent ? parent->id() : -1, after ? after->id() : -1 );
            break;
        case MenuItems::NewFolder:
            emit requestNewFolder( parent, after );
            emit requestNewFolder( parent ? parent->id() : -1, after ? after->id() : -1 );
            break;
        case MenuItems::RemoveLayer:
            {
                QValueList<int> ids;
                for( int i = 0, n = selected.count(); i < n; ++i )
                {
                    ids.append( selected[i]->id() );
                    emit requestRemoveLayer( selected[i]->id() );
                }
                emit requestRemoveLayers( ids );
            }
            for( int i = 0, n = selected.count(); i < n; ++i )
                emit requestRemoveLayer( selected[i] );
            emit requestRemoveLayers( selected );
            break;
        case MenuItems::LayerProperties:
            if( layer )
            {
                emit requestLayerProperties( layer );
                emit requestLayerProperties( layer->id() );
            }
            break;
        default:
            if( id >= MenuItems::COUNT && layer )
                super::menuActivated( id, layer );
            else if( id != -1 ) //object layer was selected
            {
                emit requestNewObjectLayer( parent, after, m_partLayerAction->documentEntry() );
                emit requestNewObjectLayer( parent ? parent->id() : -1, after ? after->id() : -1, m_partLayerAction->documentEntry() );
            }
    }
}

#include "kis_layerlist.moc"
