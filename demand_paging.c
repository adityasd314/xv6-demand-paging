#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "memlayout.h"

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
  int found = 0;  // Flag to check if we found the right program header
  
  if((ip = namei(curproc->name)) == 0){
    end_op();
    cprintf("file not found\n");
    return -1;
  }
  ilock(ip);
  
  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    cprintf("readi elf header failed\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  
  if(elf.magic != ELF_MAGIC) {
    cprintf("bad elf magic\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  
  // Round down to page boundary
  dppgaddr = PGROUNDDOWN(dppgaddr);
  
  if(curproc->pgsallocated == curproc->maxpgs){
    pte_t *pte;
    uint a, pa;
    pte = walkpgdir(curproc->pgdir, (char*)0, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      char* pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
    else{
      *pte = 0;
    }
  }
  else{
    curproc->pgsallocated++;
    cprintf("pages allocated: %d\n", curproc->pgsallocated);
  }

  // Allocate memory for the page
  mem = kalloc();
  if(mem == 0){
    cprintf("allocuvm out of memory\n");
    iunlockput(ip);
    end_op();
    return -1;
  }
  memset(mem, 0, PGSIZE);
  
  // Find the program header that contains the faulting address
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph)) {
      cprintf("readi program header failed\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }
    
    if(ph.type != ELF_PROG_LOAD)
      continue;
      
    if(ph.memsz < ph.filesz) {
      cprintf("memsz < filesz\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }
    
    if(ph.vaddr + ph.memsz < ph.vaddr) {
      cprintf("vaddr overflow\n");
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }
    
    if(dppgaddr >= ph.vaddr && dppgaddr < ph.vaddr + ph.memsz){
      offset = ph.off + (dppgaddr - ph.vaddr);
      found = 1;
      break;
    }
  }
  
  if (!found) {
    cprintf("Address %x not found in any program segment\n", dppgaddr);
    kfree(mem);
    iunlockput(ip);
    end_op();
    return -1;
  }
  
  // Map the page into process address space
  if(mappages(curproc->pgdir, (char*)dppgaddr, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
    cprintf("mappages failed\n");
    kfree(mem);
    iunlockput(ip);
    end_op();
    return -1;
  }
  
  // Read from file into memory if within file size
  if(dppgaddr < ph.vaddr + ph.filesz) {
    uint bytes_to_read = PGSIZE;
    if(ph.vaddr + ph.filesz - dppgaddr < PGSIZE)
      bytes_to_read = ph.vaddr + ph.filesz - dppgaddr;
      
    if(readi(ip, mem, offset, bytes_to_read) != bytes_to_read) {
      cprintf("readi data failed, offset=%d, size=%d\n", offset, bytes_to_read);
      // On failure, unmap the page
      pte_t *pte = walkpgdir(curproc->pgdir, (void*)dppgaddr, 0);
      if(pte) *pte = 0;
      kfree(mem);
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
  else {
    cprintf("out of file size\n");
  }
  
  cprintf("Page loaded successfully at address %x\n", dppgaddr);
  iunlockput(ip);
  end_op();
  return 0;
}
