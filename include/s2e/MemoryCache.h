#ifndef _S2E_MEMORY_CACHE_

#define _S2E_MEMORY_CACHE_

#include <vector>
#include <inttypes.h>
#include <llvm/ADT/SmallVector.h>
#include <iostream>

namespace s2e {

template <class T, unsigned OBJSIZE_BITS, unsigned PAGESIZE_BITS, unsigned SUPERPAGESIZE_BITS>
class MemoryCache
{
private:
    struct ThirdLevel {
        T level3[1<<(PAGESIZE_BITS-OBJSIZE_BITS)];
        ThirdLevel() {
            for (unsigned i=0; i<(1<<(PAGESIZE_BITS-OBJSIZE_BITS)); ++i) {
                level3[i] = T();
            }
        }
    };

    struct SecondLevel {
        ThirdLevel* level2[1<<(SUPERPAGESIZE_BITS-PAGESIZE_BITS)];

        SecondLevel() {
            for (unsigned i=0; i<(1<<(SUPERPAGESIZE_BITS-PAGESIZE_BITS)); ++i) {
                level2[i] = NULL;
            }
        }

        ~SecondLevel() {
            for (unsigned i=0; i<(1<<(SUPERPAGESIZE_BITS-PAGESIZE_BITS)); ++i) {
                if (level2[i]) {
                    delete level2[i];
                    level2[i] = NULL;
                }
            }
        }
    };

    SecondLevel **m_level1;
    uint64_t m_hostAddrStart;
    uint64_t m_size;
    unsigned m_pagecount;

    inline void resize()
    {
        uint64_t mask = (1<<SUPERPAGESIZE_BITS)-1;
        uint64_t pagecount = m_size >> SUPERPAGESIZE_BITS;
        if (m_size & mask) {
            ++pagecount;
        }

        m_pagecount = pagecount;
        m_level1 = new SecondLevel*[pagecount];
        for (unsigned i=0; i< pagecount; ++i) {
            m_level1[i] = NULL;
        }
    }

public:
    MemoryCache(uint64_t hostAddrStart, uint64_t size)
    {
        m_hostAddrStart = hostAddrStart;
        m_size = size;
        resize();
    }

    //XXX: Clone an empty cache for now
    MemoryCache(const MemoryCache &one) {
        m_hostAddrStart = one.m_hostAddrStart;
        m_size = one.m_size;
        resize();
    }

    ~MemoryCache() {
        flushCache();
    }

    inline uint64_t getSize() const {
        return m_size;
    }

    inline uint64_t getStart() const {
        return m_hostAddrStart;
    }

    inline void flushCache() {
        for (unsigned i=0; i<m_pagecount; ++i) {
            if (m_level1[i]) {
                delete m_level1[i];
                m_level1[i] = NULL;
            }
        }
    }

    inline bool contains(uint64_t hostAddress) {
        return (hostAddress >= m_hostAddrStart) && (hostAddress < m_hostAddrStart + m_size);
    }

    inline void put(uint64_t hostAddress, const T &obj)
    {
        uint64_t offset = hostAddress - m_hostAddrStart;
        uint64_t level1 = offset >> SUPERPAGESIZE_BITS;
        uint64_t level2 = (offset & ((1<<SUPERPAGESIZE_BITS)-1)) >> PAGESIZE_BITS;
        uint64_t level3 = (offset >> OBJSIZE_BITS) & ((1<<(PAGESIZE_BITS-OBJSIZE_BITS))-1);

        SecondLevel *ptrLevel2;
        if (!(ptrLevel2 = m_level1[level1])) {
            ptrLevel2 = new SecondLevel();
            m_level1[level1] = ptrLevel2;
        }

        ThirdLevel *ptrLevel3;
        if (!(ptrLevel3 = ptrLevel2->level2[level2])) {
            ptrLevel3 = new ThirdLevel();
            ptrLevel2->level2[level2] = ptrLevel3;
        }

        assert(level3 < (1<<(PAGESIZE_BITS-OBJSIZE_BITS)));

        ptrLevel3->level3[level3] = obj;
    }

