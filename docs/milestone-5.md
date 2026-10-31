# Milestone 5: Window Manager & Widgets

## 1. Goal and What Was Built

The primary objective of Milestone 5 is creating a full graphical desktop environment with an overlapping window manager and an interactive widget system. Moving beyond the flat canvas and paint demo of Milestone 4, Milestone 5 introduces structured window management, z-order layer compositing, interactive mouse dragging, and reusable user-interface controls (buttons, labels, and text input boxes).

### Core Deliverables
1. **Window Data Structures & Client Canvas Isolation**:
   - Each window (`window_t`) is allocated with an independent off-screen RAM canvas (`client_buffer`, size `client_w * client_h * sizeof(uint32_t)`).
   - Dedicated drawing APIs for client canvases: `window_putpixel()`, `window_fill_rect()`, `window_draw_rect()`, `window_draw_string()`, and `window_clear_client()`.
   - Drawing into a client canvas is completely decoupled from screen coordinates; window contents are drawn relative to `(0, 0)`.
   - Window frames feature 2px 3D borders, title bars with active (`0x001F487E`) and inactive (`0x007A8A9E`) color gradients, centered or left-aligned title text, and an interactive close button `[X]` with red hover/press highlighting.

2. **Window Manager & Z-Order Compositor**:
   - Implemented a doubly linked list maintaining windows sorted by depth (Z-order) from top (`wm_top_window`) to bottom (`wm_bottom_window`).
   - `wm_bring_to_front()` elevates any clicked window to the top of the stack and updates active/inactive title bar states.
   - Compositing engine uses the **Painter's Algorithm**: iterates from bottom window to top window, blitting client buffers and window frames into the 3 MB off-screen back buffer, then flushes modified dirty regions to VRAM via `gfx_flush_rect()`.
   - Tear-free cursor integration: all flushes automatically hide the software cursor, blit to VRAM, and restore the cursor.

3. **Mouse Interaction & Dragging Engine**:
   - Click-to-focus: clicking anywhere within a window's bounds raises it to the front.
   - Smooth title-bar dragging: tracks mouse delta `(dx, dy)` and clamps the window within desktop bounds (cannot be dragged above the top of the screen or below the bottom taskbar).
   - Close button interaction: clicking `[X]` destroys the window, frees all allocated child widgets and canvas memory, and triggers compositor redraw.

4. **Taskbar & System Tray**:
   - Fixed 32-pixel taskbar docked at the bottom of the screen (`y = 736` to `767`).
   - Displays a classic beveled "Start" button, dynamic window tabs for all active windows (showing title text and pressed highlight for the focused window), and a system tray uptime clock (`HH:MM:SS`) updated asynchronously from PIT timer ticks.
   - Clicking a taskbar tab brings the corresponding window to the front or toggles its minimized state.

5. **Reusable Widget Framework**:
   - Polymorphic widget structure (`widget_t`) supporting labels, push buttons, and text input fields.
   - **Labels**: Render single-line text strings with custom foreground colors.
   - **3D Relief Push Buttons**: Rendered with light top/left borders (`0x00FFFFFF`) and dark bottom/right shadows (`0x00404040`) for an authentic classic OS beveled look; inverts borders and shifts text by `(1, 1)` when pressed.
   - **Text Input Boxes**: White input field with inset sunken borders, focus outline, blinking caret cursor, keyboard event dispatching, and backspace deletion.

6. **Desktop Showcase & Test Hook F10**:
   - Pressing **F10** from the interactive console launches the desktop environment (`desktop_run()`).
   - Instantiates 3 overlapping interactive showcase windows:
     - **Window 1: "Widget Showcase"**: Contains interactive `[-] Dec`, `[+] Inc`, and `Reset` buttons updating a live counter label, alongside a focused text box with real-time typing echo.
     - **Window 2: "System Monitor"**: Displays live system statistics (total RAM, PMM free frames, kernel heap base at `0xD0000000`, heap memory used), with a "Refresh Metrics" button.
     - **Window 3: "About MyOS"**: Center dialog with milestone highlights and a "Dismiss" button.
   - Pressing **ESC** cleanly terminates the desktop event loop, unallocates back buffers, restores the framebuffer text console via `fb_console_repaint()`, and returns to the interactive shell.

