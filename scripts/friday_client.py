#!/usr/bin/env python3
"""
Friday AI Host Client v1.0
Host-side bridge for Friday OS voice/text assistant.
Connects to VirtualBox COM2 TCP server and translates
multilingual commands to kernel protocol packets.

Architecture:
    User types/speaks -> Python client -> TCP socket -> VirtualBox COM2
    -> Kernel UART IRQ 3 -> Friday parser
    Kernel response -> COM2 TX -> TCP socket -> Python client -> Display

Usage:
    python friday_client.py              # Text mode
    python friday_client.py --voice      # Voice mode (requires speech_recognition)
"""

import socket
import sys
import time
import select

# ---------------------------------------------------------------------------
# Optional voice dependencies — graceful fallback if not installed
# ---------------------------------------------------------------------------
try:
    import speech_recognition as sr
    VOICE_AVAILABLE = True
except ImportError:
    VOICE_AVAILABLE = False

# ---------------------------------------------------------------------------
# ANSI colour codes for pretty terminal output
# ---------------------------------------------------------------------------
GREEN   = "\033[92m"
CYAN    = "\033[96m"
YELLOW  = "\033[93m"
RED     = "\033[91m"
MAGENTA = "\033[95m"
RESET   = "\033[0m"
BOLD    = "\033[1m"
DIM     = "\033[2m"

# ---------------------------------------------------------------------------
# Connection parameters
# ---------------------------------------------------------------------------
HOST = "localhost"
PORT = 2345
SOCKET_TIMEOUT = 0.1          # Non-blocking read timeout (seconds)
RECONNECT_DELAY = 2.0         # Seconds between reconnect attempts
RECV_BUFFER = 4096            # Bytes per socket read

# ---------------------------------------------------------------------------
# Multilingual intent map
#
# Each entry is a tuple: ([list_of_keyword_patterns], "PROTOCOL:PACKET")
# Patterns are matched case-insensitively via substring search.
# Order from MOST specific to LEAST specific to avoid false positives.
# ---------------------------------------------------------------------------
INTENT_MAP = [
    # ------------------------------------------------------------------
    # App Launch — games (most specific names first)
    # ------------------------------------------------------------------
    (
        ["play minesweeper", "minesweeper", "mines", "माइन्सवीपर"],
        "CMD:LAUNCH:MINES"
    ),
    (
        ["play breakout", "breakout", "brick game", "ब्रेकआउट", "breakout khel"],
        "CMD:LAUNCH:BREAKOUT"
    ),
    (
        ["play snake", "snake game", "snake", "सांप खेळ", "saanp khel"],
        "CMD:LAUNCH:SNAKE"
    ),

    # ------------------------------------------------------------------
    # App Launch — productivity / system apps
    # ------------------------------------------------------------------
    (
        ["task manager", "taskmgr", "processes", "टास्क मॅनेजर"],
        "CMD:LAUNCH:TASKMGR"
    ),
    (
        ["open settings", "settings", "control panel",
         "सेटिंग्स उघड", "settings khol"],
        "CMD:LAUNCH:SETTINGS"
    ),
    (
        ["open clock", "clock", "calendar", "time",
         "घड्याळ दाखव", "time dikhao"],
        "CMD:LAUNCH:CLOCK"
    ),
    (
        ["open calculator", "calculator khol", "calc",
         "कॅल्क्युलेटर उघड", "calculator open kar"],
        "CMD:LAUNCH:CALC"
    ),
    (
        ["open terminal", "terminal khol", "term",
         "टर्मिनल चालू कर", "terminal open karo"],
        "CMD:LAUNCH:TERM"
    ),
    (
        ["file manager", "open files", "files dikhao",
         "explorer", "फाइल्स दाखव", "explorer khol"],
        "CMD:LAUNCH:EXPLORER"
    ),
    (
        ["text editor", "open editor", "notepad",
         "editor khol", "एडिटर उघड"],
        "CMD:LAUNCH:EDITOR"
    ),
    (
        ["open sheets", "spreadsheet", "excel",
         "sheets khol", "शीट्स उघड"],
        "CMD:LAUNCH:SHEETS"
    ),
    (
        ["photo viewer", "open photos", "photos", "images",
         "फोटो दाखव", "photos dikhao"],
        "CMD:LAUNCH:PHOTOS"
    ),
    (
        ["play music", "music player", "player",
         "गाणी लाव", "music chalu kar", "gaane bajao"],
        "CMD:LAUNCH:PLAYER"
    ),

    # ------------------------------------------------------------------
    # Theme commands
    # ------------------------------------------------------------------
    (
        ["catppuccin theme", "catppuccin", "mocha theme"],
        "CMD:THEME:CATPPUCCIN"
    ),
    (
        ["tokyo night", "tokyo theme"],
        "CMD:THEME:TOKYO"
    ),
    (
        ["cyberpunk theme", "cyberpunk", "neon theme",
         "सायबरपंक थीम लाव", "cyberpunk lagao"],
        "CMD:THEME:CYBERPUNK"
    ),
    (
        ["nord theme", "nord", "arctic theme", "नॉर्ड थीम"],
        "CMD:THEME:NORD"
    ),
    (
        ["retro theme", "classic theme", "windows 95", "रेट्रो थीम"],
        "CMD:THEME:RETRO"
    ),

    # ------------------------------------------------------------------
    # System commands
    # ------------------------------------------------------------------
    (
        ["lock screen", "lock", "स्क्रीन लॉक कर", "screen lock karo"],
        "CMD:LOCK"
    ),
    (
        ["how much ram", "ram status", "memory",
         "रॅम किती आहे", "ram kitni hai", "ram dikhao"],
        "QUERY:RAM"
    ),
    (
        ["self destruct", "http_pruthvi command code 0",
         "wipe everything", "सर्व हटवा"],
        "CMD:SELF_DESTRUCT"
    ),
]


