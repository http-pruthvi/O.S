# Milestone 10: PCI Bus Enumeration & Network Interface Card (NIC) Drivers

## 1. Goal and What Was Built

Milestone 10 equips MyOS with hardware peripheral discovery and physical network connectivity. It introduces a comprehensive PCI configuration subsystem and high-performance drivers for the two most common network interface cards in virtualization and legacy x86 PCs:

1. **PCI Bus Enumeration Subsystem (`include/drivers/pci.h`, `src/drivers/pci.c`)**:
   - PCI Configuration Space access via standard **I/O Mechanism #1** using Port `0xCF8` (Address) and Port `0xCFC` (Data).
   - Functions for 8-bit, 16-bit, and 32-bit configuration register reads and writes.
   - Comprehensive bus scan across all 256 buses, 32 device slots, and 8 functions per slot, identifying multi-function devices via Header Type bit 7.
   - Base Address Register (BAR) probing: detects I/O Port vs. MMIO space and determines hardware address window sizes by writing all 1s (`0xFFFFFFFF`).
   - PCI device database caching up to 64 devices with human-readable vendor and class names (Host Bridge, Ethernet Controller, VGA Controller, IDE/SATA Controller, Audio Device, USB Host).
   - Device search helpers (`pci_find_device`, `pci_find_device_by_class`) and Bus Master DMA enablement (`pci_enable_bus_master`).

2. **Network Hardware Interface Abstraction (`include/drivers/net.h`, `src/drivers/net.c`)**:
   - Unified hardware network interface abstraction decoupling the physical network card from upper protocol layers (Milestone 11 TCP/IP stack).
   - Standard 6-byte Ethernet MAC representation and throughput statistics tracking (link state, TX/RX packet counters, TX/RX byte counters).
   - Unified packet transmission (`net_send_packet`) and receiver callback dispatch (`net_register_receive_callback`).
   - Auto-detection and binding engine (`net_driver_init`) that probes for an Intel E1000 first, falls back to Realtek RTL8139, and brings the link up.

3. **Intel 82540EM Gigabit NIC Driver (`include/drivers/e1000.h`, `src/drivers/e1000.c`)**:
   - High-speed Gigabit driver for Intel 82540EM (Vendor `0x8086`, Device `0x100E`), standard in VirtualBox and QEMU.
   - Memory-Mapped I/O (MMIO): maps the 128 KB BAR0 physical register window directly into virtual page tables with Cache Disable (`PAGE_PCD`).
   - Dual MAC acquisition: reads 16-bit words from EEPROM via `EERD` register or falls back to Receive Address Low/High (`RAL0`/`RAH0`).
   - Direct Memory Access (DMA) Descriptor Rings:
     - 32-entry Receive (RX) descriptor ring with page-aligned 2048-byte physical buffers.
     - 16-entry Transmit (TX) descriptor ring with automatic FCS/CRC insertion and status reporting.
   - Hardware Receive Control (`RCTL`) and Transmit Control (`TCTL`) with IPG (Inter-Packet Gap) configuration.
   - Hardware interrupt handling via PIC IRQ with Interrupt Cause Read (`ICR`) acknowledgement and link status tracking.

4. **Realtek RTL8139 Fast Ethernet NIC Driver (`include/drivers/rtl8139.h`, `src/drivers/rtl8139.c`)**:
   - Battle-tested 10/100 Mbps driver for Realtek RTL8139 (Vendor `0x10EC`, Device `0x8139`).
   - Port I/O register control via BAR0/BAR1 I/O space.
   - Software chip reset, power management (`CONFIG1`), and MAC reading from `IDR0..IDR5`.
   - Circular 8 KB + 16B + 1.5 KB wrap margin receive buffer (`RBSTART`) with hardware packet status checking.
   - 4-slot round-robin DMA transmit pipeline (`TSAD0..3` and `TSD0..3`).

5. **Terminal CLI Integration (`src/apps/terminal.c`)**:
   - `lspci`: Formatted table displaying all enumerated PCI devices (`Bus:Dev.Func | Vendor:Device | Class/Type | IRQ | BAR0`).
   - `ifconfig` / `netstat`: Displays active network interface details (Device model, MAC HWaddr, Link state, RX/TX packets and bytes).

