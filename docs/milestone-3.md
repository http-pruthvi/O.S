# Milestone 3: Memory Management

## 1. Goal & Deliverables
The objective of Milestone 3 is to give the operating system full control over physical and virtual memory:
1. **Multiboot Memory Map Parser**: Traverses the BIOS physical memory map with stride safety (`entry->size + 4`), extracts 64-bit boundaries, and buffers regions into kernel memory before virtual memory is activated.
2. **Physical Memory Manager (PMM)**: Implements a 128 KB static bitmap in `.bss` tracking up to 4 GB of RAM (1,048,576 4 KB frames). Automatically reserves Frame 0, low memory (0 to 1 MB), the kernel image (rounded up to a page boundary), and bootloader structures.
3. **Two-Level 32-bit Paging**: Constructs a Page Directory and identity-maps all usable physical RAM (up to 256 MB) to ensure page-table frames allocated from anywhere in RAM can be written directly by the kernel. Leaves Page 0 (`0x00000000` to `0x00000FFF`) **unmapped** to catch NULL pointer dereferences.
4. **Page Fault Exception Handler**: Catches Vector 14 (`#PF`), reads the faulting linear address from `CR2`, decodes the hardware error code bits, and displays a dedicated panic screen.
5. **Dynamic Kernel Heap Allocator**: Manages a virtual heap at `0xD0000000` expanding on demand up to 64 MB. Uses a first-fit free list with 24-byte headers (`magic`, `size`, `is_free`, `padding`, `next`, `prev`), 8-byte payload alignment, block splitting, and bi-directional coalescing on `kfree`.
6. **Interactive Memory Test Hooks**:
   - `F1`: Divide-by-Zero exception
   - `F2`: Software interrupt (`int 0x80`)
   - `F3`: Print live PMM frame metrics and heap statistics
   - `F4`: 20-block heap stress and coalescing verification test
   - `F5`: 10-frame PMM allocation and deallocation test
   - `F6`: Trigger Page Fault via NULL pointer read (`0x0`)
   - `F7`: Trigger Page Fault via unmapped write (`0xDEADB000`)

---

## 2. Core Concepts Explained Simply

### 1. Physical vs. Virtual Memory
* **Physical Memory**: The actual hardware DRAM chips installed on the motherboard, indexed by physical electrical bus addresses starting at `0x00000000`.
* **Virtual Memory**: An artificial address space presented to the CPU once paging is enabled. Every instruction fetch and data read/write uses a virtual address, which the CPU's internal Memory Management Unit (MMU) translates into a physical address in hardware.

### 2. Frames vs. Pages
* A **Frame** is a physical 4096-byte (4 KB) chunk of actual hardware RAM managed by the Physical Memory Manager (PMM).
* A **Page** is a virtual 4096-byte (4 KB) block of linear address space mapped by the Paging subsystem to a physical frame.

### 3. Why a Bitmap for Physical Memory?
A bitmap allocator uses exactly 1 bit per 4 KB frame: `0` indicates the frame is free, and `1` indicates it is in use.
Tracking the full 4 GB 32-bit address space requires $4\text{ GB} / 4\text{ KB} = 1,048,576$ bits = $131,072$ bytes (128 KB).
By declaring this 128 KB array statically in `.bss`, the kernel eliminates the "chicken-and-egg" bootstrap problem: the PMM does not need dynamic memory allocation to initialize itself.

### 4. How a 32-bit Virtual Address is Translated
In x86 32-bit two-level paging, the CPU's MMU splits every 32-bit linear address into three distinct fields:

```text
31            22 21            12 11                     0
+---------------+---------------+-------------------------+
| Directory Idx |  Table Index  |       Byte Offset       |
|   (10 bits)   |   (10 bits)   |        (12 bits)        |
+---------------+---------------+-------------------------+
        │               │                     │
        │               │                     v
        │               v          +----------------------+
        │       +---------------+  | Physical 4 KB Frame  |
        │       |  Page Table   |─>| Base + Offset        |
        v       | (1024 entries)|  +----------------------+
+---------------+---------------+
| Page Directory|
| (1024 entries)|
+---------------+
```

1. **Bits 22–31 (10 bits, Directory Index)**: Indexes into the Page Directory pointed to by `CR3`, selecting 1 of 1024 Page Tables.
2. **Bits 12–21 (10 bits, Table Index)**: Indexes into the selected Page Table, selecting 1 of 1024 physical page frames.
3. **Bits 0–11 (12 bits, Byte Offset)**: Selects the exact byte within the 4096-byte physical frame ($2^{12} = 4096$).