# ---------------------------------------------------------------------------
# Helper: coloured status tags
# ---------------------------------------------------------------------------
def tag_connected(msg):
    """Green [CONNECTED] tag."""
    return f"{GREEN}{BOLD}[CONNECTED]{RESET} {msg}"

def tag_sent(msg):
    """Cyan [SENT] tag."""
    return f"{CYAN}[SENT]{RESET} {msg}"

def tag_recv(msg):
    """Magenta [RECV] tag."""
    return f"{MAGENTA}[RECV]{RESET} {msg}"

def tag_error(msg):
    """Red [ERROR] tag."""
    return f"{RED}[ERROR]{RESET} {msg}"

def tag_friday(msg):
    """Cyan bold [FRIDAY] tag."""
    return f"{CYAN}{BOLD}[FRIDAY]{RESET} {msg}"


# ---------------------------------------------------------------------------
# Intent matching
# ---------------------------------------------------------------------------
def match_intent(user_input):
    """
    Map user input to a protocol packet string.

    1. If the input is already a raw protocol command (starts with CMD: or
       QUERY:), return it uppercased as-is.
    2. Otherwise scan INTENT_MAP for the first substring match.
    3. Return None if nothing matches.
    """
    text = user_input.strip().lower()

    # Pass-through raw protocol commands
    if text.startswith(("cmd:", "query:")):
        return text.upper()

    # Scan intent dictionary (order matters — most specific first)
    for patterns, protocol in INTENT_MAP:
        for pattern in patterns:
            if pattern in text:
                return protocol

    return None  # Unknown command


# ---------------------------------------------------------------------------
# Response parser
# ---------------------------------------------------------------------------
def format_response(raw):
    """
    Parse a kernel response packet and return a human-readable string.

    Known formats:
        RESP:RAM:FREE=XXXX:TOTAL=YYYY
        RESP:DESTRUCT:COMPLETE
    Everything else is shown raw.
    """
    raw = raw.strip()

    # --- RAM response ---
    if raw.startswith("RESP:RAM:"):
        try:
            # Example: RESP:RAM:FREE=1234:TOTAL=5678
            parts = raw.split(":")
            free_part  = parts[2]   # FREE=XXXX
            total_part = parts[3]   # TOTAL=YYYY
            free_frames  = int(free_part.split("=")[1])
            total_frames = int(total_part.split("=")[1])
            # Each frame is 4 KB
            free_mb  = (free_frames * 4) // 1024
            total_mb = (total_frames * 4) // 1024
            return (
                f"{CYAN}RAM:{RESET} {free_frames} free / {total_frames} total "
                f"frames ({free_mb} MB free / {total_mb} MB total)"
            )
        except (IndexError, ValueError):
            pass  # Fall through to raw display

    # --- Self-destruct confirmation ---
    if raw.startswith("RESP:DESTRUCT:COMPLETE"):
        return f"{RED}{BOLD}⚠️  Self-destruct sequence completed!{RESET}"

    # --- Unknown / generic ---
    return raw


# ---------------------------------------------------------------------------
# Banner & help
# ---------------------------------------------------------------------------
def print_banner():
    """Print the startup banner."""
    print(f"""
{YELLOW}╔══════════════════════════════════════════════════╗
║  🤖 Friday AI Host Client v1.0                  ║
║  OS: Friday (32-bit x86 Hobby OS)               ║
║  Bridge: COM2 Serial over TCP (115200 baud)      ║
╚══════════════════════════════════════════════════╝{RESET}

{DIM}Commands: Type in English, Marathi, or Hinglish
          Or use raw protocol (e.g., CMD:LAUNCH:CALC)
          Type 'help' for command list, 'quit' to exit{RESET}
""")