---

## 2. Architecture & How It Works

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Applications & Desktop                            │
│     Terminal CLI ('lspci', 'ifconfig')  │  Web App Store / Network Apps     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                  TCP/IP Protocol Stack (Milestone 11)                       │
│     Ethernet II Frame Handler  │  ARP  │  IPv4  │  ICMP  │  UDP  │  TCP     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                 Network Hardware Interface (drivers/net.h)                  │
│   - net_send_packet(data, len)          - net_register_receive_callback()   │
│   - net_get_mac()                       - net_is_active()                   │
└──────────────────────┬──────────────────────────────────────┬───────────────┘
                       │                                      │
                       ▼                                      ▼
       ┌───────────────────────────────┐      ┌───────────────────────────────┐
       │   Intel E1000 Gigabit Driver  │      │   Realtek RTL8139 Driver      │
       │   (Vendor: 0x8086 Dev: 0x100E)│      │   (Vendor: 0x10EC Dev: 0x8139)│
       ├───────────────────────────────┤      ├───────────────────────────────┤
       │ - MMIO BAR0 (128 KB Window)   │      │ - Port I/O Base (0xC000)      │
       │ - 32 RX Descriptors (DMA)     │      │ - 8 KB + 1.5 KB Circular Ring │
       │ - 16 TX Descriptors (DMA)     │      │ - 4 Round-Robin TX Slots      │
       │ - PIC IRQ Interrupt Service   │      │ - PIC IRQ Interrupt Service   │
       └───────────────┬───────────────┘      └───────────────┬───────────────┘
                       │                                      │
┌──────────────────────▼──────────────────────────────────────▼───────────────┐
│                       PCI Bus Subsystem (drivers/pci.h)                     │
│   - Configuration Mechanism #1: Port 0xCF8 (Address), Port 0xCFC (Data)     │
│   - Full Bus Scan (Busses 0-255, Slots 0-31, Funcs 0-7)                     │
│   - BAR Probing (I/O vs MMIO, Sizing via 0xFFFFFFFF Masking)                │
│   - Bus Master DMA Enablement (Command Register Bit 2)                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

### PCI Configuration Mechanism #1 Detail

The x86 architecture accesses PCI configuration space through two 32-bit I/O ports:
1. **Port `0xCF8` (`PCI_CONFIG_ADDRESS_PORT`)**: Specifies which bus, device, function, and register to access:
   - **Bit 31**: Enable bit (must be `1` to translate accesses).
   - **Bits 23-16**: Bus Number (`0 - 255`).
   - **Bits 15-11**: Device / Slot Number (`0 - 31`).
   - **Bits 10-8**: Function Number (`0 - 7`).
   - **Bits 7-2**: Register Offset (`0x00 - 0xFC`, 4-byte aligned).
   - **Bits 1-0**: Always `00`.
2. **Port `0xCFC` (`PCI_CONFIG_DATA_PORT`)**: Reads or writes the 32-bit data register targeted by port `0xCF8`.

### BAR Sizing Algorithm

To determine how much memory or I/O space a peripheral requires without consulting external datasheets:
1. Save the existing BAR value: `orig = read_bar()`.
2. Write all 1s: `write_bar(0xFFFFFFFF)`.
3. Read back the mask: `mask = read_bar()`.
4. Restore original: `write_bar(orig)`.
5. Compute size:
   - For I/O Space (`orig & 1`): `size = (~(mask & ~0x03)) + 1`.
   - For MMIO Space (`(orig & 1) == 0`): `size = (~(mask & ~0x0F)) + 1`.

---

## 3. Key Concepts Explained Simply

