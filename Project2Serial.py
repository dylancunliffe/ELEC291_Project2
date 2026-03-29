import pygame
import serial
import threading
import sys

# --- CONFIGURATION ---
SERIAL_PORT = 'COM8' 
BAUD_RATE = 115200

# Colors
WHITE, BLACK = (255, 255, 255), (20, 20, 20)
GREEN, RED = (0, 255, 0), (255, 0, 0)
BLUE, GRAY = (0, 150, 255), (60, 60, 60)
YELLOW, DARK_BLUE = (255, 255, 0), (0, 50, 100)

pygame.init()
screen = pygame.display.set_mode((900, 800)) # Extra height for the new layout
pygame.display.set_caption("EFM8 Telemetry - Reorganized v4")

font = pygame.font.SysFont("Consolas", 18)
header_font = pygame.font.SysFont("Consolas", 26, bold=True)
mode_font = pygame.font.SysFont("Consolas", 22, bold=True)

# Shared State - EXACT ORDER of your printf
telemetry = {
    "drive": 0, "eco_m": 0, "ai_m": 0, "rot_m": 0, "car_hb": 0, "ctrl_hb": 0, 
    "spk": 0, "inter": 0, "b_eco": 0, "b_ai": 0, "b_rot": 0, "b_stop": 0,
    "b_up": 0, "b_down": 0, "b_left": 0, "b_right": 0, 
    "vert": 0, "horz": 0, "tof": 0, 
    "ind1": 0.0, "ind2": 0.0, "ind3": 0.0
}

computer_mode = False
last_key = "NONE"
ser = None

def serial_handler():
    global ser
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line.startswith('$'):
                    parts = line[1:].split(',')
                    if len(parts) >= 22:
                        keys = list(telemetry.keys())
                        for i in range(len(keys)):
                            val = parts[i]
                            # Indices 0-18 are ints (Drive to TOF), 19-21 are floats (Inductors)
                            telemetry[keys[i]] = float(val) if i >= 19 else int(val)
    except Exception as e: print(f"Serial Error: {e}")

threading.Thread(target=serial_handler, daemon=True).start()

def render_text(text, x, y, color=WHITE, header=False):
    f = header_font if header else font
    screen.blit(f.render(text, True, color), (x, y))

def draw_centered_pot(name, val, x, y, vertical=True):
    w, h = (30, 150) if vertical else (150, 30)
    pygame.draw.rect(screen, GRAY, (x, y, w, h), 2)
    mid_x, mid_y = x + w/2, y + h/2
    offset = (val / 128) * (h/2 if vertical else w/2)
    pos = (int(mid_x), int(mid_y - offset)) if vertical else (int(mid_x + offset), int(mid_y))
    pygame.draw.circle(screen, BLUE, pos, 8)
    render_text(f"{name}: {int(val)}", x - 10, y + h + 10 if vertical else y + h + 5)

running = True
while running:
    screen.fill(BLACK)
    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        if event.type == pygame.KEYDOWN:
            key_char = event.unicode.lower()
            if key_char in ['w', 'a', 's', 'd', 'm']:
                last_key = key_char.upper()
                if key_char == 'm': computer_mode = not computer_mode
                if ser and ser.is_open: ser.write(key_char.encode())

    # --- 1. HEADER ---
    render_text("CAR TELEMETRY SYSTEM", 280, 20, WHITE, header=True)
    pygame.draw.line(screen, BLUE, (50, 60), (850, 60), 2)
    
    # --- 2. TOP LEFT: MODES & SPEAKER ---
    lx = 50
    render_text(f"Drive: {['FWD', 'REV', 'TRN', 'STP'][telemetry['drive']%4]}", lx, 90)
    render_text(f"ECO Mode: {'ACTIVE' if telemetry['eco_m'] else 'OFF'}", lx, 120, GREEN if telemetry['eco_m'] else WHITE)
    render_text(f"AI Mode:  {'ACTIVE' if telemetry['ai_m'] else 'OFF'}", lx, 150, GREEN if telemetry['ai_m'] else WHITE)
    render_text(f"ROT Mode: {'ACTIVE' if telemetry['rot_m'] else 'OFF'}", lx, 180, GREEN if telemetry['rot_m'] else WHITE)
    render_text(f"Speaker:  {'ON' if telemetry['spk'] else 'OFF'}", lx, 210, YELLOW if telemetry['spk'] else WHITE)

    # --- 3. TOP RIGHT: SENSORS, INTERSECTION, HB ---
    rx = 450
    render_text(f"TOF Range: {telemetry['tof']}mm", rx, 90)
    render_text(f"Inductors: {telemetry['ind1']:.2f}V | {telemetry['ind2']:.2f}V | {telemetry['ind3']:.2f}V", rx, 120, YELLOW)
    render_text(f"Intersection: {'YES' if telemetry['inter'] else 'NO'}", rx, 150, GREEN if telemetry['inter'] else WHITE)
    
    # Heartbeats
    render_text("Car Heartbeat:", rx, 185); pygame.draw.circle(screen, GREEN if telemetry['car_hb'] else RED, (rx + 160, 195), 8)
    render_text("Ctrl Heartbeat:", rx, 215); pygame.draw.circle(screen, GREEN if telemetry['ctrl_hb'] else RED, (rx + 160, 225), 8)

    # --- 4. BUTTON GROUPS (BELOW TOP SECTIONS) ---
    # Left Side: Mode/Stop Buttons (under Speaker)
    render_text("MODE / STOP BUTTONS", 50, 280, GRAY)
    mode_btns = [("ECO", "b_eco"), ("AI", "b_ai"), ("ROT", "b_rot"), ("STOP", "b_stop")]
    for i, (label, key) in enumerate(mode_btns):
        btn_color = (RED if telemetry[key] else GRAY) if label == "STOP" else (GREEN if telemetry[key] else GRAY)
        pygame.draw.rect(screen, btn_color, (50 + (i*100), 310, 80, 40), 2 if not telemetry[key] else 0)
        render_text(label, 65 + (i*100), 320, WHITE if telemetry[key] else GRAY)

    # Right Side: Direction Buttons (under Heartbeats)
    render_text("DIRECTIONAL BUTTONS", 450, 280, GRAY)
    dir_btns = [("UP", "b_up"), ("DOWN", "b_down"), ("LEFT", "b_left"), ("RIGHT", "b_right")]
    for i, (label, key) in enumerate(dir_btns):
        color = GREEN if telemetry[key] else GRAY
        pygame.draw.rect(screen, color, (450 + (i*100), 310, 80, 40), 2 if not telemetry[key] else 0)
        render_text(label, 465 + (i*100), 320, WHITE if telemetry[key] else GRAY)

    # --- 5. VISUALS (POTS & COMPUTER MODE) ---
    draw_centered_pot("V-POT", telemetry['vert'], 100, 500, vertical=True)
    draw_centered_pot("H-POT", telemetry['horz'], 250, 560, vertical=False)

    # Command Monitor
    render_text(f"LAST COMMAND: {last_key}", 600, 510, YELLOW)
    mode_color = GREEN if computer_mode else RED
    pygame.draw.rect(screen, DARK_BLUE, (600, 550, 250, 120), 0, 10)
    pygame.draw.rect(screen, mode_color, (600, 550, 250, 120), 3, 10)
    screen.blit(mode_font.render("COMPUTER MODE", True, mode_color), (635, 570))
    render_text("Type: W, A, S, D, M", 640, 610, GRAY)
    if computer_mode: render_text("SENDING ACTIVE", 660, 640, GREEN)

    pygame.display.flip()

pygame.quit()