### 5. Why All Usable RAM (up to 256 MB) Must Be Identity-Mapped
When paging is enabled, the CPU translates **every** memory access through page tables. When the heap or another subsystem calls `paging_map_page()`, it may need to allocate a frame from the PMM to create a brand-new Page Table. The PMM can hand out any free physical frame in RAM (e.g. at 45 MB or 120 MB). If that frame were not mapped, the CPU could not write or zero the new page table, triggering a fault. Identity-mapping all detected physical RAM (capped at 256 MB) ensures that any frame handed out by the PMM is immediately accessible at its physical address.

### 6. What the x86 Control Registers Do
* **`CR0`**: Master control register. Bit 31 (`PG`) is the Paging Enable bit. Setting `CR0.PG = 1` activates the MMU.
* **`CR2`**: Page Fault Linear Address Register. When a Page Fault (`#PF`) occurs, the CPU automatically stores the exact virtual address that caused the fault in `CR2`.
* **`CR3`**: Page Directory Base Register (PDBR). Holds the 32-bit physical base address of the active Page Directory. Loading `CR3` flushes non-global TLB entries.

---

## 3. Heap Block Header Layout (`heap_block_t`)

```c
typedef struct heap_block {
    uint32_t magic;            /* 4 bytes: 0x48454150 ("HEAP") */
    uint32_t size;             /* 4 bytes: usable payload size in bytes */
    uint32_t is_free;          /* 4 bytes: 1 if free, 0 if allocated */
    uint32_t padding;          /* 4 bytes: padding for 8-byte alignment */
    struct heap_block* next;   /* 4 bytes: pointer to next block */
    struct heap_block* prev;   /* 4 bytes: pointer to previous block */
} heap_block_t;
```

```text
Byte Offset:
 0                   4                   8                  12
+-------------------+-------------------+-------------------+-------------------+
|  magic (4 bytes)  |   size (4 bytes)  |  is_free (4 bytes)|  padding (4 bytes)|
+-------------------+-------------------+-------------------+-------------------+
 16                  20                  24 (8-byte aligned payload begins here)
+-------------------+-------------------+=======================================+
|  next* (4 bytes)  |  prev* (4 bytes)  | Usable Data Payload (multiple of 8)   |
+-------------------+-------------------+=======================================+
```

Total header size is exactly **24 bytes** ($24 \pmod 8 == 0$, verified by `_Static_assert`). Because the heap begins at `0xD0000000` (an 8-byte aligned address) and all allocated payloads are rounded up to multiples of 8, every returned pointer is guaranteed to be 8-byte aligned.

---

## 4. Files Created / Modified and Their Roles

| File | Role |
| :--- | :--- |
| [`include/mm/memmap.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/mm/memmap.h) / [`src/mm/memmap.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/mm/memmap.c) | Stride-safe (`entry->size + 4`) Multiboot memory map parser buffering regions in kernel memory before paging. |
| [`include/mm/pmm.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/mm/pmm.h) / [`src/mm/pmm.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/mm/pmm.c) | 128 KB static bitmap PMM tracking 4 KB physical frames, frame allocation, reservation, and double-free detection. |
| [`include/mm/paging.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/mm/paging.h) / [`src/mm/paging.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/mm/paging.c) | Two-level paging directory, identity-mapping all detected RAM (capping at 256 MB), unmapped Page 0, and mapping API. |
| [`src/mm/paging_asm.asm`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/mm/paging_asm.asm) | Assembly routines for loading `CR3`, setting `CR0.PG`, reading `CR2`, and invalidating TLB entries via `invlpg`. |
| [`include/mm/heap.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/mm/heap.h) / [`src/mm/heap.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/mm/heap.c) | First-fit kernel heap at `0xD0000000` expanding on demand to 64 MB, block splitting, and bi-directional coalescing. |
| [`include/drivers/keyboard.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/drivers/keyboard.h) / [`src/drivers/keyboard.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/drivers/keyboard.c) | Added make code decoding for test keys `F3` (`0x3D`) through `F7` (`0x41`). |
| [`src/arch/isr.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/arch/isr.c) | Routed Vector 14 directly to `page_fault_handler()`. |
| [`src/kernel/kernel.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/kernel/kernel.c) | Full init sequence (`memmap -> pmm -> paging -> heap -> sti`), test hooks `F3` through `F7`. |

---

## 5. Ten Likely Viva Questions & Answers

