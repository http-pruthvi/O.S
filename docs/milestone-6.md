# Milestone 6: Productivity Desktop Applications

## 1. Goal and What Was Built

The primary objective of Milestone 6 is transforming MyOS from a GUI widget demonstration into a complete, productive, mouse-driven desktop operating system. Milestone 6 introduces four standalone productivity applications—a desktop calculator, a multi-line text editor, an Excel-like spreadsheet with formula evaluation, and a live digital clock with an interactive monthly calendar—all orchestrated by an application launcher, clickable desktop icons, and a classic Windows-style Start Menu.

### Core Deliverables
1. **Application Launcher & Popup Start Menu (`apps/launcher.c`)**:
   - **Start Menu**: Clicking the taskbar's `[Start]` button toggles a 3D beveled popup menu (`x = 6`, `y = 514`, `w = 220`, `h = 218`) featuring a dark blue vertical brand banner ("MyOS") and 7 items: Calculator, Text Editor, Spreadsheet, Clock & Calendar, System Monitor, Paint Demo, and Shutdown (Exit).
   - **Desktop Shortcut Icons**: 6 stylized desktop icons rendered on the desktop background (`x = 24`, spacing `86px`), complete with custom 2D glyphs (calculator body, notepad paper, green spreadsheet matrix, clock dial, monitor chassis, and info badge) and text labels.
   - **Window Instance Management**: Clicking any desktop icon or Start Menu item launches the app or brings an existing instance to the front (`wm_bring_to_front()`).
   - **Clean Shutdown**: Selecting "Shutdown (Exit)" cleanly exits the graphical desktop and returns to the interactive console.

2. **Desktop Calculator (`apps/calc.c`)**:
   - Compact window (260x330) with an LCD-style digital readout display.
   - 4x5 button matrix: Digits `0`–`9`, operators `+`, `-`, `*`, `/`, `C` (Clear all), `CE` (Clear entry), `+/-` (Negate), and `=` (Equals).
   - 32-bit signed arithmetic state machine with integer division, negative number handling, operator chaining (`5 + 3 * 2 =`), and error detection for division by zero (`Error: /0`) and overflow.
   - Full keyboard typing support: typing numbers, operators, or `Enter` on the keyboard routes directly into the calculator when the window has focus.

3. **Notepad-like Text Editor (`apps/editor.c`)**:
   - Window (520x360) with a multi-line text editing canvas.
   - Multi-line buffer supporting up to 64 lines of 60 characters each.
   - Live vertical caret cursor tracking row and column position.
   - Full typing engine: printable character insertion, `Enter` line splitting, `Backspace` character deletion across lines, and `Tab` indentation (4 spaces).
   - Vertical viewport scrolling when text exceeds visible lines.
   - Toolbar: `[New]` (resets document) and `[Sample]` (loads project demonstration text).
   - Bottom status bar displaying real-time metrics: `Ln X, Col Y | Lines: N | Chars: M`.

4. **Mini Spreadsheet (`apps/sheet.c`)**:
   - Window (580x330) featuring a 2D grid of 60 cells: 6 columns (`A`–`F`) × 10 rows (`1`–`10`).
   - Top formula bar displaying active cell address (e.g., `B3`) and editable raw formula text.
   - Formula Evaluation Engine supporting:
     - Integer constants and text labels.
     - Cell arithmetic: `=A1+A2`, `=B2*C2`, `=C3-10`, `=D1/2`.
     - Range functions: `=SUM(D2:D5)`, `=AVG(D2:D5)`, `=MIN(D2:D5)`, `=MAX(D2:D5)`.
     - Two-pass dependency evaluation: editing any input cell automatically updates all dependent formula cells.
   - `[Sample]` button loading a full hardware budget spreadsheet with automatic row costs, total sum, average, min, and max calculations.
   - Interactive mouse selection: clicking any cell selects it and populates the formula bar.

5. **Real-Time Clock & Monthly Calendar (`apps/clock.c`)**:
   - Window (320x350) with:
     - Large stylized digital clock (`HH:MM:SS`) updated every second via PIT timer ticks.
     - Live system uptime counter in seconds.
     - Interactive monthly calendar with Sakamoto's day-of-week algorithm, `[<]` and `[>]` month navigation buttons, and today's date highlighted with a distinct blue cell.

