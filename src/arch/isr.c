#include "arch/isr.h"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"

/* External assembly stubs declared in isr_stubs.asm */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr128(void);

/* Table of human-readable CPU exception descriptions */
static const char* const exception_messages[32] = {
    "Divide-by-zero Error (#DE)",
    "Debug (#DB)",
    "Non-maskable Interrupt (NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

/*
 * isr_init:
 * Installs all 32 CPU exception stubs and test software interrupt (vector 128) into the IDT.
 */
void isr_init(void) {
    idt_set_gate(0,  (uint32_t)isr0,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(1,  (uint32_t)isr1,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(2,  (uint32_t)isr2,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(3,  (uint32_t)isr3,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(4,  (uint32_t)isr4,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(5,  (uint32_t)isr5,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(6,  (uint32_t)isr6,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(7,  (uint32_t)isr7,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(8,  (uint32_t)isr8,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(9,  (uint32_t)isr9,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(10, (uint32_t)isr10, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(11, (uint32_t)isr11, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(12, (uint32_t)isr12, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(13, (uint32_t)isr13, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(14, (uint32_t)isr14, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(15, (uint32_t)isr15, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(16, (uint32_t)isr16, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(17, (uint32_t)isr17, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(18, (uint32_t)isr18, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(19, (uint32_t)isr19, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(20, (uint32_t)isr20, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(21, (uint32_t)isr21, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(22, (uint32_t)isr22, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(23, (uint32_t)isr23, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(24, (uint32_t)isr24, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(25, (uint32_t)isr25, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(26, (uint32_t)isr26, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(27, (uint32_t)isr27, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(28, (uint32_t)isr28, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(29, (uint32_t)isr29, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(30, (uint32_t)isr30, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(31, (uint32_t)isr31, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);

    /* Software interrupt vector 128 (0x80) */
    idt_set_gate(128, (uint32_t)isr128, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_USER_INT_GATE);
}

/*
 * isr_handler:
 * Central C dispatcher called from isr_common_stub in isr_stubs.asm.
 */
void isr_handler(registers_t* regs) {
    if (!regs) {
        return;
    }

    /* Handle software interrupt test hook (vector 128 / 0x80) */
    if (regs->int_no == 128) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("\n[INT 0x80] Software interrupt received successfully! (EIP: %p)\n", (void*)regs->eip);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        /* Return to caller; CPU executes iret and resumes execution */
        return;
    }

    /* Vector 14: Dedicated Page Fault handler */
    if (regs->int_no == 14) {
        extern void page_fault_handler(registers_t* regs);
        page_fault_handler(regs);
        return;
    }

    /* Hide software cursor before panic output */
    extern void cursor_hide(void);
    cursor_hide();

    /* All other vectors 0-31 are fatal CPU exceptions: display Kernel Panic Screen */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n============================ KERNEL PANIC ============================\n");

    const char* desc = (regs->int_no < 32) ? exception_messages[regs->int_no] : "Unknown Exception";
    kprintf(" EXCEPTION: %s (Vector: %u, Error Code: 0x%x)\n", desc, regs->int_no, regs->err_code);
    kprintf("----------------------------------------------------------------------\n");
    kprintf(" CS:EIP: 0x%x:%p   EFLAGS: 0x%x\n", regs->cs, (void*)regs->eip, regs->eflags);
    kprintf(" EAX: 0x%x   EBX: 0x%x   ECX: 0x%x   EDX: 0x%x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf(" ESI: 0x%x   EDI: 0x%x   EBP: 0x%x   ESP: %p\n",
            regs->esi, regs->edi, regs->ebp, (void*)(uintptr_t)(regs + 1));
    kprintf(" DS: 0x%x    ES: 0x%x    FS: 0x%x    GS: 0x%x\n",
            regs->ds, regs->es, regs->fs, regs->gs);
    kprintf("======================================================================\n");
    kprintf(" System halted safely.\n");

    /* Infinite halt loop */
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}
