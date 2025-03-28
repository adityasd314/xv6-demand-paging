#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "memlayout.h"

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

int 
load_demand_page(uint dppgaddr)
{
  struct proc *curproc = myproc();

  begin_op();
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  char *mem;
  uint offset;
  int off, i;

  if((ip = namei(curproc->name)) == 0){
    end_op();
    cprintf("file not found\n");
    return -1;
  }
  ilock(ip);
  
  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    return -1;
  if(elf.magic != ELF_MAGIC)
    return -1;

  dppgaddr = PGROUNDDOWN(dppgaddr);

  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      return -1;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      return -1;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      return -1;
    if(dppgaddr >= ph.vaddr && dppgaddr < ph.vaddr + ph.memsz){
      offset = ph.off + (dppgaddr - ph.vaddr);
      break;
    }
  }

  mem = kalloc();
  if(mem == 0){
    cprintf("allocuvm out of memory\n");
    return 0;
  }
  memset(mem, 0, PGSIZE);
  pte_t *pte;
  if((pte = walkpgdir(curproc->pgdir, (void*)dppgaddr, 0)) == 0)
    return -1;
  *pte = (uint)mem | PTE_W | PTE_U | PTE_P;
  if(loaduvm(curproc->pgdir, (char*)dppgaddr, ip, offset, PGSIZE) < 0)
    return -1;

  cprintf("Page loaded\n");
  iunlockput(ip);
  end_op();
  return 0;
}
