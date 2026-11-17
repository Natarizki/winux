/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "host_api.h"
#include "memory_manager.h"
#include "vma_manager.h"
#include <cassert>
#include <vector>
int main() {
    const size_t page=(size_t)winux_host_page_size();
    winux_memory_manager *mm=nullptr; winux_vma_manager *vm=nullptr;
    winux_host_memory_region r{}; size_t count=0;
    assert(page && winux_memory_manager_create(&mm));
    assert(winux_memory_manager_reserve(mm,page*8,&r));
    assert(winux_vma_manager_create(mm,r.base,r.size,&vm));
    assert(winux_vma_map_anonymous_private(vm,0,page*4,WINUX_VMA_PROT_READ|WINUX_VMA_PROT_WRITE));
    assert(!winux_vma_map_anonymous_private(vm,page*2,page,WINUX_VMA_PROT_READ));
    assert(winux_vma_protect(vm,page,page*2,WINUX_VMA_PROT_READ));
    assert(winux_vma_snapshot(vm,nullptr,0,&count)==0 && count==3);
    std::vector<winux_vma_descriptor> a(count);
    assert(winux_vma_snapshot(vm,a.data(),a.size(),&count));
    assert(a[0].offset==0 && a[0].length==page && a[0].protection==(WINUX_VMA_PROT_READ|WINUX_VMA_PROT_WRITE));
    assert(a[1].offset==page && a[1].length==page*2 && a[1].protection==WINUX_VMA_PROT_READ);
    assert(a[2].offset==page*3 && a[2].length==page);
    assert(winux_vma_protect(vm,page,page*2,WINUX_VMA_PROT_READ|WINUX_VMA_PROT_WRITE));
    assert(winux_vma_snapshot(vm,a.data(),a.size(),&count) && count==1);
    assert(a[0].length==page*4);
    assert(winux_vma_unmap(vm,page,page*2));
    assert(winux_vma_snapshot(vm,nullptr,0,&count)==0 && count==2);
    a.resize(count); assert(winux_vma_snapshot(vm,a.data(),a.size(),&count));
    assert(a[0].offset==0 && a[0].length==page);
    assert(a[1].offset==page*3 && a[1].length==page);
    assert(!winux_vma_protect(vm,page,page,WINUX_VMA_PROT_READ));
    assert(!winux_vma_unmap(vm,page*6,page));
    winux_vma_manager_destroy(vm);
    assert(winux_memory_manager_release(mm,r.base));
    winux_memory_manager_destroy(mm);
    return 0;
}