---

## 2. How It Works

### Architectural Overview

```mermaid
graph TD
    subgraph Input ["User Input (Mouse & Keyboard)"]
        Mouse["PS/2 Mouse (IRQ 12)"] --> WM_Mouse["wm_handle_mouse()"]
        Kbd["PS/2 Keyboard (IRQ 1)"] --> WM_Kbd["wm_handle_keyboard()"]
        Timer["PIT Timer (IRQ 0)"] --> ClockTick["app_clock_update()"]
    end

    subgraph Launcher ["Launcher & Start Menu (apps/launcher.c)"]
        WM_Mouse -->|Click on Desktop Icon| LaunchApp["Launch Selected App"]
        WM_Mouse -->|Click [Start] Button| ToggleMenu["Toggle Start Menu"]
        ToggleMenu --> StartMenuUI["Render Start Menu Popup (220x218)"]
        StartMenuUI -->|Select Item| LaunchApp
    end

    subgraph Apps ["Milestone 6 Productivity Applications"]
        LaunchApp --> Calc["Calculator (apps/calc.c)"]
        LaunchApp --> Editor["Text Editor (apps/editor.c)"]
        LaunchApp --> Sheet["Spreadsheet (apps/sheet.c)"]
        LaunchApp --> Clock["Clock & Calendar (apps/clock.c)"]
    end

    subgraph Routing ["Event Routing to Active Window"]
        WM_Kbd -->|Focused Window on_key| AppKeyDispatch{"Window has on_key?"}
        AppKeyDispatch -->|Yes| CalcKey["calc_handle_digit/op()"]
        AppKeyDispatch -->|Yes| EditorKey["editor_insert_char/newline()"]
        AppKeyDispatch -->|Yes| SheetKey["sheet formula input"]
        AppKeyDispatch -->|No| WidgetKey["widget_dispatch_key()"]

        WM_Mouse -->|Client Click on_click| SheetClick["sheet_handle_client_click()"]
    end

    subgraph Compositor ["Compositor & Display"]
        Calc --> WinBuf["Window client_buffer in RAM"]
        Editor --> WinBuf
        Sheet --> WinBuf
        Clock --> WinBuf
        WinBuf --> CompositorEngine["wm_invalidate_rect() (Painter's Algorithm)"]
        CompositorEngine --> VRAM["Linear Framebuffer (0xE0000000)"]
    end
```

---

## 3. Key Concepts Explained Simply

### 1. How GUI Applications Run in a Ring 0 Kernel
In standard desktop operating systems (like Linux or Windows), applications run as separate user-mode processes (`Ring 3`) in isolated virtual address spaces. In our educational hobby OS milestone, all apps run as **in-kernel graphical tasks** (`Ring 0`). Each application creates a `window_t` object with an isolated off-screen `client_buffer`. The kernel's event loop dispatches mouse and keyboard events directly to the focused window's callbacks, and the window compositor blits the canvas to video memory.

### 2. Arithmetic State Machine & 32-bit x86 Division
In freestanding 32-bit x86 C without standard runtime libraries (`libc`), operations like 64-bit integer division (`long long / long long`) require compiler helper functions like `__divdi3`. To keep our operating system freestanding, fast, and self-contained, our calculator uses 32-bit signed integers (`int32_t`), which GCC compiles directly into native x86 hardware instructions (`idiv`, `imul`, `add`, `sub`). We enforce an entry cap of 9 digits (`±999,999,999`) to prevent integer overflow during interactive entry.

### 3. Multi-Line Text Buffer & Dynamic Caret
The text editor maintains a 2D line array `char s_lines[64][60]` and length array `s_line_lens[64]`:
- **Character Insertion**: Shifts characters to the right of the caret by 1 and inserts the new character at `s_cursor_col`.
- **Newline**: Inserts a new line entry at `s_cursor_row + 1`, copies characters after the caret to the new line, truncates the current line at the caret, and increments `s_cursor_row`.
- **Backspace Across Lines**: If the caret is at column 0 and row > 0, the current line's text is appended to the previous line, the lines below are shifted up, and the caret moves to the join point.