def print_help():
    """Display a formatted table of all available commands by category."""
    print(f"""
{YELLOW}{BOLD}┌─────────────────────────────────────────────────────────────────┐
│                    Friday Command Reference                     │
├─────────────────────────────────────────────────────────────────┤{RESET}
{CYAN}{BOLD} 🚀 App Launch:{RESET}
   calc / calculator khol / कॅल्क्युलेटर उघड       → Calculator
   term / terminal khol / टर्मिनल चालू कर           → Terminal
   explorer / files dikhao / फाइल्स दाखव            → File Explorer
   notepad / editor khol / एडिटर उघड                → Text Editor
   spreadsheet / sheets khol / शीट्स उघड            → Spreadsheet
   photos / photos dikhao / फोटो दाखव               → Photo Viewer
   play music / गाणी लाव / gaane bajao               → Music Player
   clock / time dikhao / घड्याळ दाखव                → Clock
   task manager / taskmgr / टास्क मॅनेजर            → Task Manager
   settings / settings khol / सेटिंग्स उघड          → Settings

{CYAN}{BOLD} 🎮 Games:{RESET}
   breakout / brick game / ब्रेकआउट                 → Breakout
   minesweeper / mines / माइन्सवीपर                 → Minesweeper
   snake / saanp khel / सांप खेळ                     → Snake

{CYAN}{BOLD} 🎨 Themes:{RESET}
   catppuccin / mocha theme                          → Catppuccin Mocha
   tokyo night / tokyo theme                         → Tokyo Night
   cyberpunk / neon theme / सायबरपंक थीम लाव        → Cyberpunk
   nord / arctic theme / नॉर्ड थीम                  → Nord
   retro / windows 95 / रेट्रो थीम                  → Retro

{CYAN}{BOLD} 🔧 System:{RESET}
   lock / screen lock karo / स्क्रीन लॉक कर        → Lock Screen
   memory / ram dikhao / रॅम किती आहे               → RAM Status
   self destruct / http_pruthvi command code 0       → ⚠️  Self Destruct

{CYAN}{BOLD} 📡 Raw Protocol:{RESET}
   CMD:LAUNCH:<APP>     → Launch an app directly
   QUERY:<TOPIC>        → Query system information

{CYAN}{BOLD} 🔧 Client:{RESET}
   help                 → This help screen
   quit / exit          → Close Friday client
{YELLOW}{BOLD}└─────────────────────────────────────────────────────────────────┘{RESET}
""")


# ---------------------------------------------------------------------------
# Socket connection with auto-reconnect
# ---------------------------------------------------------------------------
def connect_socket():
    """
    Attempt to connect to the VirtualBox COM2 TCP server.
    Retries every RECONNECT_DELAY seconds until successful.
    Returns the connected socket.
    """
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(SOCKET_TIMEOUT)
            sock.connect((HOST, PORT))
            print(tag_connected(f"Friday Bridge active on {HOST}:{PORT}"))
            return sock
        except (ConnectionRefusedError, socket.timeout, OSError) as exc:
            print(tag_error(
                f"Cannot connect to {HOST}:{PORT} — {exc}. "
                f"Retrying in {RECONNECT_DELAY}s …"
            ))
            time.sleep(RECONNECT_DELAY)


def send_packet(sock, packet):
    """
    Send a protocol packet over the TCP socket.
    The packet is newline-terminated so the kernel UART parser can
    delimit messages.
    Returns True on success, False on failure (caller should reconnect).
    """
    try:
        data = (packet + "\n").encode("utf-8")
        sock.sendall(data)
        print(tag_sent(packet))
        return True
    except (BrokenPipeError, ConnectionResetError, OSError) as exc:
        print(tag_error(f"Send failed — {exc}"))
        return False


def recv_response(sock, timeout=0.5):
    """
    Non-blocking read from the socket.
    Waits up to `timeout` seconds for data.  Returns decoded string or None.
    """
    try:
        ready, _, _ = select.select([sock], [], [], timeout)
        if ready:
            data = sock.recv(RECV_BUFFER)
            if not data:
                # Server closed the connection
                return None
            return data.decode("utf-8", errors="replace")
    except (socket.timeout, OSError):
        pass
    return ""