---

## 2. How It Works

### Architectural Overview

```mermaid
graph TD
    subgraph Input ["Hardware Interrupts & Drivers"]
        MouseHW["PS/2 Mouse (IRQ 12)"] -->|mouse_poll_event| EventLoop["desktop_run() Event Loop"]
        KbdHW["PS/2 Keyboard (IRQ 1)"] -->|keyboard_getchar| EventLoop
        TimerHW["PIT Timer (IRQ 0)"] -->|timer_is_clock_dirty| ClockUpdate["Taskbar Clock Refresh"]
    end

    subgraph WM ["Window Manager & Event Dispatch (wm.c)"]
        EventLoop -->|Mouse Event (mx, my)| MouseDispatch{"Hit Test Target?"}
        MouseDispatch -->|Title Bar Drag| DragHandler["Update win->x, win->y (Clamped)"]
        MouseDispatch -->|Close Button [X]| CloseHandler["window_destroy() -> Free RAM"]
        MouseDispatch -->|Taskbar Tab Click| FocusHandler["wm_bring_to_front()"]
        MouseDispatch -->|Client Area Click| WidgetMouse["widget_dispatch_mouse()"]

        EventLoop -->|Keystroke (ASCII / Scancode)| WidgetKey["widget_dispatch_key()"]
    end

    subgraph Widgets ["Widget System (widget.c)"]
        WidgetMouse -->|Click inside button| BtnClick["Invoke on_click callback()"]
        WidgetKey -->|Char / Backspace| TxtInput["Update textbox text & caret"]
    end

    subgraph Compositor ["Z-Order Painter's Compositor"]
        DragHandler --> Invalidate["wm_invalidate_rect()"]
        CloseHandler --> Invalidate
        FocusHandler --> Invalidate
        BtnClick --> Invalidate
        TxtInput --> Invalidate

        Invalidate --> RenderDesktop["1. Draw Desktop Background (0x003A6EA5)"]
        RenderDesktop --> RenderTaskbar["2. Render Taskbar & Tabs"]
        RenderTaskbar --> RenderZOrder["3. Render Windows: Bottom -> Top"]
        RenderZOrder --> BlitCanvas["Blit Window Frame & client_buffer"]
        BlitCanvas --> BackBuf["3 MB Off-Screen Back Buffer"]
        BackBuf --> FlushVRAM["gfx_flush_rect() -> VRAM (Tear-Free)"]
    end
```

### Z-Order Traversal vs Hit Testing

A critical design pattern in window managers is the opposing directions of **hit testing** and **compositing**:

```text
       Top Window (Z = 0, Focused)  <--- Hit Test starts here (Top to Bottom)
             |
       Middle Window (Z = 1)
             |
       Bottom Window (Z = 2)        <--- Painter's Algorithm starts here (Bottom to Top)
```

1. **Compositing (Bottom-to-Top)**:
   - When redrawing the desktop, the compositor begins at `wm_bottom_window` and traverses up through `window->prev` until it reaches `wm_top_window`.
   - Higher windows naturally overwrite lower windows in the back buffer, producing correct visual occlusion without complex clipping mathematics.
2. **Hit Testing (Top-to-Bottom)**:
   - When a mouse click occurs, the event dispatcher begins at `wm_top_window` and traverses down to `wm_bottom_window`.
   - The first window whose bounding box contains the mouse coordinates intercepts the event, preventing obscured windows beneath from receiving phantom clicks.

---

## 3. Key Concepts Explained Simply

### 1. What is Z-Order?
In a 2D graphical desktop, pixels only have `X` (horizontal) and `Y` (vertical) coordinates. The third dimension—depth or layering—is called **Z-order** (referencing the Cartesian Z axis). A window with a higher Z-order visually floats above windows with lower Z-orders. In our operating system, Z-order is maintained as a doubly linked list where the head is the top-most window.