### 4. Two-Pass Spreadsheet Formula Resolution
A spreadsheet formula like `=SUM(D2:D5)` or `=B2*C2` references other cells that may themselves contain formulas:
- **Pass 1**: Constant numbers and plain text labels are parsed and stored in `cell->computed_val`.
- **Pass 2**: Formula strings starting with `=` are evaluated using cell coordinates and arithmetic operators. Running evaluation across two passes ensures that basic dependent formulas resolve correctly without circular infinite loops.

### 5. Sakamoto's Day-of-Week Algorithm
To compute the exact calendar layout for any month and year without external date libraries, the clock application implements Tomohiko Sakamoto's algorithm:
```c
int get_first_day_of_week(int year, int month) {
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (month < 3) year -= 1;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + 1) % 7;
}
```
This formula returns the weekday of the 1st day of the month (`0 = Sunday` through `6 = Saturday`) in `O(1)` time with zero table lookups or runtime memory overhead.

---

## 4. Files Created and Modified

| File | Role in Milestone 6 |
| :--- | :--- |
| `include/apps/launcher.h` | Header declaring Start Menu, desktop shortcut dimensions, and app launch APIs. |
| `src/apps/launcher.c` | Implements Start Menu popup, 6 desktop icons with 2D glyphs, and app launching logic. |
| `include/apps/calc.h` | Header declaring Calculator app entry point. |
| `src/apps/calc.c` | Implements Calculator UI (4x5 button grid, LCD display), 32-bit arithmetic engine, and keyboard routing. |
| `include/apps/editor.h` | Header declaring Text Editor app entry point. |
| `src/apps/editor.c` | Implements multi-line text buffer (64 lines), caret cursor, line split/join, status bar, and toolbar. |
| `include/apps/sheet.h` | Header declaring Spreadsheet app entry point. |
| `src/apps/sheet.c` | Implements 6x10 cell grid, formula bar, arithmetic/range formula parser (`=SUM`, `=AVG`), and sample data. |
| `include/apps/clock.h` | Header declaring Clock & Calendar entry point and periodic update hook. |
| `src/apps/clock.c` | Implements digital readout, uptime counter, Sakamoto's calendar grid, and month navigation. |
| `include/gui/window.h` | Added `on_key` and `on_click` optional event callbacks to `window_t`. |
| `src/gui/window.c` | Initialized `on_key = NULL` and `on_click = NULL` in `window_create()`. |
| `src/gui/wm.c` | Integrated desktop icon and Start menu rendering in compositor; routed clicks and keys to active windows. |
| `src/gui/desktop.c` | Added `launcher_init()`, Start menu exit check, and hooked `app_clock_update()` on timer ticks. |
| `include/lib/string.h` | Added `strcpy`, `strncpy`, and `strcmp` declarations. |
| `src/lib/string.c` | Implemented freestanding `strcpy`, `strncpy`, and `strcmp`. |
| `src/kernel/kernel.c` | Updated boot welcome banner to Milestone 6. |

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
   - The boot banner displays:
     ```text
     ================================================================
       MyOS - 32-bit x86 Kernel (Milestone 6: Productivity Desktop)  
     ================================================================
     ```
2. **Launching the Graphical Desktop**:
   - Press **`[F10]`** to enter the graphical environment.
   - Notice the 6 desktop shortcut icons on the left side of the desktop:
     `Calculator`, `Editor`, `Sheets`, `Clock`, `SysMon`, `About`.
3. **Start Menu Testing**:
   - Click the **`[Start]`** button at the bottom-left of the taskbar:
     A classic 3D beveled Start Menu opens with the blue "MyOS" vertical banner and 7 application choices.
   - Click outside the menu: it closes cleanly.
4. **Testing the Calculator (`apps/calc.c`)**:
   - Click the **Calculator** icon or select it from the Start menu.
   - Click `7`, `*`, `8`, `=`: the display shows `56`.
   - Click `1`, `0`, `/`, `0`, `=`: display reports `Error: /0`.
   - Click `C` to clear, then type `1`, `2`, `+`, `5`, `Enter` on the keyboard: display shows `17`.