# ---------------------------------------------------------------------------
# Voice input helper (optional)
# ---------------------------------------------------------------------------
def voice_listen():
    """
    Listen for a single spoken phrase via the default microphone using
    Google's free speech recognition API.
    Returns the recognised text string, or None on failure.
    """
    if not VOICE_AVAILABLE:
        return None

    recognizer = sr.Recognizer()
    try:
        with sr.Microphone() as source:
            print(f"{YELLOW}🎤 Listening …{RESET}", end="", flush=True)
            recognizer.adjust_for_ambient_noise(source, duration=0.3)
            audio = recognizer.listen(source, timeout=5, phrase_time_limit=8)

        print(f"\r{YELLOW}🎤 Recognising …{RESET}  ", end="", flush=True)
        # Try English first, then Marathi
        try:
            text = recognizer.recognize_google(audio, language="en-IN")
        except sr.UnknownValueError:
            try:
                text = recognizer.recognize_google(audio, language="mr-IN")
            except sr.UnknownValueError:
                print(f"\r{RED}🎤 Could not understand audio.{RESET}  ")
                return None

        print(f"\r{GREEN}🎤 Heard:{RESET} {text}                     ")
        return text

    except sr.WaitTimeoutError:
        print(f"\r{DIM}🎤 No speech detected.{RESET}            ")
        return None
    except Exception as exc:
        print(f"\r{RED}🎤 Mic error: {exc}{RESET}")
        return None


# ---------------------------------------------------------------------------
# Self-destruct confirmation gate
# ---------------------------------------------------------------------------
def confirm_self_destruct():
    """
    Require the user to type 'CONFIRM DESTRUCT' before we actually send
    the self-destruct packet.  Returns True if confirmed.
    """
    print(f"\n{RED}{BOLD}⚠️  WARNING: This will wipe all OS data!{RESET}")
    try:
        answer = input(
            f"{RED}Type 'CONFIRM DESTRUCT' to proceed: {RESET}"
        ).strip()
    except (EOFError, KeyboardInterrupt):
        print()
        return False

    if answer == "CONFIRM DESTRUCT":
        return True

    print(f"{YELLOW}Aborted.{RESET}")
    return False


# ---------------------------------------------------------------------------
# Main REPL
# ---------------------------------------------------------------------------
def main():
    """Entry point — parse CLI args, connect, run the REPL."""

    # --- CLI argument handling ---
    voice_mode = "--voice" in sys.argv

    if voice_mode and not VOICE_AVAILABLE:
        print(tag_error(
            "Voice mode requires the SpeechRecognition library.\n"
            "  Install with:  pip install SpeechRecognition sounddevice\n"
            "  Falling back to text mode."
        ))
        voice_mode = False

    # --- Startup banner ---
    print_banner()

    if voice_mode:
        print(f"{GREEN}{BOLD}🎤 Voice mode ENABLED{RESET} "
              f"— speak or type commands.\n")

    # --- Connect to VirtualBox COM2 TCP server ---
    sock = connect_socket()

    # --- REPL loop ---
    try:
        while True:
            # ---- Get user input (voice or text) ----
            user_input = None

            if voice_mode:
                user_input = voice_listen()

            if user_input is None:
                # Fall back to (or use) keyboard input
                try:
                    user_input = input(
                        f"{CYAN}{BOLD}Friday>{RESET} "
                    )
                except EOFError:
                    # stdin closed — treat as quit
                    print()
                    break

            user_input = user_input.strip()
            if not user_input:
                continue

            # ---- Built-in client commands ----
            lower = user_input.lower()

            if lower in ("quit", "exit"):
                print(f"{YELLOW}Goodbye! 👋{RESET}")
                break

            if lower == "help":
                print_help()
                continue

            # ---- Match intent ----
            protocol = match_intent(user_input)

            if protocol is None:
                print(tag_friday(
                    f"{YELLOW}Unknown command:{RESET} \"{user_input}\"\n"
                    f"       Type {BOLD}help{RESET} for a list of commands, "
                    f"or send a raw protocol packet."
                ))
                continue

            # ---- Self-destruct safety gate ----
            if protocol == "CMD:SELF_DESTRUCT":
                if not confirm_self_destruct():
                    continue

            # ---- Send packet ----
            ok = send_packet(sock, protocol)
            if not ok:
                # Connection lost — reconnect and retry once
                print(tag_error("Reconnecting …"))
                try:
                    sock.close()
                except OSError:
                    pass
                sock = connect_socket()
                if not send_packet(sock, protocol):
                    print(tag_error("Send failed after reconnect. Skipping."))
                    continue

            # ---- Read response (wait a bit for the kernel to reply) ----
            response = recv_response(sock, timeout=0.5)

            if response is None:
                # Server closed connection
                print(tag_error("Connection closed by server. Reconnecting …"))
                try:
                    sock.close()
                except OSError:
                    pass
                sock = connect_socket()
            elif response:
                # May contain multiple newline-delimited responses
                for line in response.strip().splitlines():
                    line = line.strip()
                    if line:
                        formatted = format_response(line)
                        print(tag_recv(formatted))

    except KeyboardInterrupt:
        print(f"\n{YELLOW}Interrupted — shutting down.{RESET}")

    finally:
        try:
            sock.close()
        except OSError:
            pass
        print(f"{DIM}Socket closed. Friday Bridge offline.{RESET}")


# ---------------------------------------------------------------------------
if __name__ == "__main__":
    main()
