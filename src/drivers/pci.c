#include "drivers/pci.h"
#include "drivers/serial.h"
#include "lib/io.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Static storage for discovered PCI devices */
static pci_device_t s_pci_devices[PCI_MAX_DEVICES];
static uint32_t     s_pci_device_count = 0;

/*
 * pci_make_address:
 * Computes the 32-bit configuration address value for port 0xCF8.
 *
 * Bit layout:
 * - Bit 31: Enable Configuration Space Translation (1)
 * - Bits 30-24: Reserved (0)
 * - Bits 23-16: Bus Number (0-255)
 * - Bits 15-11: Device / Slot Number (0-31)
 * - Bits 10-8:  Function Number (0-7)
 * - Bits 7-0:   Register Offset (bits 1:0 must be 0 for dword alignment)
 */
static inline uint32_t pci_make_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(0x80000000 |
                     ((uint32_t)bus << 16) |
                     ((uint32_t)(slot & 0x1F) << 11) |
                     ((uint32_t)(func & 0x07) << 8) |
                     (offset & 0xFC));
}

/*
 * pci_read_config_dword:
 * Reads a 32-bit double-word from the device configuration space.
 */
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS_PORT, addr);
    return inl(PCI_CONFIG_DATA_PORT);
}

/*
 * pci_read_config_word:
 * Reads a 16-bit word from the device configuration space.
 */
uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS_PORT, addr);
    /* Read 32-bit value and extract the target 16-bit word */
    uint32_t dword = inl(PCI_CONFIG_DATA_PORT);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

/*
 * pci_read_config_byte:
 * Reads an 8-bit byte from the device configuration space.
 */
uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS_PORT, addr);
    /* Read 32-bit value and extract the target 8-bit byte */
    uint32_t dword = inl(PCI_CONFIG_DATA_PORT);
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

/*
 * pci_write_config_dword:
 * Writes a 32-bit double-word to the device configuration space.
 */
void pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS_PORT, addr);
    outl(PCI_CONFIG_DATA_PORT, val);
}

/*
 * pci_write_config_word:
 * Writes a 16-bit word to the device configuration space.
 */
void pci_write_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t addr = pci_make_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS_PORT, addr);
    outw(PCI_CONFIG_DATA_PORT + (offset & 2), val);
}

/*
 * pci_probe_bar:
 * Determines the type, base address, and size of a Base Address Register (BAR).
 */
static void pci_probe_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_idx,
                          uint32_t* out_addr, pci_bar_type_t* out_type, uint32_t* out_size) {
    uint8_t offset = PCI_REG_BAR0 + (bar_idx * 4);
    uint32_t orig = pci_read_config_dword(bus, slot, func, offset);

    if (orig == 0 || orig == 0xFFFFFFFF) {
        *out_addr = 0;
        *out_type = PCI_BAR_TYPE_NONE;
        *out_size = 0;
        return;
    }

    if (orig & 0x01) {
        /* I/O Port BAR: bottom bit is 1, address is in bits 31..2 */
        *out_addr = orig & ~0x03;
        *out_type = PCI_BAR_TYPE_IO;

        /* Determine size by writing all 1s and reading back mask */
        pci_write_config_dword(bus, slot, func, offset, 0xFFFFFFFF);
        uint32_t mask = pci_read_config_dword(bus, slot, func, offset);
        pci_write_config_dword(bus, slot, func, offset, orig); /* Restore original */

        *out_size = (~(mask & ~0x03)) + 1;
    } else {
        /* Memory-Mapped I/O (MMIO) BAR: bottom bit is 0, address is 16-byte aligned */
        *out_addr = orig & ~0x0F;
        *out_type = PCI_BAR_TYPE_MMIO;

        /* Determine size by writing all 1s and reading back mask */
        pci_write_config_dword(bus, slot, func, offset, 0xFFFFFFFF);
        uint32_t mask = pci_read_config_dword(bus, slot, func, offset);
        pci_write_config_dword(bus, slot, func, offset, orig); /* Restore original */

        *out_size = (~(mask & ~0x0F)) + 1;
    }
}

/*
 * pci_probe_function:
 * Probes and records a single PCI function.
 */