5. **Testing the Text Editor (`apps/editor.c`)**:
   - Click the **Editor** icon.
   - Click **`[Sample]`**: loads project text with numbered bullet points.
   - Click anywhere in the text: caret repositions.
   - Type new words, press `Enter` to create lines, use `Backspace` to delete. Notice the status bar at the bottom updating line, column, and character counts in real time.
6. **Testing the Spreadsheet (`apps/sheet.c`)**:
   - Click the **Sheets** icon.
   - Click **`[Sample]`**: loads a hardware budget table. Notice `D2` (`=B2*C2`) evaluates to `100`, `D6` (`=SUM(D2:D5)`) evaluates to `280`, and `D7` (`=AVG(D2:D5)`) evaluates to `70`!
   - Click cell `B2`, edit `4` to `10` in the formula bar, and press Enter: `D2` automatically recalculates to `250`, and `Total` automatically updates to `430`!
7. **Testing the Clock & Calendar (`apps/clock.c`)**:
   - Click the **Clock** icon.
   - Watch the digital time (`HH:MM:SS`) and uptime counter tick in real time.
   - Click `[<]` and `[>]` to browse months; notice the calendar days shift and today (October 31, 2026) is highlighted.
8. **Shutdown**:
   - Open Start Menu and click **"Shutdown (Exit)"** or press **`ESC`**: the desktop cleanly returns to the console.

---

## 6. Viva Questions and Answers

### Q1: How do applications in your operating system execute concurrently without preemptive kernel threads?
**Answer**:
Applications run cooperatively inside the kernel's single address space. Each app instance owns an allocated `window_t` and `client_buffer`. The central desktop event loop continuously drains hardware events from the mouse (IRQ 12) and keyboard (IRQ 1) ring buffers, routing events to the currently focused window. When an app modifies its state, it requests the compositor to repaint its dirty region. This provides an interactive multi-window user experience while keeping the kernel simple and deterministic.

### Q2: Why did 64-bit integer division fail when compiling the calculator on i686-elf-gcc, and how did you resolve it?
**Answer**:
On a 32-bit x86 target without standard runtime libraries (`libc`), dividing two 64-bit integers (`long long / long long`) requires GCC's runtime math library helper function `__divdi3`. Because our kernel is freestanding and links with `-nostdlib`, referencing 64-bit division produces an unresolved symbol at link time. We resolved this by using 32-bit signed integers (`int32_t`), which GCC compiles directly into native single-instruction x86 hardware integer division (`idiv`).

### Q3: How does your spreadsheet evaluate range formulas like `=SUM(A1:A5)`?
**Answer**:
When a formula begins with `=SUM(` or `=AVG(`, the parser extracts the start cell coordinate (`A1`) and end cell coordinate (`A5`). It converts column letters (`A`–`F`) to column indices (`0`–`5`) and row numbers (`1`–`10`) to row indices (`0`–`9`). It then iterates through the rectangular cell matrix bounded by `(min_col, min_row)` and `(max_col, max_row)`, skipping empty cells or errors, and accumulates numeric cell values into a running sum and counter.

### Q4: How is the text editor's caret position tracked and rendered on screen?
**Answer**:
The editor maintains `s_cursor_row` (0-indexed line in the document) and `s_cursor_col` (0-indexed character offset in that line). During canvas rendering, the pixel coordinates of the caret are calculated as `x = margin_x + s_cursor_col * FONT_GLYPH_WIDTH` and `y = margin_y + (s_cursor_row - s_scroll_top) * LINE_HEIGHT`. A 2-pixel wide vertical rectangle is drawn into the window's client buffer at those coordinates, ensuring the caret visually aligns with character glyphs.

### Q5: How does the Start Menu achieve tear-free overlay rendering without corrupting the windows beneath it?
**Answer**:
The Start Menu is integrated directly into the window manager's Painter's Algorithm compositing pipeline. When the Start Menu is open, `wm_invalidate_rect()` first composites the desktop background, then composites all open windows in bottom-to-top Z-order, then renders the taskbar, and finally renders the Start Menu popup on top of all layers. When the Start menu is closed, invalidating the menu's bounding box causes the compositor to reconstruct and redraw the exact contents of the windows previously hidden beneath it.
