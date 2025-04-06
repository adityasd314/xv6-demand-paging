  // Simple PIO-based (non-DMA) IDE driver code.

  #include "types.h"
  #include "defs.h"
  #include "param.h"
  #include "memlayout.h"
  #include "mmu.h"
  #include "proc.h"
  #include "x86.h"
  #include "traps.h"
  #include "spinlock.h"
  #include "sleeplock.h"
  #include "fs.h"
  #include "buf.h"

  #define SECTOR_SIZE 512
  #define IDE_BSY 0x80
  #define IDE_DRDY 0x40
  #define IDE_DF 0x20
  #define IDE_ERR 0x01

  #define IDE_CMD_READ 0x20
  #define IDE_CMD_WRITE 0x30
  #define IDE_CMD_RDMUL 0xc4
  #define IDE_CMD_WRMUL 0xc5

  // some macros for better readability
  #define DISK1_PORT_NO 0x1f0
  #define SWAPDISK_PORT_NO 0x170
  #define PRIMARY_IDE_CONTROL_PORT 0x3f6
  #define SECONDARY_IDE_CONTROL_PORT 0x376
  // idequeue points to the buf now being read/written to the disk.
  // idequeue->qnext points to the next buf to be processed.
  // You must hold idelock while manipulating queue.

  static struct spinlock idelock;
  static struct buf *idequeue;

  static int havedisk1;
  static int havedisk2;
  static void idestart(struct buf *);

  // Wait for IDE disk to become ready.
  static int
  idewait(int checkerr, int portno)
  {
    int r;

    while (((r = inb(portno + 7)) & (IDE_BSY | IDE_DRDY)) != IDE_DRDY)
      ;
    if (checkerr && (r & (IDE_DF | IDE_ERR)) != 0)
      return -1;
    return 0;
  }

  void ideinit(void)
  {
    int i;

    initlock(&idelock, "ide");
    ioapicenable(IRQ_IDE, ncpu - 1);
    ioapicenable(IRQ_IDE + 1, ncpu - 1);

    // Check if disk 1 is present
    idewait(0, DISK1_PORT_NO);
    outb(DISK1_PORT_NO + 6, 0xe0 | (1 << 4));
    for (i = 0; i < 1000; i++)
    {
      if (inb(DISK1_PORT_NO + 7) != 0)
      {
        havedisk1 = 1;
        break;
      }
    }
    // check if disk 2 is present
    outb(SWAPDISK_PORT_NO + 6, 0xe0 | (2 << 4));
    for (i = 0; i < 1000; i++)
    {
      if (inb(SWAPDISK_PORT_NO + 7) != 0)
      {
        cprintf("HERE IS MY SWAP DISK !");
        havedisk2 = 1;
        break;
      }
    }

    // Switch back to disk 0.
    outb(DISK1_PORT_NO + 6, 0xe0 | (0 << 4));
  }

  // Start the request for b.  Caller must hold idelock.
  static void
  idestart(struct buf *b)
  {
    if (b == 0)
      panic("idestart");
    if (b->blockno >= FSSIZE)
      panic("incorrect blockno");
    int portno;
    if (b->dev <= 1)
      portno = DISK1_PORT_NO;
    else
      portno = SWAPDISK_PORT_NO;
    int sector_per_block = BSIZE / SECTOR_SIZE;
    int sector = b->blockno * sector_per_block;
    int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ : IDE_CMD_RDMUL;
    int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

    if (sector_per_block > 7)
      panic("idestart");

    idewait(0, portno);
    if (b->dev <= 1)
      outb(PRIMARY_IDE_CONTROL_PORT, 0);
    else
      outb(SECONDARY_IDE_CONTROL_PORT, 0);
    outb(portno + 2, sector_per_block); // number of sectors
    outb(portno + 3, sector & 0xff);
    outb(portno + 4, (sector >> 8) & 0xff);
    outb(portno + 5, (sector >> 16) & 0xff);
    outb(portno + 6, 0xe0 | ((b->dev & 1) << 4) | ((sector >> 24) & 0x0f));
    if (b->flags & B_DIRTY)
    {
      outb(portno + 7, write_cmd);
      outsl(portno, b->data, BSIZE / 4);
    }
    else
    {
      outb(portno + 7, read_cmd);
    }
  }

  // Interrupt handler.
  void ideintr(int toggle_disk)
  {
    struct buf *b;
    int portno;
    // First queued buffer is the active request.
    acquire(&idelock);
    if (toggle_disk == 0)
    {
      portno = DISK1_PORT_NO;
    }
    else
    {
      portno = SWAPDISK_PORT_NO;
    }
    if ((b = idequeue) == 0)
    {
      release(&idelock);
      return;
    }
    idequeue = b->qnext;

    // Read data if needed.
    if (!(b->flags & B_DIRTY) && idewait(1, portno) >= 0)
      insl(DISK1_PORT_NO, b->data, BSIZE / 4);

    // Wake process waiting for this buf.
    b->flags |= B_VALID;
    b->flags &= ~B_DIRTY;
    wakeup(b);

    // Start disk on next buf in queue.
    if (idequeue != 0)
      idestart(idequeue);

    release(&idelock);
  }

  // PAGEBREAK!
  //  Sync buf with disk.
  //  If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
  //  Else if B_VALID is not set, read buf from disk, set B_VALID.
  void iderw(struct buf *b)
  {
    struct buf **pp;

    if (!holdingsleep(&b->lock))
      panic("iderw: buf not locked");
    if ((b->flags & (B_VALID | B_DIRTY)) == B_VALID)
      panic("iderw: nothing to do");
    if (b->dev != 0 && !havedisk1)
      panic("iderw: ide disk 1 not present");
    if (b->dev != 0 && !havedisk2)
      panic("iderw: ide disk 2 not present");

    acquire(&idelock); // DOC:acquire-lock

    // Append b to idequeue.
    b->qnext = 0;
    for (pp = &idequeue; *pp; pp = &(*pp)->qnext) // DOC:insert-queue
      ;
    *pp = b;

    // Start disk if necessary.
    if (idequeue == b)
      idestart(b);

    // Wait for request to finish.
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID)
    {
      sleep(b, &idelock);
    }

    release(&idelock);
  }
