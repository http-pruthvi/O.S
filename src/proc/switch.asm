; ==============================================================================
; switch.asm - Assembly Context Switch & Task Trampoline (Milestone 7)
; ==============================================================================
; Architecture: x86 32-bit protected mode (cdecl calling convention)
;
; Why this is needed:
; Context switching requires manipulating the CPU stack pointer (ESP) and
; saving/restoring CPU callee-saved registers and the EFLAGS register.
; These operations cannot be expressed purely in freestanding C without compiler
; interference.
; ==============================================================================

section .text

global switch_context
global task_entry_trampoline
extern task_exit

; ------------------------------------------------------------------------------
; switch_context:
; Swaps execution context from an old task to a new task.
;
; C signature:
;   void switch_context(uint32_t* old_esp, uint32_t new_esp);
;
; Parameters on entry:
;   [esp + 4] = old_esp: pointer to uint32_t storing old task's stack pointer
;   [esp + 8] = new_esp: new task's stack pointer to restore into ESP
; ------------------------------------------------------------------------------
switch_context:
    ; 1. Save EFLAGS so interrupt enable/disable state is preserved
    pushf

    ; 2. Save callee-saved general-purpose registers per System V x86 ABI
    push ebp
    push edi
    push esi
    push ebx

    ; 3. Store current ESP into *old_esp
    ; Stack contents:
    ;   [esp + 0]  = saved ebx
    ;   [esp + 4]  = saved esi
    ;   [esp + 8]  = saved edi
    ;   [esp + 12] = saved ebp
    ;   [esp + 16] = saved eflags (pushf)
    ;   [esp + 20] = return address to caller (e.g. schedule())
    ;   [esp + 24] = old_esp parameter
    ;   [esp + 28] = new_esp parameter
    mov eax, [esp + 24]
    mov [eax], esp

    ; 4. Switch stack to new task
    mov esp, [esp + 28]

    ; 5. Restore callee-saved registers from the new task's stack
    pop ebx
    pop esi
    pop edi
    pop ebp

    ; 6. Restore EFLAGS (restoring interrupt state IF)
    popf

    ; 7. Return to caller or task_entry_trampoline
    ret

; ------------------------------------------------------------------------------
; task_entry_trampoline:
; Initial landing pad for newly created tasks when switched to for the first time.
;
; Stack state upon entry:
;   [esp + 0] = entry function pointer (task_entry_t)
;   [esp + 4] = argument pointer (void* arg)
; ------------------------------------------------------------------------------
task_entry_trampoline:
    ; 1. Enable hardware interrupts now that the task context is fully active
    sti

    ; 2. Pop entry function pointer into EAX
    pop eax

    ; 3. Call the entry function with 'arg' at [esp] (cdecl argument convention)
    call eax

    ; 4. If entry function returns, cleanly terminate the task
    call task_exit

    ; 5. Safety fallback in case task_exit returns (should never happen)
.hang:
    cli
    hlt
    jmp .hang
