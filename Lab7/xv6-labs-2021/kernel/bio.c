// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct
{
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

int get_bucket(int n)
{
  return n % NBUCKET;
}

void binit(void)
{
  struct buf *b;

  for (int i=0; i<NBUCKET; i++)
  {
    char name[10];
    snprintf(name, 10, "bcache.hash%d", i);
    initlock(&bcache.lock[i], name);
    bcache.head[i].next = 0;
  }

  // Create linked list of buffers
  // bcache.head[0].next = &bcache.buf[0];
  // printf("%d %d\n", 0, bcache.head[0].next);
  for (int i = 0; i < NBUF; i++)
  {
    b = bcache.buf + i;
    int id = get_bucket(i);
    b->next = bcache.head[id].next;
    bcache.head[id].next = b;
    initsleeplock(&b->lock, "buffer");
  }
  // for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  // {
  //   b->next = b + 1;
  //   initsleeplock(&b->lock, "buffer");
  // }
  // // set the next of the last buffer to NULL
  // (b-1)->next = 0;
  // printf("%d\n", bcache.head[0].next);
}

void write_cache(struct buf *take_buf, uint dev, uint blockno)
{
  take_buf->dev = dev;
  take_buf->blockno = blockno;
  take_buf->valid = 0;
  take_buf->refcnt = 1;
  take_buf->time = ticks;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *a, *b;
  struct buf *take_prev = 0, *take_buf = 0;

  int id = get_bucket(blockno);

  acquire(&bcache.lock[id]);

  // Is the block already cached?
  for (b = bcache.head[id].next; b; b = b->next)
  {
    // Already cached
    if (b->dev == dev && b->blockno == blockno)
    {
      b->time = ticks;
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
    // free block
    if (b->refcnt == 0)
    {
      take_buf = b;
    }
  }

  // printf("%d: %d\n", ticks, take_buf);
  // printf("%d\n", bcache.head[0].next);

  if (take_buf)
  {
    write_cache(take_buf, dev, blockno);
    release(&bcache.lock[id]);
    acquiresleep(&take_buf->lock);
    return take_buf;
  }

  int cur_min_time = __UINT32_MAX__;
  int cur_min_bucket = -1;
  take_buf = 0;
  for (int i=0; i<NBUCKET; i++)
  {
    if (i == id) continue;

    acquire(&bcache.lock[i]);

    // printf("%d %d\n", i, bcache.head[i].next);

    for (a=&bcache.head[i], b=bcache.head[i].next; b; a=a->next, b=b->next)
    {
      if (b->refcnt == 0)
      {
        if (b->time < cur_min_time)
        {
          if (cur_min_bucket != -1 && cur_min_bucket != i)
          {
            release(&bcache.lock[cur_min_bucket]);
          }
          cur_min_time = b->time;
          cur_min_bucket = i;
          take_prev = a;  // the previous block of free block
          take_buf = b;   // the free block
        }
      }
    }

    if (cur_min_bucket != i && cur_min_bucket != -1)
    {
      release(&bcache.lock[i]);
    }
  }

  if (!take_buf)
    panic("bget: no buffers");
  
  take_prev->next = take_buf->next;
  // printf("release1 %d\n", cur_min_bucket);
  release(&bcache.lock[cur_min_bucket]);

  take_buf->next = bcache.head[id].next;
  bcache.head[id].next = take_buf;
  write_cache(take_buf, dev, blockno);

  // printf("release2");
  release(&bcache.lock[id]);
  acquiresleep(&take_buf->lock);

  return take_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = get_bucket(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}

void bpin(struct buf *b)
{
  int id = get_bucket(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void bunpin(struct buf *b)
{
  int id = get_bucket(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}
