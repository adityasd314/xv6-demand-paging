# xv6 Demand Paging Implementation

## Overview
This project extends the xv6 operating system to support **demand paging**, a memory management technique where pages are loaded into physical memory only when they are accessed. The implementation includes a **Least Recently Used (LRU) second-chance algorithm** for page eviction and a **swap space** managed via a bitmap, with page table entries (PTEs) storing swap location details, inspired by Linux's approach.

## Features
- **Pure Demand Paging**: Pages are loaded on-demand when a page fault occurs, rather than pre-loading the entire process.
- **LRU Second-Chance Eviction**: Uses the accessed bit in PTEs to track page usage. Pages with the accessed bit unset are evicted first; otherwise, the bit is reset to give the page a "second chance."
- **Swap Space Management**: A dedicated swap disk (`swap.img`) stores evicted pages. A bitmap tracks free and allocated swap pages, with each page occupying 8 disk blocks (4KB page / 512B block).
- **PTE-Based Swap Metadata**: Swap location indices are encoded in PTEs, with a specific bit marking pages as swapped.
- **Test Program**: A user program (`hello.c`) is included to test demand paging by accessing memory randomly, triggering page faults.

## Implementation Details
- **Page Fault Handling**: Handled in `trap.c` for `T_PGFLT`. The faulting address (from `CR2`) is checked, and `load_demand_page` is called to load the page from the executable file or swap space.
- **Demand Paging Logic** (`demand_paging.c`):
  - Checks if the faulting page is in swap (via PTE flags).
  - If in swap, loads the page using `load_swap_page`.
  - If not, loads from the executable file by reading the relevant program segment.
  - Maintains an LRU list per process, tracking up to `MAX_PAGES_PER_PROCESS` (default 4) pages.
- **Swap Management** (`swap.c`):
  - Uses a bitmap to manage 1024 swap pages (16 uints × 32 bits).
  - Functions: `bitmap_init`, `bitmap_get_free_page`, `bitmap_alloc_page`, `bitmap_dealloc_page`.
- **Disk I/O** (`bio.c`, `ide.c`):
  - Added `bread_swap` and `bwrite_swap` to read/write pages from/to the swap disk.
  - Modified `ide.c` to support a second disk (device ID 2) for swap, using a separate IDE port.
- **Process Management** (`proc.h`, `proc.c`):
  - Added `lru_list` to `struct proc` to track PTEs for LRU.
  - Calculates maximum pages based on file size, capped between `MIN_PAGES_PER_PROCESS` (2) and `MAX_PAGES_PER_PROCESS` (4).
- **Memory Management** (`vm.c`, `exec.c`):
  - Modified `allocuvm` and `loaduvm` to support demand paging (`newallocuvm`, `newloaduvm`).
  - PTEs are updated to mark pages as swapped or reset accessed bits.

## Building and Running
### Prerequisites
- A Unix-like environment (e.g., Linux, macOS with cross-compilation tools).
- `qemu-system-i386` for emulation.
- `gcc` and `make` for building.
- xv6 toolchain (e.g., `i386-jos-elf` for cross-compiling, if needed).

### Steps
1. **Clone the Repository**:
   ```bash
   git clone <repository-url>
   cd xv6-demand-paging
   ```

2. **Build the System**:
   ```bash
   make
   ```
   This creates:
   - `xv6.img`: The bootable kernel image.
   - `fs.img`: The filesystem image with user programs.
   - `swap.img`: The swap disk (4096 blocks).

3. **Run in QEMU**:
   ```bash
   make qemu
   ```
   This starts xv6 in QEMU with the filesystem and swap disk attached. The kernel will boot, and you can interact with the shell.

4. **Test Demand Paging**:
   - Run the `hello` program in the xv6 shell:
     ```bash
     hello
     ```
   - `hello` accesses a 32KB array randomly, triggering page faults to test demand paging and swapping.
   - Observe kernel logs (via `cprintf`) for page fault details, LRU list updates, and swap operations.

### Cleaning Up
To remove generated files:
```bash
make clean
```

## Debugging
- **Logs**: Extensive `cprintf` statements in `demand_paging.c`, `swap.c`, and `bio.c` provide insights into page faults, LRU eviction, and swap I/O.
- **GDB**: Use `make qemu-gdb` and connect with `gdb` to debug the kernel.
- **Page Table Inspection**: The `print_pte` function in `demand_paging.c` lists present pages and their accessed bits.

## Limitations
- Fixed swap size (1024 pages).
- Maximum of 4 pages per process (excluding guard/stack pages).
- No copy-on-write or shared memory support.
- Assumes swap disk is always present (`havedisk2` check in `ide.c`).

## Future Improvements
- Dynamic swap space allocation.
- Support for larger process memory with variable page limits.
- Enhanced eviction policies (e.g., clock algorithm with reference counts).
- Integration with copy-on-write for fork efficiency.

## Authors
- [Aditya Deshmukh]
- [COEP Technological University]

## License
This project is based on the MIT xv6 operating system and retains its licensing terms. See the original xv6 repository for details.
