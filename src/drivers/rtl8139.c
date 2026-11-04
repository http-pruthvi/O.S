#include "drivers/rtl8139.h"
#include "drivers/serial.h"
#include "arch/irq.h"
#include "arch/pic.h"
#include "lib/io.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* RX Ring buffer: 8 KB + 16 bytes header + 1536 bytes wrap margin + 16 bytes alignment */
#define RTL_RX_BUF_SIZE     (8192 + 16 + 1536 + 16)
#define RTL_NUM_TX_DESC     4

static uint16_t           s_iobase = 0;
static uint8_t            s_mac[ETH_ALEN] = { 0 };
static net_stats_t        s_stats = { 0 };
static net_rx_callback_t  s_rx_cb = NULL;

/* Statically allocated, page-aligned buffers (identity mapped in RAM) */
static uint8_t s_rx_buffer[RTL_RX_BUF_SIZE] __attribute__((aligned(4096)));
static uint8_t s_tx_buffers[RTL_NUM_TX_DESC][ETH_MAX_BUF] __attribute__((aligned(4096)));

static uint16_t s_rx_read_offset = 0;
static uint8_t  s_tx_cur = 0;

/*
 * rtl8139_poll_rx:
 * Processes incoming packets received in the RTL8139 ring buffer.
 */
static void rtl8139_poll_rx(void) {
    while ((inb(s_iobase + RTL_REG_CMD) & 0x01) == 0) { /* While Buffer Not Empty (BUFE == 0) */
        uint8_t* rx_ptr = &s_rx_buffer[s_rx_read_offset];
        uint16_t status = *(uint16_t*)(rx_ptr);
        uint16_t length = *(uint16_t*)(rx_ptr + 2);

        /* Check for Packet OK (Bit 0) */
        if (!(status & 0x01) || length == 0) {
            break;
        }

        /* Length includes 4-byte CRC at the end */
        uint16_t payload_len = (length > 4) ? (length - 4) : 0;
        if (s_rx_cb && payload_len > 0) {
            s_rx_cb(rx_ptr + 4, payload_len);
        }

        s_stats.packets_rx++;
        s_stats.bytes_rx += payload_len;

        /* Advance read offset: 4-byte header + length, dword aligned */
        s_rx_read_offset = (uint16_t)((s_rx_read_offset + length + 4 + 3) & ~3);
        s_rx_read_offset %= 8192;

        /* RTL8139 hardware quirk: CAPR register must be programmed with offset - 0x10 */
        outw(s_iobase + RTL_REG_CAPR, (uint16_t)(s_rx_read_offset - 0x10));
    }
}

/*
 * rtl8139_irq_handler:
 * Hardware interrupt service routine for RTL8139 NIC.
 */
static void rtl8139_irq_handler(registers_t* regs) {
    (void)regs;
    uint16_t status = inw(s_iobase + RTL_REG_ISR);
    if (status == 0) {
        return; /* Not our interrupt */
    }

    /* Acknowledge interrupt flags by writing 1s back */
    outw(s_iobase + RTL_REG_ISR, status);

    /* Bit 0: Receive (Rx) OK */
    if (status & 0x0001) {
        rtl8139_poll_rx();
    }

    /* Bit 2: Transmit (Tx) OK */
    if (status & 0x0004) {
        /* Transmit completed */
    }

    /* Bit 5: Cable Length Change / Link Change */
    if (status & 0x0020) {
        s_stats.is_link_up = true;
    }
}

/*
 * rtl8139_init:
 * Initializes Realtek RTL8139 NIC.
 */