static void pci_probe_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_read_config_word(bus, slot, func, PCI_REG_VENDOR_ID);
    if (vendor_id == 0xFFFF) {
        return; /* Device / function not present */
    }

    if (s_pci_device_count >= PCI_MAX_DEVICES) {
        return; /* Cache full */
    }

    pci_device_t* dev = &s_pci_devices[s_pci_device_count];
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus         = bus;
    dev->slot        = slot;
    dev->func        = func;
    dev->vendor_id   = vendor_id;
    dev->device_id   = pci_read_config_word(bus, slot, func, PCI_REG_DEVICE_ID);
    dev->revision_id = pci_read_config_byte(bus, slot, func, PCI_REG_REVISION_ID);
    dev->prog_if     = pci_read_config_byte(bus, slot, func, PCI_REG_PROG_IF);
    dev->subclass    = pci_read_config_byte(bus, slot, func, PCI_REG_SUBCLASS);
    dev->class_code  = pci_read_config_byte(bus, slot, func, PCI_REG_CLASS);
    dev->header_type = pci_read_config_byte(bus, slot, func, PCI_REG_HEADER_TYPE);
    dev->irq_line    = pci_read_config_byte(bus, slot, func, PCI_REG_INTERRUPT_LINE);
    dev->irq_pin     = pci_read_config_byte(bus, slot, func, PCI_REG_INTERRUPT_PIN);

    /* Header Type 0x00 has 6 BARs; Header Type 0x01 (Bridge) has 2 BARs */
    uint8_t num_bars = ((dev->header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) ? 2 : PCI_BAR_COUNT;
    for (uint8_t b = 0; b < num_bars; b++) {
        pci_probe_bar(bus, slot, func, b, &dev->bar[b], &dev->bar_type[b], &dev->bar_size[b]);
        /* If 64-bit MMIO BAR, the next BAR register contains the upper 32 bits */
        if (dev->bar_type[b] == PCI_BAR_TYPE_MMIO) {
            uint32_t raw_bar = pci_read_config_dword(bus, slot, func, PCI_REG_BAR0 + (b * 4));
            if ((raw_bar & 0x06) == 0x04 && b + 1 < num_bars) {
                b++; /* Skip high 32-bit BAR register in 32-bit kernel */
                dev->bar_type[b] = PCI_BAR_TYPE_NONE;
            }
        }
    }

    s_pci_device_count++;
}

/*
 * pci_init:
 * Scans all PCI buses (0-255), slots (0-31), and functions (0-7).
 */
void pci_init(void) {
    s_pci_device_count = 0;
    memset(s_pci_devices, 0, sizeof(s_pci_devices));

    serial_writestring("[PCI] Scanning PCI bus hierarchy...\n");

    /* Scan bus 0..255 */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor_id = pci_read_config_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (vendor_id == 0xFFFF) {
                continue; /* Slot is empty */
            }

            /* Probe function 0 */
            pci_probe_function((uint8_t)bus, slot, 0);

            /* Check bit 7 of Header Type to see if this is a multifunction device */
            uint8_t header_type = pci_read_config_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            if (header_type & PCI_HEADER_TYPE_MULTIFUNC) {
                for (uint8_t func = 1; func < 8; func++) {
                    pci_probe_function((uint8_t)bus, slot, func);
                }
            }
        }
    }

    kprintf("[PCI] Bus enumeration complete: %u device%s discovered\n",
            s_pci_device_count, (s_pci_device_count == 1) ? "" : "s");
    pci_print_devices();
}

/*
 * pci_get_device_count:
 * Returns the total count of discovered PCI devices.
 */
uint32_t pci_get_device_count(void) {
    return s_pci_device_count;
}

/*
 * pci_get_device:
 * Returns a pointer to the cached device at specified index.
 */
const pci_device_t* pci_get_device(uint32_t index) {
    if (index >= s_pci_device_count) {
        return NULL;
    }
    return &s_pci_devices[index];
}

/*
 * pci_find_device_by_class:
 * Finds the first detected device matching class and subclass code.
 */
pci_device_t* pci_find_device_by_class(uint8_t class_code, uint8_t subclass) {
    for (uint32_t i = 0; i < s_pci_device_count; i++) {
        if (s_pci_devices[i].class_code == class_code &&
            s_pci_devices[i].subclass == subclass) {
            return &s_pci_devices[i];
        }
    }
    return NULL;
}