- **PCI Configuration Space**: A standardized 256-byte parameter block implemented by every PCI peripheral. It tells the operating system who made the device (Vendor ID), what model it is (Device ID), how to talk to it (BAR registers), and what interrupt line it is wired to.
- **Base Address Register (BAR)**: Registers in PCI configuration space that tell the CPU where the device's control registers reside. Bit 0 indicates whether the register addresses are mapped to I/O ports (`in`/`out` instructions) or physical RAM addresses (MMIO, dereferenced as pointers).
- **Memory-Mapped I/O (MMIO)**: Hardware device registers that are accessed by reading and writing to regular physical memory addresses. In paging mode, these physical addresses must be mapped into the CPU's page tables with Cache-Disable (`PCD`) so the CPU does not cache hardware register reads.
- **Bus Mastering (DMA)**: A feature enabled via Bit 2 of the PCI Command Register allowing the network card to initiate Direct Memory Access, copying received packet buffers straight into system RAM without loading the CPU with byte-by-byte port I/O.
- **Descriptor Ring**: A circular queue of metadata structures shared between the CPU and the network controller. Each descriptor points to a physical memory buffer and contains status flags (e.g., `DD` Descriptor Done) and packet length. Hardware advances the Head pointer while software advances the Tail pointer.

---

## 4. Files Created / Changed and Their Roles

| File | Role |
| :--- | :--- |
| `include/drivers/pci.h` | Header defining PCI register offsets, `pci_device_t` structure, BAR types, and configuration API. |
| `src/drivers/pci.c` | Implementation of PCI Mechanism #1 read/write, bus hierarchy scan, BAR probing, and class formatting. |
| `include/drivers/net.h` | Generic network hardware abstraction interface (`net_send_packet`, `net_register_receive_callback`, stats). |
| `src/drivers/net.c` | Network adapter manager: auto-detects E1000/RTL8139, binds driver ops, and manages interface statistics. |
| `include/drivers/e1000.h` | Register offsets, descriptor formats (`e1000_rx_desc_t`, `e1000_tx_desc_t`), and Intel E1000 interface. |
| `src/drivers/e1000.c` | Complete Intel 82540EM Gigabit NIC driver: MMIO mapping, EEPROM MAC, DMA rings, and IRQ handler. |
| `include/drivers/rtl8139.h`| Register offsets and interface for Realtek RTL8139 Fast Ethernet driver. |
| `src/drivers/rtl8139.c` | Realtek RTL8139 driver: Port I/O control, circular receive buffer, 4-slot TX, and IRQ handler. |
| `include/lib/io.h` | Extended hardware port I/O helpers with `outw`, `inw`, `outl`, and `inl`. |
| `src/apps/terminal.c` | Added `lspci` and `ifconfig` commands with column formatting and hex conversion. |
| `src/kernel/kernel.c` | Boot initialization: calls `pci_init()` and `net_driver_init()` during kernel startup. |

---

## 5. How to Build, Deploy and Test

### Compilation & ISO Generation
```bash
# Clean and build the kernel binary
make clean && make build

# Generate bootable ISO
make iso

# Deploy to VirtualBox staging folder (C:\os\myos.iso)
make deploy
```

### Expected Output in `serial.log`
```text
[PCI] Scanning PCI bus hierarchy...
[PCI] Bus enumeration complete: 6 devices discovered

============================= PCI DEVICE TREE =============================
Bus:Dev.Fn  Vendor:Device  Class / Subclass           IRQ   BAR0
---------------------------------------------------------------------------
[00:00.0] 8086:1237 | Host / PCI Bridge        | IRQ  0 | BAR0: 0x00000000 (None)
[00:01.0] 8086:7000 | ISA Bridge               | IRQ  0 | BAR0: 0x00000000 (None)
[00:01.1] 8086:7111 | IDE Interface            | IRQ 14 | BAR0: 0x000001f0 (PIO)
[00:02.0] 80ee:beef | VGA Display Controller   | IRQ  0 | BAR0: 0xe0000000 (MMIO)
[00:03.0] 8086:100e | Ethernet Controller      | IRQ 10 | BAR0: 0xf0000000 (MMIO)
[00:04.0] 80ee:cafe | System Peripheral        | IRQ  9 | BAR0: 0x0000d000 (PIO)
===========================================================================

[OK] PCI Bus Subsystem initialized (6 devices discovered)
[NET] Probing network interface hardware...
[NET] Discovered Intel Ethernet Controller [8086:100e] at 00:03.0
[E1000] Initializing Intel Gigabit NIC (82540EM)...
[E1000] Hardware MAC: 08:00:27:4d:5d:23
[E1000] Registered IRQ 10
[E1000] Initial link state: LINK UP (Connected) (STATUS: 0x00080383)
[NET] Driver bound: Intel 82540EM Gigabit (e1000)
[OK] Network Interface active: Intel 82540EM Gigabit (e1000) (MAC: 08:00:27:4d:5d:23)
```

