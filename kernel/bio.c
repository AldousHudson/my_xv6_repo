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

#define NBUCKETS 13

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.

  // 通过哈希桶来代替链表，对块进行分组，每个哈希桶有一个linked list及一个lock
  struct buf hashbucket[NBUCKETS];
} bcache;

// 创建hash函数
int
get_hash(int blockno)
{
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  // 给每个哈希桶初始化bcache锁和linked list
  for (int i = 0; i < NBUCKETS; i++){
    initlock(&bcache.lock[i], "bcache_bucket");
    // Create linked list of buffers
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int hash = get_hash(b->blockno);
    b->next = bcache.hashbucket[hash].next;
    b->prev = &bcache.hashbucket[hash];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[hash].next->prev = b;
    bcache.hashbucket[hash].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hash = get_hash(blockno);

  acquire(&bcache.lock[hash]);

  // Is the block already cached?
  for(b = bcache.hashbucket[hash].next; b != &bcache.hashbucket[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 当bget()查找数据块未命中时，bget()可从其他哈希桶选择一个未被使用的缓存块，移入到当前的哈希桶链表中使用。
  // next_hash表示下一个要探索的bucket，当它重新变成hash，说明所有的buffer都busy，此时产生panic。
  int next_hash = (hash + 1) % NBUCKETS;
  while (next_hash != hash) {
    // 查询其他哈希桶时也要注意上锁
    acquire(&bcache.lock[next_hash]);
    for(b = bcache.hashbucket[next_hash].prev; b != &bcache.hashbucket[next_hash]; b = b->prev){
      if(b->refcnt == 0){
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 从原先链表中移出b
        b->next->prev = b->prev;
        b->prev->next = b->next;
        // 将b插入新链表
        b->next = bcache.hashbucket[hash].next;
        b->prev = &bcache.hashbucket[hash];
        bcache.hashbucket[hash].next->prev = b;
        bcache.hashbucket[hash].next = b;

        release(&bcache.lock[next_hash]);
        release(&bcache.lock[hash]);
        acquiresleep(&b->lock);

        return b;
      }
    }
    // 若在哈希桶i中没有找到可用的缓存块，不要忘记解锁
    release(&bcache.lock[next_hash]);
    next_hash = (next_hash + 1) % NBUCKETS;
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid){
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int hash = get_hash(b->blockno);

  acquire(&bcache.lock[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.

    // 从链表中移出b
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // 将b插入链表头
    b->next = bcache.hashbucket[hash].next;
    b->prev = &bcache.hashbucket[hash];
    bcache.hashbucket[hash].next->prev = b;
    bcache.hashbucket[hash].next = b;
  }
  
  release(&bcache.lock[hash]);
}

void
bpin(struct buf *b) {
  int hash = get_hash(b->blockno);
  acquire(&bcache.lock[hash]);
  b->refcnt++;
  release(&bcache.lock[hash]);
}

void
bunpin(struct buf *b) {
  int hash = get_hash(b->blockno);
  acquire(&bcache.lock[hash]);
  b->refcnt--;
  release(&bcache.lock[hash]);
}


