// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
    struct run *next;
};

struct
{
    struct spinlock lock;
    struct run *freelist;
} kmem;

struct
{
    struct spinlock lock;
    uint counter[(PHYSTOP - KERNBASE) / PGSIZE];
} cow_ref;

void kinit()
{
    initlock(&kmem.lock, "kmem");
    initlock(&cow_ref.lock, "cow_ref");
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    {
        set_ref(p, 1);
        kfree(p);
    }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
    struct run *r;

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // 只有当引用计数为0了才回收空间
    // 否则只是将引用计数减1
    lock_ref();
    if (--cow_ref.counter[pgindex(pa)] == 0)
    {
        release_ref();

        r = (struct run *)pa;

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    }
    else
    {
        release_ref();
    }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist; // 获取内存
    if (r)
    {
        kmem.freelist = r->next; // 从空闲链表中删除获取的内存
        set_ref(r, 1);
    }
    release(&kmem.lock);

    if (r)
        memset((char *)r, 5, PGSIZE); // fill with junk
    return (void *)r;
}

uint64 pgindex(void *pa)
{
    return ((uint64)pa - KERNBASE) / PGSIZE;
}

void decrease_ref(void *pa)
{
    lock_ref();
    cow_ref.counter[pgindex(pa)]--;
    release_ref();
}

void increase_ref(void *pa)
{
    lock_ref();
    cow_ref.counter[pgindex(pa)]++;
    release_ref();
}

void set_ref(void *pa, uint x)
{
    lock_ref();
    cow_ref.counter[pgindex(pa)] = x;
    release_ref();
}

uint get_ref(void *pa)
{
    return cow_ref.counter[pgindex(pa)];
}

void lock_ref()
{
    acquire(&cow_ref.lock);
}

void release_ref()
{
    release(&cow_ref.lock);
}