### Q1: What is the purpose of the Physical Memory Manager (PMM)?
**Answer**:
The PMM manages the machine's actual hardware RAM at the page-frame level (4096-byte granularity). It maintains a record of which physical frames are free, allocated, or reserved by the hardware/BIOS. It provides physical addresses to the paging system (when creating page tables) and kernel heap (when expanding virtual heap space).

### Q2: Why did we place the PMM bitmap in `.bss` instead of allocating it dynamically?
**Answer**:
Allocating the bitmap dynamically creates a circular dependency (chicken-and-egg problem): a heap allocator needs physical frames from the PMM to exist, but the PMM would need a heap allocator to allocate its bitmap. Sizing the bitmap statically at compile time (128 KB in `.bss` to track 4 GB) ensures the PMM can initialize immediately at boot with zero dynamic allocation prerequisites.

### Q3: Why is virtual Page 0 (`0x00000000` to `0x00000FFF`) explicitly left unmapped?
**Answer**:
Leaving virtual page 0 unmapped ensures that any attempt by kernel code to dereference a `NULL` pointer (address `0x0` or small structure member offsets like `0x00000004`) immediately triggers a hardware Page Fault (`#PF`). This instantly catches null-pointer bugs rather than silently reading or corrupting real-mode BIOS interrupt vectors located at physical address 0.

### Q4: Why must we identity-map all detected physical RAM before enabling paging?
**Answer**:
Once `CR0.PG` is enabled, the CPU cannot access physical memory directly; every address is translated. When `paging_map_page()` needs to allocate a frame from PMM for a new page table, that frame could be at any physical address (e.g. 50 MB or 110 MB). Identity-mapping all physical RAM ensures that any frame allocated by the PMM is directly readable and writable by the kernel at `virtual == physical`.

### Q5: What is the Translation Lookaside Buffer (TLB) and why do we use `invlpg`?
**Answer**:
The TLB is an on-chip associative cache inside the CPU that stores recent virtual-to-physical address translations to accelerate memory access. When the kernel modifies or unmaps a page table entry (`PTE`), the CPU may still retain the old translation in its TLB. The `invlpg` assembly instruction invalidates the cached translation for that specific virtual address, forcing the MMU to reload the new entry from RAM.

### Q6: How do we decode the Page Fault error code pushed by the CPU?
**Answer**:
The CPU pushes a hardware error code onto the stack for Vector 14:
- **Bit 0 (`P`)**: `0` = Fault was caused by a non-present page; `1` = Page was present, but violated read/write/user protection permissions.
- **Bit 1 (`W/R`)**: `0` = Access was a Read; `1` = Access was a Write.
- **Bit 2 (`U/S`)**: `0` = Processor was executing in Supervisor mode (Ring 0); `1` = User mode (Ring 3).
- **Bit 3 (`RSVD`)**: `1` = Fault was caused by reserved bits overwritten in the page directory/table entries.
- **Bit 4 (`I/D`)**: `1` = Fault occurred during an instruction fetch.

### Q7: What is the Multiboot memory map stride bug?
**Answer**:
The Multiboot 1 specification defines each memory map record with a 4-byte `size` member followed by the entry payload (`base`, `length`, `type`). Crucially, `entry->size` specifies the length of the record *excluding* the `size` variable itself. Advancing by `entry->size` results in an incorrect 4-byte stride, reading corrupt alignment data. The correct pointer advancement must add `entry->size + sizeof(uint32_t)`.

### Q8: How does heap coalescing prevent memory fragmentation?
**Answer**:
When memory blocks of varying sizes are allocated and freed, the heap can become fragmented into small, unusable slivers. In our allocator, each block header has `prev` and `next` pointers. When `kfree()` deallocates a block, it checks whether the adjacent neighbor blocks are also marked as free. If so, it merges the headers into a single contiguous free block, allowing subsequent larger allocations to succeed.

### Q9: Why is `_Static_assert` used on `sizeof(heap_block_t)`?
**Answer**:
`_Static_assert(sizeof(heap_block_t) % 8 == 0, ...)` performs a compile-time check ensuring the block header is an exact multiple of 8 bytes (24 bytes). Because the heap starts at an 8-byte aligned virtual address (`0xD0000000`), adding a 24-byte header guarantees that every user payload pointer returned by `kmalloc()` is 8-byte aligned, preventing unaligned memory access penalties on x86.

### Q10: What is the role of `CR2` during a Page Fault?
**Answer**:
When a page fault occurs, the CPU hardware automatically latches the 32-bit linear address that attempted the illegal access into the `CR2` control register. The `#PF` exception handler reads `CR2` via inline assembly (`mov eax, cr2`) to determine exactly which address triggered the fault and display it on the panic screen.
