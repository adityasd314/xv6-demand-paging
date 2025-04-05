#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "memlayout.h"
void print_pte()
{
  // prints the present pages
  struct proc *curproc = myproc();
  pte_t *pte = walkpgdir(curproc->pgdir, (void *)0, 0);
  int count = 0;
  uint p = 0;
  cprintf("\n===================PRESeNT : ");
  while (p < 1024)
  {
    if (*pte & PTE_P)
    {
      count++;
      cprintf("%x A:%d ", pte, PTE_GET_ACCESSED(*pte));
    }
    p++;
    pte = walkpgdir(curproc->pgdir, (void *)(p * 4096), 0);
    ;
  }
  cprintf("\nCOUNT: %d\n", count);
}
int load_demand_page(uint dppgaddr)
{
  struct proc *curproc = myproc();
  begin_op();
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  char *mem;
  uint offset;
  int off, i;
  int found = 0; // Flag to check if we found the right program header

  if ((ip = namei(curproc->name)) == 0)
  {
    end_op();
    cprintf("file not found\n");
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf))
  {
    cprintf("readi elf header failed\n");
    iunlockput(ip);
    end_op();
    return -1;
  }

  if (elf.magic != ELF_MAGIC)
  {
    cprintf("bad elf magic\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  // Round down to page boundary
  dppgaddr = PGROUNDDOWN(dppgaddr);
  cprintf("PAGE FAULT AT ADDR %d", dppgaddr);
  if (curproc->lru_list.sz == curproc->lru_list.max_sz)
  {
    // pte_t *pte;
    uint a, pa;
    int found = -1;
    uint addrFound = 0;
    print_pte();
    for (int i = 0; i < curproc->lru_list.sz; i++)
    {

      if (found == -1 && PTE_GET_ACCESSED(*curproc->lru_list.arr[i]) == 0)
      {
        found = i;
        addrFound = curproc->lru_list.arr[i];
      }
      if (found != -1 && i + 1 < curproc->lru_list.sz)
      {
        curproc->lru_list.arr[i] = curproc->lru_list.arr[i + 1];
      }
      *curproc->lru_list.arr[i] = PTE_RESET_ACCESSED(*curproc->lru_list.arr[i]);
    }
    curproc->lru_list.sz--;

    pte_t *pte = (found == -1) ? curproc->lru_list.arr[0] : addrFound;
    if (found == -1)
    {
      for (int i = 0; i < curproc->lru_list.sz; i++)
        curproc->lru_list.arr[i] = curproc->lru_list.arr[i + 1];
    }
    cprintf("\nFound %d Removing page table entry : %x\n", found, pte);
    if (!pte)
    {
      cprintf("Page not there !");
    }
    else
    {
      if ((*pte & PTE_P) != 0)
      {
        char *pa = PTE_ADDR(*pte);
        if (pa == 0)
          panic("kfree");
        char *v = P2V(pa);
        kfree(v);
        *pte = 0;
      }
      else
      {
        *pte = 0;
      }
      // curproc->lru_list.sz--;
      // cprintf("LIST : 0");
      // for (int i = 0; i < curproc->lru_list.sz; i++)
      // {
      //   curproc->lru_list.arr[i] = curproc->lru_list.arr[i + 1];
      //   cprintf("%x A:%d ", curproc->lru_list.arr[i + 1], PTE_GET_ACCESSED(curproc->lru_list.arr[i + 1]));
      // }
    }
  }
  // else{
  //   curproc->lru_list.sz++;
  //   cprintf("pages allocated: %d\n", curproc->lru_list.sz);
  // }

  // Allocate memory for the page
  mem = kalloc();
  if (mem == 0)
  {
    cprintf("allocuvm out of memory\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  memset(mem, 0, PGSIZE);

  // Find the program header that contains the faulting address
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
  {
    if (readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph))
    {
      cprintf("readi program header failed\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }

    if (ph.type != ELF_PROG_LOAD)
      continue;

    if (ph.memsz < ph.filesz)
    {
      cprintf("memsz < filesz\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }

    if (ph.vaddr + ph.memsz < ph.vaddr)
    {
      cprintf("vaddr overflow\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }

    if (dppgaddr >= ph.vaddr && dppgaddr < ph.vaddr + ph.memsz)
    {
      offset = ph.off + (dppgaddr - ph.vaddr);
      found = 1;
      break;
    }
  }

  if (!found)
  {
    cprintf("Address %x not found in any program segment\n", dppgaddr);
    kfree(mem);
    iunlockput(ip);
    end_op();
    return -1;
  }

  // Map the page into process address space
  if (mappages(curproc->pgdir, (char *)dppgaddr, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0)
  {
    cprintf("mappages failed\n");
    kfree(mem);
    iunlockput(ip);
    end_op();
    return -1;
  }

  // Read from file into memory if within file size
  if (dppgaddr < ph.vaddr + ph.filesz)
  {
    uint bytes_to_read = PGSIZE;
    if (ph.vaddr + ph.filesz - dppgaddr < PGSIZE)
      bytes_to_read = ph.vaddr + ph.filesz - dppgaddr;

    if (readi(ip, mem, offset, bytes_to_read) != bytes_to_read)
    {
      cprintf("readi data failed, offset=%d, size=%d\n", offset, bytes_to_read);
      // On failure, unmap the page
      pte_t *pte = walkpgdir(curproc->pgdir, (void *)dppgaddr, 0);
      if (pte)
        *pte = 0;
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
  else if (dppgaddr < ph.vaddr + ph.memsz)
  {
    // in BSS
    memset(mem, 0, PGSIZE);
  }
  else
  {
    cprintf("out of file size\n");
    return -1;
  }

  pte_t *pte = walkpgdir(curproc->pgdir, (void *)dppgaddr, 0);
  cprintf("Added into list index: %x MAX: %x PTE: %x\n", curproc->lru_list.sz, curproc->lru_list.max_sz, pte);
  curproc->lru_list.arr[curproc->lru_list.sz++] = pte;
  cprintf("Page loaded successfully at address %x\n", dppgaddr);
  for (int i = 0; i < curproc->lru_list.sz; i++)
  {

    cprintf(" %x A:%d", curproc->lru_list.arr[i], PTE_GET_ACCESSED(*curproc->lru_list.arr[i]));
  }
  cprintf("\n");

  iunlockput(ip);
  end_op();
  return 0;
}
