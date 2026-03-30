import pygame
import serial
import threading
import time

# --- CONFIGURATION ---
SERIAL_PORT = 'COM7' 
BAUD_RATE = 115200
WIN_W, WIN_H = 1920, 1080 # Expanded width for new panels

# Colors
WHITE, BLACK = (255, 255, 255), (15, 15, 15)
GREEN, RED = (0, 255, 0), (255, 50, 50)
BLUE, LIGHT_GRAY = (100, 200, 255), (200, 200, 200)
DARK_BLUE = (0, 30, 70)

pygame.init()
screen = pygame.display.set_mode((WIN_W, WIN_H))
pygame.display.set_caption("MCU Bitstream Controller (WASDE)")

font = pygame.font.SysFont("Consolas", 18)
large_font = pygame.font.SysFont("Consolas", 32, bold=True)
bit_font = pygame.font.SysFont("Consolas", 40, bold=True)

# State Variables
enable_toggle = 0
mcu_feedback = "MCU Upload Only Mode - No RX Data expected."
ser = None
is_connected = False
key_states = {'w': 0, 'a': 0, 's': 0, 'd': 0}

def get_bit_string():
    return f"{key_states['w']}{key_states['a']}{key_states['s']}{key_states['d']}{enable_toggle}"

def get_drive_state():
    """Calculates drive state based on Python-side logic."""
    if not enable_toggle:
        return "STOPPED"
    
    if key_states['w'] and not key_states['s']:
        return "FORWARD"
    elif key_states['s'] and not key_states['w']:
        return "REVERSE"
    elif key_states['a'] or key_states['d']:
        return "TURNING"
    else:
        return "STOPPED"

def send_command():
    global is_connected
    if ser and ser.is_open:
        try:
            cmd = get_bit_string() + '\n'
            ser.write(cmd.encode())
            is_connected = True
        except serial.SerialException:
            is_connected = False

def serial_monitor():
    global ser, is_connected, mcu_feedback
    while True:
        try:
            if ser is None or not ser.is_open:
                ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
                is_connected = True
                mcu_feedback = f"Connected to {SERIAL_PORT}."
        except Exception:
            is_connected = False
            mcu_feedback = f"Searching for {SERIAL_PORT}..."
            ser = None
        time.sleep(1)

threading.Thread(target=serial_monitor, daemon=True).start()

def draw_text(text, x, y, color=WHITE, center=False, use_large=False):
    f = large_font if use_large else font
    surf = f.render(text, True, color)
    rect = surf.get_rect(center=(x, y)) if center else surf.get_rect(topleft=(x, y))
    screen.blit(surf, rect)

def draw_bit_box(label, bit_val, x, y, size=70):
    color = GREEN if bit_val == 1 else LIGHT_GRAY
    rect = (x, y, size, size)
    pygame.draw.rect(screen, color, rect, 2 if bit_val == 0 else 0, 8)
    b_txt = bit_font.render(str(bit_val), True, color)
    screen.blit(b_txt, (x + size//2 - 12, y + size//2 - 20))
    draw_text(label, x + size//2 - 8, y + size + 10, color)

running = True
while running:
    screen.fill(BLACK)
    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        if event.type == pygame.KEYDOWN:
            k = event.unicode.lower()
            if k in key_states:
                key_states[k] = 1
                send_command()
            elif k == 'e':
                enable_toggle = 1 if enable_toggle == 0 else 0
                send_command()
        if event.type == pygame.KEYUP:
            k = event.unicode.lower()
            if k in key_states:
                key_states[k] = 0
                send_command()

    # 1. Header Row (Enable Status & Drive State)
    # Enable Panel
    status_color = GREEN if enable_toggle else RED
    pygame.draw.rect(screen, DARK_BLUE, (50, 30, 430, 80), 0, 15)
    pygame.draw.rect(screen, status_color, (50, 30, 430, 80), 3, 15)
    draw_text("SYSTEM ENABLED" if enable_toggle else "SYSTEM DISABLED", 265, 70, status_color, center=True, use_large=True)

    # Drive State Panel (Manual Mode Info)
    drive_str = get_drive_state()
    pygame.draw.rect(screen, DARK_BLUE, (500, 30, 400, 80), 0, 15)
    pygame.draw.rect(screen, BLUE, (500, 30, 400, 80), 2, 15)
    draw_text("MANUAL MODE", 700, 50, LIGHT_GRAY, center=True)
    draw_text(f"DRIVE STATE: {drive_str}", 700, 85, WHITE if drive_str == "STOPPED" else GREEN, center=True)

    # 2. Command Layout
    draw_text("COMMAND LAYOUT:", 100, 150, LIGHT_GRAY)
    base_x, base_y = 150, 190
    draw_bit_box("W", key_states['w'], base_x + 85, base_y)
    draw_bit_box("A", key_states['a'], base_x, base_y + 85)
    draw_bit_box("S", key_states['s'], base_x + 85, base_y + 85)
    draw_bit_box("D", key_states['d'], base_x + 170, base_y + 85)
    draw_bit_box("ENABLE", enable_toggle, base_x + 350, base_y + 42, size=90)

    # 3. Connection Heartbeat
    hb_color = GREEN if is_connected else RED
    hb_text = "SERIAL ACTIVE" if is_connected else "SERIAL DISCONNECTED"
    draw_text("CABLE CONNECTION:", 100, 430, LIGHT_GRAY)
    pygame.draw.circle(screen, hb_color, (300, 440), 8)
    draw_text(hb_text, 320, 430, hb_color)

    # 4. Status Log
    draw_text("STATUS LOG:", 100, 470, LIGHT_GRAY)
    term_rect = (100, 500, 750, 100) # Widened log box
    pygame.draw.rect(screen, (10, 10, 10), term_rect, 0, 5)
    pygame.draw.rect(screen, LIGHT_GRAY, term_rect, 1, 5)
    draw_text(mcu_feedback, 115, 515, GREEN)

    # 5. Footer
    draw_text(f"BITSTREAM: {get_bit_string()}", 475, 650, BLUE, center=True, use_large=True)
    draw_text("WASD: Momentary | E: Latched Toggle", 475, 690, LIGHT_GRAY, center=True)

    pygame.display.flip()
pygame.quit()