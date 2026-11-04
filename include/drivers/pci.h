#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include "lib/types.h"

/*
 * pci.h:
 * Peripheral Component Interconnect (PCI) Bus Interface.
 *
 * Why this is needed:
 * The PCI bus connects high-speed internal devices (Ethernet NICs, VGA adapters,
 * storage controllers, USB hosts) to the CPU. Software enumerates devices by querying
 * PCI Configuration Space via I/O Ports 0xCF8 (Address) and 0xCFC (Data).
 */

/* PCI Configuration Ports (Mechanism #1) */
#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT    0xCFC

/* Configuration Register Offsets */
#define PCI_REG_VENDOR_ID       0x00
#define PCI_REG_DEVICE_ID       0x02
#define PCI_REG_COMMAND         0x04
#define PCI_REG_STATUS          0x06
#define PCI_REG_REVISION_ID     0x08
#define PCI_REG_PROG_IF         0x09
#define PCI_REG_SUBCLASS        0x0A
#define PCI_REG_CLASS           0x0B
#define PCI_REG_CACHE_LINE_SIZE 0x0C
#define PCI_REG_LATENCY_TIMER   0x0D
#define PCI_REG_HEADER_TYPE     0x0E
#define PCI_REG_BIST            0x0F
#define PCI_REG_BAR0            0x10
#define PCI_REG_BAR1            0x14
#define PCI_REG_BAR2            0x18
#define PCI_REG_BAR3            0x1C
#define PCI_REG_BAR4            0x20
#define PCI_REG_BAR5            0x24
#define PCI_REG_INTERRUPT_LINE  0x3C
#define PCI_REG_INTERRUPT_PIN   0x3D

/* Command Register Bit Flags */
#define PCI_COMMAND_IO          (1 << 0) /* Enable response to I/O space accesses */
#define PCI_COMMAND_MEMORY      (1 << 1) /* Enable response to Memory space accesses */
#define PCI_COMMAND_BUS_MASTER  (1 << 2) /* Enable device to behave as bus master (DMA) */
#define PCI_COMMAND_INTERRUPT_DISABLE (1 << 10) /* Disable INTx assertion */

/* Header Types */
#define PCI_HEADER_TYPE_NORMAL  0x00
#define PCI_HEADER_TYPE_BRIDGE  0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02
#define PCI_HEADER_TYPE_MULTIFUNC 0x80

/* Maximum devices tracked in system */
#define PCI_MAX_DEVICES         64
#define PCI_BAR_COUNT           6

/* Base Address Register (BAR) Types */
typedef enum {
    PCI_BAR_TYPE_NONE = 0,
    PCI_BAR_TYPE_MMIO = 1,
    PCI_BAR_TYPE_IO   = 2
} pci_bar_type_t;

/* Discovered PCI Device Representation */
typedef struct {
    uint8_t        bus;
    uint8_t        slot;
    uint8_t        func;
    uint16_t       vendor_id;
    uint16_t       device_id;
    uint8_t        class_code;
    uint8_t        subclass;
    uint8_t        prog_if;
    uint8_t        revision_id;
    uint8_t        header_type;
    uint8_t        irq_line;
    uint8_t        irq_pin;

    uint32_t       bar[PCI_BAR_COUNT];
    pci_bar_type_t bar_type[PCI_BAR_COUNT];
    uint32_t       bar_size[PCI_BAR_COUNT];
} pci_device_t;

/* PCI Configuration Space Accessors */
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void     pci_write_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

/* PCI Subsystem Initialization & Enumeration */
void pci_init(void);

/* Device Query & Management */
uint32_t            pci_get_device_count(void);
const pci_device_t* pci_get_device(uint32_t index);
pci_device_t*       pci_find_device_by_class(uint8_t class_code, uint8_t subclass);
pci_device_t*       pci_find_device(uint16_t vendor_id, uint16_t device_id);
void                pci_enable_bus_master(const pci_device_t* dev);

/* Diagnostics & Formatting */
void        pci_print_devices(void);
const char* pci_class_to_string(uint8_t class_code, uint8_t subclass);
const char* pci_vendor_to_string(uint16_t vendor_id);

#endif /* DRIVERS_PCI_H */