    inline T get(uint64_t hostAddress)
    {
        uint64_t offset = hostAddress - m_hostAddrStart;
        uint64_t level1 = offset >> SUPERPAGESIZE_BITS;
        uint64_t level2 = (offset & ((1<<SUPERPAGESIZE_BITS)-1)) >> PAGESIZE_BITS;
        uint64_t level3 = (offset >> OBJSIZE_BITS) & ((1<<(PAGESIZE_BITS-OBJSIZE_BITS))-1);

        SecondLevel *ptrLevel2;
        if (!(ptrLevel2 = m_level1[level1])) {
            return T();
        }

        ThirdLevel *ptrLevel3;
        if (!(ptrLevel3 = ptrLevel2->level2[level2])) {
            return T();
        }

        return ptrLevel3->level3[level3];
    }

    inline T* getArray(uint64_t hostAddress)
    {
        uint64_t offset = hostAddress - m_hostAddrStart;
        uint64_t level1 = offset >> SUPERPAGESIZE_BITS;
        uint64_t level2 = (offset & ((1<<SUPERPAGESIZE_BITS)-1)) >> PAGESIZE_BITS;

        SecondLevel *ptrLevel2;
        if (!(ptrLevel2 = m_level1[level1])) {
            return NULL;
        }

        ThirdLevel *ptrLevel3;
        if (!(ptrLevel3 = ptrLevel2->level2[level2])) {
            return NULL;
        }

        return ptrLevel3->level3;
    }
};

template <class T, unsigned OBJSIZE_BITS, unsigned PAGESIZE_BITS, unsigned SUPERPAGESIZE_BITS>
class MemoryCachePool
{
private:
    typedef MemoryCache<T,OBJSIZE_BITS,PAGESIZE_BITS,SUPERPAGESIZE_BITS> MemoryCacheT;
    typedef llvm::SmallVector<MemoryCacheT*, 10> Caches;
    Caches m_caches;

public:
    MemoryCachePool() {

    }

    MemoryCachePool(const MemoryCachePool &one) {
        for (unsigned i=0; i<one.m_caches.size(); ++i) {
            m_caches.push_back(new MemoryCacheT(*one.m_caches[i]));
        }

    }

    ~MemoryCachePool() {
        for (unsigned i=0; i<m_caches.size(); ++i) {
            delete m_caches[i];
        }
    }

    void print()
    {
	for ( auto cache_entry : m_caches ) {
            std::cout << std::hex <<
                    "Cache start=0x" << cache_entry->getStart() <<
                    " size=0x" << cache_entry->getSize() << std::endl << std::endl;
        }
    }

    //We sort the cache be decreasing size.
    //The idea is that most accesses fall in the RAM, so it will
    //be found first in the list.
    void registerPool(uint64_t hostAddrStart, uint64_t size)
    {
        assert((hostAddrStart & ((1<<PAGESIZE_BITS)-1)) == 0);
        MemoryCacheT *mc = new MemoryCacheT(hostAddrStart, size);
        if (m_caches.size() == 0) {
            m_caches.push_back(mc);
            return;
        }

        //Locate the place to insert
        auto it = m_caches.begin();
        for (; it != m_caches.end(); ++it) {
            if (size > (*it)->getSize()) {
                break;
            }
        }

        m_caches.insert(it, mc);
    }

    void put(uint64_t hostAddress, const T &obj)
    {
	for ( auto cache_entry : m_caches ) {
            if (cache_entry->contains(hostAddress)) {
                cache_entry->put(hostAddress, obj);
                return;
            }
        }
    }

    T* getArray(uint64_t hostAddress) {
	for ( auto cache_entry : m_caches ) {
            if (cache_entry->contains(hostAddress)) {
                return cache_entry->getArray(hostAddress);
            }
        }
        return NULL;
    }

    T get(uint64_t hostAddress)
    {
	for ( auto cache_entry : m_caches ) {
            if (cache_entry->contains(hostAddress)) {
                return cache_entry->get(hostAddress);
            }
        }
        return T();
    }

};

}

#endif