bool rtl8139_init(const pci_device_t* dev) {
    if (!dev) return false;

    /* Detect I/O Base Port from BAR0 or BAR1 */
    uint16_t iobase = 0;
    if (dev->bar_type[0] == PCI_BAR_TYPE_IO) {
        iobase = (uint16_t)dev->bar[0];
    } else if (dev->bar_type[1] == PCI_BAR_TYPE_IO) {
        iobase = (uint16_t)dev->bar[1];
    } else {
        serial_writestring("[RTL8139] Error: No I/O BAR detected!\n");
        return false;
    }

    s_iobase = iobase;
    serial_writestring("[RTL8139] Initializing Realtek RTL8139 Fast Ethernet...\n");

    /* Step 1: Enable PCI Bus Mastering */
    pci_enable_bus_master(dev);

    /* Step 2: Power on chip (Config 1 = 0x00) */
    outb(s_iobase + RTL_REG_CONFIG1, 0x00);

    /* Step 3: Software Reset (Command Register Bit 4 = 1) */
    outb(s_iobase + RTL_REG_CMD, 0x10);
    while ((inb(s_iobase + RTL_REG_CMD) & 0x10) != 0) {
        /* Wait for reset to finish */
    }

    /* Step 4: Read 6-byte MAC address from IDR registers (0x00..0x05) */
    for (int i = 0; i < ETH_ALEN; i++) {
        s_mac[i] = inb(s_iobase + RTL_REG_MAC0 + i);
    }
    kprintf("[RTL8139] Hardware MAC: %02x:%02x:%02x:%02x:%02x:%02x (I/O Port: 0x%04x)\n",
            s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5], s_iobase);

    /* Step 5: Initialize RX buffer */
    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_rx_read_offset = 0;
    outl(s_iobase + RTL_REG_RBSTART, (uint32_t)(uintptr_t)s_rx_buffer);

    /* Step 6: Configure Interrupt Mask Register (IMR) */
    /* Bit 0: ROK (Rx OK), Bit 2: TOK (Tx OK), Bit 5: Link change */
    outw(s_iobase + RTL_REG_IMR, 0x0005 | 0x0020);

    /* Step 7: Configure Receive Configuration Register (RCR) */
    /*
     * AAP: Accept All Packets (Bit 0)
     * APM: Accept Physical Match (Bit 1)
     * AM:  Accept Multicast (Bit 2)
     * AB:  Accept Broadcast (Bit 3)
     * WRAP: Wrap buffer to beginning (Bit 7)
     */
    outl(s_iobase + RTL_REG_RCR, 0x0F | (1 << 7));

    /* Step 8: Enable Receiver and Transmitter (RE | TE in CMD) */
    outb(s_iobase + RTL_REG_CMD, 0x0C);

    /* Step 9: Register IRQ handler & unmask PIC */
    if (dev->irq_line < 16) {
        irq_register_handler(dev->irq_line, rtl8139_irq_handler);
        pic_unmask_irq(dev->irq_line);
        kprintf("[RTL8139] Registered IRQ %u\n", dev->irq_line);
    }

    s_stats.is_link_up = true;
    s_tx_cur = 0;

    return true;
}

/*
 * rtl8139_send_packet:
 * Sends an Ethernet frame using one of the 4 round-robin TX slots.
 */
bool rtl8139_send_packet(const void* data, size_t len) {
    if (!data || len == 0 || len > ETH_FRAME_LEN || s_iobase == 0) {
        return false;
    }

    uint8_t cur = s_tx_cur;

    /* Copy packet data into aligned TX buffer */
    memcpy(s_tx_buffers[cur], data, len);

    /* Set buffer physical address in TSAD */
    outl(s_iobase + RTL_REG_TSAD0 + (cur * 4), (uint32_t)(uintptr_t)s_tx_buffers[cur]);

    /* Write size to TSD and clear OWN bit to initiate DMA transmission */
    outl(s_iobase + RTL_REG_TSD0 + (cur * 4), (uint32_t)len & 0x1FFF);

    s_tx_cur = (s_tx_cur + 1) % RTL_NUM_TX_DESC;
    s_stats.packets_tx++;
    s_stats.bytes_tx += len;

    return true;
}

/*
 * rtl8139_register_rx_callback:
 * Registers packet receiver callback.
 */
void rtl8139_register_rx_callback(net_rx_callback_t cb) {
    s_rx_cb = cb;
}

/*
 * rtl8139_get_mac:
 * Returns the hardware MAC address.
 */
const uint8_t* rtl8139_get_mac(void) {
    return s_mac;
}

/*
 * rtl8139_get_stats:
 * Returns network statistics for RTL8139 interface.
 */
const net_stats_t* rtl8139_get_stats(void) {
    return &s_stats;
}
