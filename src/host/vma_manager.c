/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vma_manager.h"
#include "host_api.h"
#include <stdlib.h>
#include <string.h>
struct vma_node { winux_vma_descriptor area; struct vma_node *next; };
struct winux_vma_manager {
    winux_memory_manager *memory;
    void *base;
    size_t size, page_size;
    winux_host_mutex *mutex;
    struct vma_node *areas;
};
static int lock_manager(winux_vma_manager *m) { uint32_t e=0; return winux_host_mutex_lock(m->mutex,&e); }
static void unlock_manager(winux_vma_manager *m) { uint32_t e=0; (void)winux_host_mutex_unlock(m->mutex,&e); }
static int valid_protection(uint32_t p) {
    return (p & ~(uint32_t)(WINUX_VMA_PROT_READ|WINUX_VMA_PROT_WRITE))==0u;
}
static int valid_range(winux_vma_manager *m,size_t o,size_t n) {
    return n!=0u && o % m->page_size==0u && n % m->page_size==0u &&
        o<=m->size && n<=m->size-o;
}
static winux_host_memory_protection host_protection(uint32_t p) {
    if (p==WINUX_VMA_PROT_NONE) return WINUX_HOST_MEMORY_NO_ACCESS;
    if ((p & WINUX_VMA_PROT_WRITE)!=0u) return WINUX_HOST_MEMORY_READ_WRITE;
    return WINUX_HOST_MEMORY_READ_ONLY;
}
static struct vma_node *node_new(size_t o,size_t n,uint32_t p) {
    struct vma_node *x=(struct vma_node*)calloc(1u,sizeof(*x));
    if (x) { x->area.offset=o; x->area.length=n; x->area.protection=p;
        x->area.kind=WINUX_VMA_ANONYMOUS_PRIVATE; }
    return x;
}
static void list_free(struct vma_node *p) {
    while(p) { struct vma_node *n=p->next; free(p); p=n; }
}
static int append(struct vma_node **head,struct vma_node **tail,
    size_t o,size_t n,uint32_t prot) {
    struct vma_node *x;
    if (!n) return 1;
    if (*tail && (*tail)->area.offset+(*tail)->area.length==o &&
        (*tail)->area.protection==prot) { (*tail)->area.length+=n; return 1; }
    x=node_new(o,n,prot); if(!x) return 0;
    if(*tail) (*tail)->next=x; else *head=x; *tail=x; return 1;
}
int winux_vma_manager_create(winux_memory_manager *memory,void *base,size_t size,
    winux_vma_manager **out) {
    winux_vma_manager *m; uint32_t e=0;
    if(!out) return 0; *out=NULL;
    if(!memory || !base || !size || size%(size_t)winux_host_page_size()!=0u) return 0;
    m=(winux_vma_manager*)calloc(1u,sizeof(*m)); if(!m) return 0;
    m->memory=memory; m->base=base; m->size=size;
    m->page_size=(size_t)winux_host_page_size();
    if(!m->page_size || !winux_host_mutex_create(&m->mutex,&e)) { free(m); return 0; }
    *out=m; return 1;
}
void winux_vma_manager_destroy(winux_vma_manager *m) {
    if(!m) return; list_free(m->areas); winux_host_mutex_destroy(m->mutex); free(m);
}
int winux_vma_map_anonymous_private(winux_vma_manager *m,size_t o,size_t n,uint32_t p) {
    struct vma_node **link,*x; size_t end; int ok;
    if(!m || !valid_protection(p) || !valid_range(m,o,n) || !lock_manager(m)) return 0;
    end=o+n;
    for(x=m->areas;x;x=x->next) {
        size_t xe=x->area.offset+x->area.length;
        if(o<xe && x->area.offset<end) { unlock_manager(m); return 0; }
    }
    if(!winux_memory_manager_commit(m->memory,m->base,o,n)) { unlock_manager(m); return 0; }
    ok=(p== (WINUX_VMA_PROT_READ|WINUX_VMA_PROT_WRITE)) ||
        winux_memory_manager_protect(m->memory,m->base,o,n,host_protection(p));
    if(!ok) { (void)winux_memory_manager_decommit(m->memory,m->base,o,n); unlock_manager(m); return 0; }
    x=node_new(o,n,p);
    if(!x) { (void)winux_memory_manager_decommit(m->memory,m->base,o,n); unlock_manager(m); return 0; }
    for(link=&m->areas;*link && (*link)->area.offset<o;link=&(*link)->next) {}
    x->next=*link; *link=x;
    /* Coalesce adjacent equal-protection areas without allocating. */
    { struct vma_node *cur=m->areas;
      while(cur && cur->next) {
        struct vma_node *next=cur->next;
        if(cur->area.offset+cur->area.length==next->area.offset &&
           cur->area.protection==next->area.protection) {
            cur->area.length+=next->area.length;
            cur->next=next->next;
            free(next);
        } else cur=next;
      } }
    unlock_manager(m); return 1;
}
static int rebuild_range(winux_vma_manager *m,size_t o,size_t n,uint32_t p,int remove) {
    struct vma_node *head=NULL,*tail=NULL,*cur; size_t end=o+n,cursor=o;
    int touched=0,ok;
    for(cur=m->areas;cur;cur=cur->next) {
        size_t s=cur->area.offset,e=s+cur->area.length;
        if(e<=o || s>=end) {
            if(!append(&head,&tail,s,e-s,cur->area.protection)) goto alloc_fail;
            continue;
        }
        if(s>cursor) goto invalid_range;
        touched=1;
        if(s<o && !append(&head,&tail,s,o-s,cur->area.protection)) goto alloc_fail;
        { size_t is=s>o?s:o, ie=e<end?e:end;
          if(!remove && !append(&head,&tail,is,ie-is,p)) goto alloc_fail;
          cursor=ie;
        }
        if(e>end && !append(&head,&tail,end,e-end,cur->area.protection)) goto alloc_fail;
    }
    if(!touched || cursor<end) goto invalid_range;
    if(remove) ok=winux_memory_manager_decommit(m->memory,m->base,o,n);
    else ok=winux_memory_manager_protect(m->memory,m->base,o,n,host_protection(p));
    if(!ok) { list_free(head); return 0; }
    list_free(m->areas); m->areas=head; return 1;
alloc_fail:
    list_free(head); return 0;
invalid_range:
    list_free(head); return 0;
}
int winux_vma_protect(winux_vma_manager *m,size_t o,size_t n,uint32_t p) {
    int r; if(!m || !valid_protection(p) || !valid_range(m,o,n) || !lock_manager(m)) return 0;
    r=rebuild_range(m,o,n,p,0); unlock_manager(m); return r;
}
int winux_vma_unmap(winux_vma_manager *m,size_t o,size_t n) {
    int r; if(!m || !valid_range(m,o,n) || !lock_manager(m)) return 0;
    r=rebuild_range(m,o,n,0,1); unlock_manager(m); return r;
}
int winux_vma_snapshot(winux_vma_manager *m,winux_vma_descriptor *out,size_t cap,size_t *count) {
    struct vma_node *p; size_t n=0,i=0;
    if(!m || !count || (cap && !out) || !lock_manager(m)) return 0;
    for(p=m->areas;p;p=p->next) ++n; *count=n;
    if(cap<n) { unlock_manager(m); return 0; }
    for(p=m->areas;p;p=p->next) out[i++]=p->area;
    unlock_manager(m); return 1;
}
