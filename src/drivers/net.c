#include "drivers/net.h"
#include "drivers/pci.h"
#include "drivers/e1000.h"
#include "drivers/rtl8139.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"

typedef enum {
    NET_DRIVER_NONE = 0,
    NET_DRIVER_E1000,
    NET_DRIVER_RTL8139
} net_driver_type_t;

static net_driver_type_t s_active_driver = NET_DRIVER_NONE;
static bool              s_net_active = false;
static const char*       s_driver_name = "None";
static uint8_t           s_null_mac[ETH_ALEN] = { 0 };
static net_stats_t       s_null_stats = { 0 };

/*
 * net_driver_init:
 * Auto-detects compatible network interface cards from the PCI bus,
 * initializes the optimal driver, registers interrupt handlers,
 * and sets up hardware packet TX/RX pipelines.
 */
void net_driver_init(void) {
    s_active_driver = NET_DRIVER_NONE;
    s_net_active    = false;
    s_driver_name   = "None";

    serial_writestring("[NET] Probing network interface hardware...\n");

    /* 1. Probe for Intel Gigabit NIC (Primary standard in VirtualBox) */
    pci_device_t* dev = pci_find_device(0x8086, 0x100E); /* 82540EM */
    if (!dev) {
        dev = pci_find_device(0x8086, 0x100F); /* 82545EM */
    }
    if (!dev) {
        dev = pci_find_device(0x8086, 0x1004); /* 82543GC */
    }
    if (!dev) {
        /* General Intel Ethernet controller scan */
        for (uint32_t i = 0; i < pci_get_device_count(); i++) {
            const pci_device_t* d = pci_get_device(i);
            if (d && d->vendor_id == 0x8086 && d->class_code == 0x02 && d->subclass == 0x00) {
                dev = (pci_device_t*)d;
                break;
            }
        }
    }

    if (dev) {
        kprintf("[NET] Discovered Intel Ethernet Controller [%04x:%04x] at %02x:%02x.%u\n",
                dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
        if (e1000_init(dev)) {
            s_active_driver = NET_DRIVER_E1000;
            s_net_active    = true;
            s_driver_name   = "Intel 82540EM Gigabit (e1000)";
            kprintf("[NET] Driver bound: %s\n", s_driver_name);
            return;
        }
    }

    /* 2. Probe for Realtek RTL8139 Fast Ethernet (Fallback) */
    dev = pci_find_device(0x10EC, 0x8139);
    if (dev) {
        kprintf("[NET] Discovered Realtek RTL8139 Controller [%04x:%04x] at %02x:%02x.%u\n",
                dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
        if (rtl8139_init(dev)) {
            s_active_driver = NET_DRIVER_RTL8139;
            s_net_active    = true;
            s_driver_name   = "Realtek RTL8139 Fast Ethernet";
            kprintf("[NET] Driver bound: %s\n", s_driver_name);
            return;
        }
    }

    /* 3. Check for any unsupported network controller */
    dev = pci_find_device_by_class(0x02, 0x00);
    if (dev) {
        kprintf("[NET] Notice: Unrecognized Network Controller [%04x:%04x] at %02x:%02x.%u\n",
                dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
    } else {
        serial_writestring("[NET] No PCI Ethernet network card detected on system.\n");
    }
}

/*
 * net_send_packet:
 * Dispatches packet transmission to the active hardware driver.
 */
bool net_send_packet(const void* data, size_t len) {
    if (!s_net_active) {
        return false;
    }

    switch (s_active_driver) {
        case NET_DRIVER_E1000:
            return e1000_send_packet(data, len);
        case NET_DRIVER_RTL8139:
            return rtl8139_send_packet(data, len);
        default:
            return false;
    }
}

/*
 * net_register_receive_callback:
 * Registers packet receive hook with the active hardware driver.
 */
void net_register_receive_callback(void (*rx_cb)(const void* data, size_t len)) {
    switch (s_active_driver) {
        case NET_DRIVER_E1000:
            e1000_register_rx_callback(rx_cb);
            break;
        case NET_DRIVER_RTL8139:
            rtl8139_register_rx_callback(rx_cb);
            break;
        default:
            break;
    }
}

/*
 * net_get_mac:
 * Returns the hardware MAC address of the active network card.
 */
const uint8_t* net_get_mac(void) {
    switch (s_active_driver) {
        case NET_DRIVER_E1000:
            return e1000_get_mac();
        case NET_DRIVER_RTL8139:
            return rtl8139_get_mac();
        default:
            return s_null_mac;
    }
}

/*
 * net_is_active:
 * Returns true if an Ethernet interface is operational.
 */
bool net_is_active(void) {
    return s_net_active;
}

/*
 * net_get_device_name:
 * Returns descriptive hardware model string.
 */
const char* net_get_device_name(void) {
    return s_driver_name;
}

/*
 * net_get_stats:
 * Returns interface throughput and error statistics.
 */
const net_stats_t* net_get_stats(void) {
    switch (s_active_driver) {
        case NET_DRIVER_E1000:
            return e1000_get_stats();
        case NET_DRIVER_RTL8139:
            return rtl8139_get_stats();
        default:
            return &s_null_stats;
    }
}