### 2. Why Client Canvas Buffering Beats In-Place Drawing
If an application draws directly into the video display buffer, moving or resizing any overlapping window requires all windows behind it to immediately recalculate and redraw their entire UI. If an application is slow, moving a window causes "trails" and visual corruption (the classic Windows XP freeze effect).
By allocating an isolated `client_buffer` in RAM for each window:
- An application renders into its own off-screen memory without knowing or caring where it is on the screen.
- When a window is dragged, the compositor simply copies rectangular strips from `client_buffer` into the screen back buffer. It never asks the application to redraw its controls!

### 3. What is Dirty Rectangle Invalidation?
A 1024x768 32-bit display requires 3,145,728 bytes per full frame. Copying 3 MB over the PCI bus to video memory on every single mouse event would bottleneck CPU performance and cause sluggish cursor movement.
**Dirty rectangle invalidation** tracks the exact bounding box `(x, y, w, h)` modified by an action (such as a 120x30 button click or a window move from `old_rect` to `new_rect`). Only pixels within that bounding box are composited and flushed to VRAM, leaving the rest of the display untouched.

### 4. 3D Beveled Relief Button Rendering
To create an authentic 1990s/early-2000s desktop appearance without GPU shaders:
- **Released State**: The top and left outer borders are drawn in white (`0x00FFFFFF`), while the bottom and right borders are drawn in dark gray (`0x00404040`). Because humans perceive light as originating from the top-left, this produces an optical illusion of a button raised above the surface.
- **Pressed State**: The colors are inverted (top-left becomes dark, bottom-right becomes white), and the button text is shifted 1 pixel right and 1 pixel down. The button appears to physically depress into the window surface.

### 5. Single-Line Text Box Caret & Keyboard Focus
Only one widget holds **keyboard focus** at any given time. When a text box is clicked:
- It claims focus and highlights its border.
- Keystrokes received from the keyboard driver are appended to its internal character buffer.
- Backspace deletes the preceding character.
- A vertical cursor line (the caret) is drawn at `text_width + 4` pixels from the left border, indicating where the next character will be placed.

---

## 4. Files Created and Modified

| File | Role in Milestone 5 |
| :--- | :--- |
| `include/gui/window.h` | Defines `window_t` struct, window flags, title bar dimensions, and client canvas drawing prototypes. |
| `src/gui/window.c` | Implements window allocation, 2px borders, active/inactive title bars, close button, and client canvas rendering. |
| `include/gui/wm.h` | Defines window manager state, desktop metrics (taskbar height, margins), and compositor prototypes. |
| `src/gui/wm.c` | Implements Z-order linked list, Painter's algorithm compositor, mouse dragging with boundary clamping, and taskbar. |
| `include/gui/widget.h` | Defines `widget_t` polymorphic structure, widget types (Button, Label, Textbox), and event dispatch headers. |
| `src/gui/widget.c` | Implements button relief rendering, label text rendering, textbox caret/input handling, and event dispatchers. |
| `include/gui/desktop.h` | Header declaring `desktop_run()` entry point. |
| `src/gui/desktop.c` | Desktop showcase application initializing 3 windows, event loops, clock updates, and clean ESC exit. |
| `include/drivers/keyboard.h` | Added `#define KEY_F10 0x44` (PS/2 Scancode Set 1 make code for F10). |
| `src/drivers/keyboard.c` | Added scancode `0x44` handling to put `KEY_F10` in keyboard buffer. |
| `src/kernel/kernel.c` | Updated boot banner, added `#include "gui/desktop.h"`, and bound `KEY_F10` to `desktop_run()`. |
| `README.md` | Documented Milestone 5 source layout, architecture, and `[F10]` interactive hook. |

---

## 5. How to Build, Deploy, and Test

### Build and Deploy Commands
Execute in WSL2:
```bash
cd ~/myos
make clean && make run
```