/*
 * pci_find_device:
 * Finds the first detected device matching vendor ID and device ID.
 */
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < s_pci_device_count; i++) {
        if (s_pci_devices[i].vendor_id == vendor_id &&
            s_pci_devices[i].device_id == device_id) {
            return &s_pci_devices[i];
        }
    }
    return NULL;
}

/*
 * pci_enable_bus_master:
 * Enables Bus Mastering and Memory/IO access in the PCI Command register.
 * Necessary for hardware Direct Memory Access (DMA).
 */
void pci_enable_bus_master(const pci_device_t* dev) {
    if (!dev) return;
    uint16_t cmd = pci_read_config_word(dev->bus, dev->slot, dev->func, PCI_REG_COMMAND);
    cmd |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    pci_write_config_word(dev->bus, dev->slot, dev->func, PCI_REG_COMMAND, cmd);
}

/*
 * pci_class_to_string:
 * Translates class and subclass codes to human-readable names.
 */
const char* pci_class_to_string(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x00: return "Legacy Device";
        case 0x01:
            switch (subclass) {
                case 0x01: return "IDE Interface";
                case 0x06: return "SATA Controller (AHCI)";
                default:   return "Mass Storage Controller";
            }
        case 0x02:
            switch (subclass) {
                case 0x00: return "Ethernet Controller";
                default:   return "Network Controller";
            }
        case 0x03:
            switch (subclass) {
                case 0x00: return "VGA Display Controller";
                default:   return "Display Controller";
            }
        case 0x04:
            switch (subclass) {
                case 0x01: return "Audio Device (AC'97/SB16)";
                case 0x03: return "Intel HD Audio";
                default:   return "Multimedia Controller";
            }
        case 0x05: return "Memory Controller";
        case 0x06:
            switch (subclass) {
                case 0x00: return "Host / PCI Bridge";
                case 0x01: return "ISA Bridge";
                case 0x04: return "PCI-to-PCI Bridge";
                default:   return "Bridge Device";
            }
        case 0x07: return "Communication Controller";
        case 0x08: return "System Peripheral";
        case 0x09: return "Input Device";
        case 0x0C:
            switch (subclass) {
                case 0x03: return "USB Controller";
                default:   return "Serial Bus Controller";
            }
        default:   return "Unknown Device";
    }
}

/*
 * pci_vendor_to_string:
 * Returns recognizable company name for standard vendor IDs.
 */
const char* pci_vendor_to_string(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel Corp.";
        case 0x10EC: return "Realtek Semi.";
        case 0x1022: return "AMD Inc.";
        case 0x10DE: return "NVIDIA Corp.";
        case 0x1234: return "QEMU Virtual";
        case 0x80EE: return "VirtualBox Guest";
        default:     return "PCI Vendor";
    }
}

/*
 * pci_print_devices:
 * Logs all detected PCI devices to COM1 serial and kprintf.
 */
void pci_print_devices(void) {
    serial_writestring("\n============================= PCI DEVICE TREE =============================\n");
    serial_writestring("Bus:Dev.Fn  Vendor:Device  Class / Subclass           IRQ   BAR0\n");
    serial_writestring("---------------------------------------------------------------------------\n");

    for (uint32_t i = 0; i < s_pci_device_count; i++) {
        pci_device_t* d = &s_pci_devices[i];
        const char* class_str = pci_class_to_string(d->class_code, d->subclass);

        kprintf("[%02x:%02x.%u] %04x:%04x | %-24s | IRQ %u | BAR0: 0x%08x (%s)\n",
                (uint32_t)d->bus, (uint32_t)d->slot, (uint32_t)d->func,
                (uint32_t)d->vendor_id, (uint32_t)d->device_id,
                class_str ? class_str : "Unknown",
                (uint32_t)d->irq_line,
                d->bar[0],
                d->bar_type[0] == PCI_BAR_TYPE_IO ? "PIO" :
                d->bar_type[0] == PCI_BAR_TYPE_MMIO ? "MMIO" : "None");
    }
    serial_writestring("===========================================================================\n\n");
}