### Expected Output in Desktop Terminal Shell (`lspci`)
```text
myos:/$ lspci
B:D.F   Vendor:Dev  Class / Type           IRQ  BAR0
----------------------------------------------------------
00:00.0 8086:1237   Host / PCI Bridge        0  none
00:01.0 8086:7000   ISA Bridge               0  none
00:01.1 8086:7111   IDE Interface           14  0x000001f0
00:02.0 80ee:beef   VGA Display Controller   0  0xe0000000
00:03.0 8086:100e   Ethernet Controller     10  0xf0000000
00:04.0 80ee:cafe   System Peripheral        9  0x0000d000

myos:/$ ifconfig
eth0 (Link UP):
  Device:  Intel 82540EM Gigabit (e1000)
  HWaddr:  08:00:27:4d:5d:23
  RX:      0 packets, 0 bytes
  TX:      0 packets, 0 bytes
```

---

## 6. 5 Likely Viva Questions & Answers

### Q1: How does the CPU access PCI Configuration Space on x86, and what is Configuration Mechanism #1?
**Answer:** On x86, PCI configuration space is accessed using two 32-bit I/O ports: `0xCF8` (Configuration Address Port) and `0xCFC` (Configuration Data Port). To read or write a device register, software formats a 32-bit address containing an Enable bit (Bit 31), Bus (Bits 23-16), Device/Slot (Bits 15-11), Function (Bits 10-8), and Register Offset (Bits 7-2), writes it to port `0xCF8`, and then reads from or writes to port `0xCFC`.

### Q2: How does the kernel determine the memory size required by a Base Address Register (BAR)?
**Answer:** The kernel reads and saves the original BAR value, writes all binary 1s (`0xFFFFFFFF`) to the BAR register, and reads back the resulting value. Per the PCI specification, hardware zeroes out all writable base address bits and preserves only the hardwired size bits. The size is calculated by masking out the control bits, inverting the mask with bitwise NOT (`~`), and adding 1. The original BAR value is then restored.

### Q3: What is the difference between an I/O BAR and an MMIO BAR?
**Answer:** An I/O BAR (identified by bit 0 being `1`) specifies a port number in the CPU's 64 KB isolated I/O address space accessed using `in` and `out` assembly instructions. An MMIO BAR (identified by bit 0 being `0`) specifies a physical memory address in the CPU's regular address space. Under paging, MMIO addresses must be mapped into page tables with the Cache-Disable (`PCD`) bit set so the CPU accesses the hardware registers directly instead of caching them in L1/L2 cache.

### Q4: How does packet transmission work in the Intel E1000 NIC driver?
**Answer:** The E1000 uses a circular transmit descriptor ring allocated in physical memory. When the OS sends a packet, it copies the data to an identity-mapped physical buffer, stores the physical buffer address in the current descriptor, sets the length, flags the descriptor with `EOP` (End of Packet), `IFCS` (Insert Frame Check Sequence/CRC), and `RS` (Report Status), and advances the Transmit Descriptor Tail (`TDT`) register. The E1000 hardware DMAs the packet directly from RAM onto the wire.

### Q5: Why is Bit 2 of the PCI Command Register critical for network drivers?
**Answer:** Bit 2 is the **Bus Master** enable bit. PCI network cards require Bus Mastering to act as DMA masters, allowing them to autonomously read transmit packet buffers and write received Ethernet frames directly to system RAM without CPU intervention. If Bit 2 is not set, DMA transfers fail and the card cannot send or receive frames.