### Expected Output
1. **Boot Screen**:
   - The boot banner displays all test hooks including:
     ```text
     - [F9]: Mouse Paint Demo           [F10]: Desktop Window Manager
     ```
2. **Launching Desktop**:
   - Pressing **`[F10]`** transitions the screen from console to the graphical desktop:
     - Classic desktop blue background (`#3A6EA5`).
     - Bottom taskbar with "Start" button, window tabs, and live updating system clock (`HH:MM:SS`).
     - Three overlapping windows:
       1. **Widget Showcase**: Left window with Dec, Inc, Reset counter buttons and live text box echo.
       2. **System Monitor**: Right window showing real-time RAM, PMM frames, heap usage, and "Refresh Metrics" button.
       3. **About MyOS**: Overlapping center window with milestone description and "Dismiss" button.
3. **Interactive Testing**:
   - **Dragging**: Click and hold the title bar of any window; drag it across the screen. Notice it stays within desktop bounds and does not corrupt windows behind it.
   - **Z-Order Focus**: Clicking a lower window raises it to the top and highlights its title bar with active blue.
   - **Counter Buttons**: Click `[+] Inc` or `[-] Dec`; the counter label updates immediately.
   - **Typing Input**: Click inside the text box, type any text; characters appear with a caret, and the label below echoes the text in real-time. Backspace deletes characters.
   - **Close Button**: Click `[X]` on "About MyOS"; the window vanishes and the windows underneath repaint cleanly.
   - **Exit**: Press **ESC**; the desktop safely exits, restoring the framebuffer console and printing:
     ```text
     [DESKTOP] Returned from Desktop Environment to Interactive Console.
     ```

---

## 6. Viva Questions and Answers

### Q1: What is the Painter's Algorithm and why is it used in your window manager?
**Answer**:
The Painter's Algorithm is a rendering technique where graphical elements are drawn in order of increasing distance from the viewer, similar to how an oil painter paints background scenery before foreground figures. In our window manager, we iterate through the Z-order linked list starting from the bottom-most window up to the top-most window. Each window renders its client canvas over whatever was drawn previously. This guarantees that higher windows naturally occlude lower windows without requiring complex polygon clipping algorithms.

### Q2: Why does hit-testing traverse windows in the exact opposite direction of compositing?
**Answer**:
Compositing traverses bottom-to-top so foreground windows visually paint over background windows. Conversely, mouse clicks must be intercepted by the window closest to the user's eye. If hit-testing traversed bottom-to-top, a click would register with a hidden, obscured window at the back rather than the focused window on top. Traversing top-to-bottom ensures the uppermost window containing the mouse cursor consumes the click event first.

### Q3: How does the window manager prevent memory leaks when a window is closed?
**Answer**:
When `window_destroy()` is invoked, it traverses the window's child widget linked list and frees every allocated button, label, and text box using `kfree()`. Next, it frees the window's dedicated off-screen `client_buffer` in RAM. Finally, it unlinks the window node from the Z-order list and frees the `window_t` structure itself.

### Q4: How is window dragging implemented without tearing or leaving ghost trails?
**Answer**:
When the mouse left button is held over a title bar, the window manager computes `dx = cur_x - prev_x` and `dy = cur_y - prev_y`. It calculates the union bounding box enclosing both the old window rectangle and the new window rectangle (`union_rect`). The window coordinates are updated, clamped to screen boundaries, and `wm_invalidate_rect(&union_rect)` is called. The compositor repaints the desktop background and all windows intersecting that bounding box into the off-screen back buffer before flushing to VRAM, ensuring zero ghost trails or tearing.

### Q5: How do widgets dispatch events in freestanding C without an object-oriented runtime?
**Answer**:
We use function pointers inside standard C structs (simulating polymorphic virtual method tables). Each `widget_t` contains an `on_click` callback pointer and a `void* user_data` context pointer. When a mouse release event occurs inside a button's bounding box, the window manager dispatches the event to the button, which invokes `button->on_click(button, button->user_data)`. This allows decoupled event handling between UI controls and application logic.
