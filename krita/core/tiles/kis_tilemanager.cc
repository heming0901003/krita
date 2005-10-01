/*
 *  Copyright (c) 2005 Bart Coppens <kde@bartcoppens.be>
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

#include <kdebug.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <kstaticdeleter.h>
#include <kglobal.h>
#include <kconfig.h>

#include "kis_tileddatamanager.h"
#include "kis_tile.h"
#include "kis_tilemanager.h"

// Note: the cache file doesn't get deleted when we crash and so :(

KisTileManager* KisTileManager::m_singleton = 0;

static KStaticDeleter<KisTileManager> staticDeleter;

KisTileManager::KisTileManager() {

    Q_ASSERT(KisTileManager::m_singleton == 0);
    KisTileManager::m_singleton = this;
    m_fileSize = 0;
    m_bytesInMem = 0;
    m_bytesTotal = 0;

    // Hardcoded (at the moment only?): 4 pools of 1000 tiles each
    m_tilesPerPool = 1000;
    m_pools = new Q_UINT8*[4];
    m_poolPixelSizes = new Q_INT32[4];
    m_poolFreeList = new PoolFreeList[4];
    for (int i = 0; i < 4; i++) {
        m_pools[i] = 0;
        m_poolPixelSizes[i] = 0;
        m_poolFreeList[i] = PoolFreeList();
    }

    m_currentInMem = 0;

    KConfig * cfg = KGlobal::config();
    cfg->setGroup("");
    m_maxInMem = cfg->readNumEntry("maxtilesinmem",  500);
    m_swappiness = cfg->readNumEntry("swappiness", 100);

    m_tileSize = KisTile::WIDTH * KisTile::HEIGHT;
    m_freeLists.reserve(8);

    counter = 0;
}

KisTileManager::~KisTileManager() {
    kdDebug(DBG_AREA_TILES) << "Destructing TileManager: unmapping everything" << endl;

    if (!m_freeLists.empty()) { // See if there are any nonempty freelists
        FreeListList::iterator listsIt = m_freeLists.begin();
        FreeListList::iterator listsEnd = m_freeLists.end();

        while(listsIt != listsEnd) {
            if ( ! (*listsIt).empty() ) {
                FreeList::iterator it = (*listsIt).begin();
                FreeList::iterator end = (*listsIt).end();

                while (it != end) {
                    // munmap it
                    munmap((*it) -> pointer, (*it) -> size);
                    delete *it;
                    ++it;
                }
                (*listsIt).clear();
            }
            ++listsIt;
        }
        m_freeLists.clear();
    }

    kdDebug(DBG_AREA_TILES) << "Destructing TileManager: deleting file" << endl;
    m_tempFile.close();
    m_tempFile.unlink();
}

KisTileManager* KisTileManager::instance() {
    if(KisTileManager::m_singleton == 0) {
        staticDeleter.setObject(KisTileManager::m_singleton, new KisTileManager());
        Q_CHECK_PTR(KisTileManager::m_singleton);
    }
    return KisTileManager::m_singleton;
}

void KisTileManager::registerTile(KisTile* tile) {
    TileInfo* info = new TileInfo();
    info -> tile = tile;
    info -> inMem = true;
    info -> filePos = -1;
    info -> size = tile -> WIDTH * tile -> HEIGHT * tile -> m_pixelSize;
    info -> fsize = 0; // the size in the file
    info -> validNode = true;

    m_tileMap[tile] = info;
    m_swappableList.push_back(info);
    info -> node = -- m_swappableList.end();

    m_currentInMem++;
    m_bytesTotal += info -> size;
    m_bytesInMem += info -> size;

    doSwapping();

    if (++counter % 50 == 0)
        printInfo();
}

void KisTileManager::deregisterTile(KisTile* tile) {
    Q_ASSERT(m_tileMap.contains(tile));

    TileInfo* info = m_tileMap[tile];

    if (info -> filePos >= 0) { // It is mmapped
        // To freelist
        FreeInfo* freeInfo = new FreeInfo();
        freeInfo -> pointer = tile -> m_data;
        freeInfo -> filePos = info -> filePos;
        freeInfo -> size = info -> fsize;
        int pixelSize = (info -> size / m_tileSize);
        if (m_freeLists.capacity() <= pixelSize)
            m_freeLists.resize(pixelSize + 1);
        m_freeLists[pixelSize].push_back(freeInfo);

        madvise(info -> tile -> m_data, info -> fsize, MADV_DONTNEED);

        // the KisTile will attempt to delete its data. This is of course silly when
        // it was mmapped. So change the m_data to NULL, which is safe to delete
        tile -> m_data = 0;
    } else {
        m_bytesInMem -= info -> size;
        m_currentInMem--;
    }

    if (info -> validNode) {
        m_swappableList.erase(info -> node);
        info -> validNode = false;
    }

    m_bytesTotal -= info -> size;

    delete info;
    m_tileMap.erase(tile);


    doSwapping();
}

void KisTileManager::ensureTileLoaded(KisTile* tile) {
    TileInfo* info = m_tileMap[tile];
    if (info -> validNode) {
        m_swappableList.erase(info -> node);
        info -> validNode = false;
    }

    if (!info -> inMem) {
        fromSwap(info);
    }
}

void KisTileManager::maySwapTile(KisTile* tile) {
    TileInfo* info = m_tileMap[tile];
    m_swappableList.push_back(info);
    info -> validNode = true;
    info -> node = -- m_swappableList.end();

    doSwapping();
}

void KisTileManager::fromSwap(TileInfo* info) {
    Q_ASSERT(!info -> inMem);

    doSwapping();

    // ### check return value!
    madvise(info -> tile -> m_data, info -> size, MADV_WILLNEED);

    info -> inMem = true;
    m_currentInMem++;
    m_bytesInMem += info -> size;
}

void KisTileManager::toSwap(TileInfo* info) {
    Q_ASSERT(info -> inMem);

    if (info -> filePos < 0) {
        // This tile is not yet in the file. Save it there
        // ### check return values of mmap functions!
        KisTile *tile = info -> tile;
        Q_UINT8* data = 0;
        int pixelSize = (info -> size / m_tileSize);
        if (m_freeLists.capacity() > pixelSize) {
            if (!m_freeLists[pixelSize].empty()) {
                // found one
                FreeList::iterator it = m_freeLists[pixelSize].begin();
                data = (*it) -> pointer;
                info -> filePos = (*it) -> filePos;
                info -> fsize = (*it) -> size;

                delete *it;
                m_freeLists[pixelSize].erase(it);
            }
        }

        if (data == 0) { // No position found or free, create a new
            long pagesize = sysconf(_SC_PAGESIZE);
            int newsize = m_fileSize + info -> size;
            newsize = newsize + newsize % pagesize;

            ftruncate(m_tempFile.handle(), newsize);

            data = (Q_UINT8*) mmap(0, info -> size,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED,
                                   m_tempFile.handle(), m_fileSize);

            if (data == (Q_UINT8*)-1) {
                kdDebug(DBG_AREA_TILES) << "mmap failed: errno is " << errno << "; we're going to crash...\n";
            }

            info -> fsize = info -> size;
            info -> filePos = m_fileSize;
            m_fileSize = newsize;
        }

        madvise(data, info -> fsize, MADV_WILLNEED);
        memcpy(data, tile -> m_data, info -> size);
        madvise(data, info -> fsize, MADV_DONTNEED);

        if (isPoolTile(tile -> m_data, tile -> m_pixelSize))
            reclaimTileToPool(tile -> m_data, tile -> m_pixelSize);
        else
            delete[] tile -> m_data;

        tile -> m_data = data;
    } else {
        madvise(info -> tile -> m_data, info -> fsize, MADV_DONTNEED);
    }

    info -> inMem = false;
    m_currentInMem--;
    m_bytesInMem -= info -> size;
}

void KisTileManager::doSwapping() {
    if (m_currentInMem <= m_maxInMem)
        return;
#if 1 // enable this to enable swapping
    Q_INT32 count = QMIN(m_swappableList.size(), m_swappiness);

    for (Q_INT32 i = 0; i < count; i++) {
        toSwap(m_swappableList.front());
        m_swappableList.front() -> validNode = false;
        m_swappableList.pop_front();
    }
#endif
}

void KisTileManager::printInfo() {
    kdDebug(DBG_AREA_TILES) << m_bytesInMem << " out of " << m_bytesTotal << " bytes in memory\n";
    kdDebug(DBG_AREA_TILES) << m_currentInMem << " out of " << m_tileMap.size() << " tiles in memory\n";
    kdDebug(DBG_AREA_TILES) << m_swappableList.size() << " elements in the swapable list\n";
    kdDebug(DBG_AREA_TILES) << "Freelists information\n";
    for (int i = 0; i < m_freeLists.capacity(); i++) {
        if ( ! m_freeLists[i].empty() ) {
            kdDebug(DBG_AREA_TILES) << m_freeLists[i].size()
                    << " elements in the freelist for pixelsize " << i << "\n";
        }
    }
    kdDebug(DBG_AREA_TILES) << "Pool stats (" <<  m_tilesPerPool << " tiles per pool)" << endl;
    for (int i = 0; i < 4; i++) {
        if (m_pools[i]) {
            kdDebug() << "Pool " << i << ": Freelist count: " << m_poolFreeList[i].count()
                    << ", pixelSize: " << m_poolPixelSizes[i] << endl;
        }
    }
    kdDebug(DBG_AREA_TILES) << endl;
}

Q_UINT8* KisTileManager::requestTileData(Q_INT32 pixelSize) {
    Q_UINT8* data = findTileFor(pixelSize);
    if ( data ) {
        return data;
    }
    return new Q_UINT8[m_tileSize * pixelSize];
}

void KisTileManager::dontNeedTileData(Q_UINT8* data, Q_INT32 pixelSize) {
    if (isPoolTile(data, pixelSize)) {
        reclaimTileToPool(data, pixelSize);
    } else
        delete[] data;
}

Q_UINT8* KisTileManager::findTileFor(Q_INT32 pixelSize) {
    for (int i = 0; i < 4; i++) {
        if (m_poolPixelSizes[i] == pixelSize) {
            if (!m_poolFreeList[i].isEmpty()) {
                Q_UINT8* data = m_poolFreeList[i].front();
                m_poolFreeList[i].pop_front();
                return data;
            }
        }
        if (m_pools[i] == 0) {
            // allocate new pool
            m_poolPixelSizes[i] = pixelSize;
            m_pools[i] = new Q_UINT8[pixelSize * m_tileSize * m_tilesPerPool];
            // j = 1 because we return the first element, so no need to add it to the freelist
            for (int j = 1; j < m_tilesPerPool; j++)
                m_poolFreeList[i].append(&m_pools[i][j * pixelSize * m_tileSize]);
            return m_pools[i];
        }
    }
    return 0;
}

bool KisTileManager::isPoolTile(Q_UINT8* data, Q_INT32 pixelSize) {
    if (data == 0)
        return false;
    for (int i = 0; i < 4; i++) {
        if (m_poolPixelSizes[i] == pixelSize)
            return data >= m_pools[i]
                    && data < m_pools[i] + pixelSize * m_tileSize * m_tilesPerPool;
    }
    return false;
}

void KisTileManager::reclaimTileToPool(Q_UINT8* data, Q_INT32 pixelSize) {
    for (int i = 0; i < 4; i++) {
        if (m_poolPixelSizes[i] == pixelSize)
            if (data >= m_pools[i]
                && data < m_pools[i] + pixelSize * m_tileSize * m_tilesPerPool)
                m_poolFreeList[i].append(data);
    }
}

void KisTileManager::configChanged() {
    KConfig * cfg = KGlobal::config();
    cfg->setGroup("");
    m_maxInMem = cfg->readNumEntry("maxtilesinmem",  500);
    m_swappiness = cfg->readNumEntry("swappiness", 100);

    kdDebug(DBG_AREA_TILES) << "TileManager has new config: maxinmem: " << m_maxInMem
            << " swappiness: " << m_swappiness << endl;

    doSwapping();
}
