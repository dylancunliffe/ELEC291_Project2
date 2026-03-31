import pygame
import serial
import threading
import time
import math

# --- CONFIGURATION ---
SERIAL_PORT = 'COM8' 
BAUD_RATE = 115200
WIN_W, WIN_H = 1350, 750

# Colors
WHITE, BLACK = (255, 255, 255), (15, 15, 15)
GREEN, RED, YELLOW = (0, 255, 0), (255, 50, 50), (255, 255, 0)
BLUE, LIGHT_GRAY = (100, 200, 200), (200, 200, 200)
DARK_BLUE, TRON_CYAN = (0, 30, 70), (0, 255, 255)
GRAY = (100, 100, 100)

pygame.init()
screen = pygame.display.set_mode((WIN_W, WIN_H))
pygame.display.set_caption("MCU Bitstream Controller | Silent Sandbox Recall")

font = pygame.font.SysFont("Consolas", 16)
large_font = pygame.font.SysFont("Consolas", 32, bold=True)
bit_font = pygame.font.SysFont("Consolas", 32, bold=True)
label_font = pygame.font.SysFont("Consolas", 14, bold=True)

# State Variables
eco_toggle = 0
system_mode = 0  # 0: Disabled, 1: Enabled, 2: Recall
rot_toggle = 0
led_toggle = 0
ser = None
is_connected = False
key_states = {'w': 0, 'a': 0, 's': 0, 'd': 0}
mcu_feedback = "Searching for MCU..."
last_sent_bits = "00000000" # This is the REAL output to hardware

# Recall & Recording Logic
recorded_path = [] 
is_replaying = False
is_recording = False 

# Tron Physics Variables
tron_surf = pygame.Surface((400, 400))
tron_surf.fill((5, 5, 5))
car_x, car_y = 200.0, 200.0
car_angle = -90.0  
turn_speed = 1.5    
base_velocity = 0.25 

def get_bit_list():
    # M bit is 0 if Disabled, 1 if Enabled or Recall
    m_bit = 1 if system_mode > 0 else 0
    return [
        ('W', key_states['w']), ('A', key_states['a']), 
        ('S', key_states['s']), ('D', key_states['d']),
        ('E', eco_toggle), ('M', m_bit), 
        ('R', rot_toggle), ('L', led_toggle)
    ]

def get_bit_string():
    return "".join(str(bit[1]) for bit in get_bit_list())

def send_command(manual_cmd=None):
    global is_connected, last_sent_bits
    cmd_str = manual_cmd if manual_cmd else get_bit_string()
    
    # Update the LIVE output variable
    last_sent_bits = cmd_str 
    
    if ser and ser.is_open:
        try:
            ser.write((cmd_str + '\n').encode())
            is_connected = True
        except: is_connected = False

def run_replay():
    global is_replaying, is_recording
    if not recorded_path: return
    is_recording = False 
    is_replaying = True
    for cmd in recorded_path:
        send_command(manual_cmd=cmd)
        time.sleep(0.016) 
    # Return to silent state after replay
    is_replaying = False

def serial_monitor():
    global ser, is_connected, mcu_feedback
    while True:
        try:
            if ser is None or not ser.is_open:
                ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
                is_connected = True
                mcu_feedback = f"Connected to {SERIAL_PORT}."
        except:
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

def draw_key(label, bit_val, x, y, size=55, custom_color=None):
    color = custom_color if custom_color else (GREEN if bit_val == 1 else LIGHT_GRAY)
    pygame.draw.rect(screen, color, (x, y, size, size), 2 if bit_val == 0 else 0, 6)
    b_txt = bit_font.render(str(bit_val), True, BLACK if bit_val == 1 else color)
    screen.blit(b_txt, (x + size//2 - 10, y + size//2 - 15))
    draw_text(label, x + size//2, y + size + 12, color, center=True)

def draw_pot(label, val, x, y, vertical=True):
    w, h = (20, 150) if vertical else (150, 20)
    pygame.draw.rect(screen, LIGHT_GRAY, (x, y, w, h), 1)
    if vertical:
        pos = y + 75 - (val * 75 // 127)
        pygame.draw.circle(screen, BLUE, (x + 10, int(pos)), 8)
    else:
        pos = x + 75 + (val * 75 // 127)
        pygame.draw.circle(screen, BLUE, (int(pos), y + 10), 8)
    draw_text(f"{label}: {val}", x - 20 if vertical else x + 35, y + h + 10, WHITE)

def update_tron_physics():
    global car_x, car_y, car_angle
    if system_mode != 2 and not is_replaying: return
    
    eco_mod = 0.5 if eco_toggle else 1.0
    current_turn = turn_speed * eco_mod
    current_vel = base_velocity * eco_mod
    old_x, old_y = car_x, car_y

    if rot_toggle:
        car_angle += current_turn * 1.5
    else:
        if key_states['a']: car_angle -= current_turn
        if key_states['d']: car_angle += current_turn
        rad = math.radians(car_angle)
        if key_states['w']:
            car_x += math.cos(rad) * current_vel
            car_y += math.sin(rad) * current_vel
        if key_states['s']:
            car_x -= math.cos(rad) * current_vel
            car_y -= math.sin(rad) * current_vel
    
    car_x %= 400
    car_y %= 400
    
    if not rot_toggle and (key_states['w'] or key_states['s']):
        if abs(car_x - old_x) < 50 and abs(car_y - old_y) < 50:
            pygame.draw.line(tron_surf, TRON_CYAN, (old_x, old_y), (car_x, car_y), 2)

running = True
clock = pygame.time.Clock()

while running:
    screen.fill(BLACK)
    
    # This bitstring is for visualization (what's happening in the UI)
    ui_bits = get_bit_string() if system_mode > 0 else "00000000"
    
    # SILENCE LOGIC: If we are in Recall mode and NOT replaying, 
    # the LIVE hardware output must stay locked at all 0s.
    if not is_replaying and (system_mode == 2 or system_mode == 0):
        last_sent_bits = "00000000"
    elif system_mode == 1 and not is_replaying:
        last_sent_bits = ui_bits

    max_pwr = 64 if eco_toggle else 127
    v_val = max_pwr if key_states['w'] else -max_pwr if key_states['s'] else 0
    h_val = -max_pwr if key_states['a'] else max_pwr if key_states['d'] else 0

    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        if event.type == pygame.KEYDOWN:
            k = event.unicode.lower()
            if k in key_states: 
                key_states[k] = 1
                if system_mode == 1: send_command()
            elif k == 'e': 
                eco_toggle = 1 - eco_toggle
                if system_mode == 1: send_command()
            elif k == 'm': 
                system_mode = (system_mode + 1) % 3
                if system_mode != 2: is_recording = False 
                if system_mode == 1: send_command()
            elif k == 'r': 
                rot_toggle = 1 - rot_toggle
                if system_mode == 1: send_command()
            elif k == 'l': 
                led_toggle = 1 - led_toggle
                if system_mode == 1: send_command()
            elif k == 'b' and system_mode == 2:
                is_recording = not is_recording
            elif k == 'n' and system_mode == 2 and not is_replaying:
                threading.Thread(target=run_replay, daemon=True).start()
            elif k == 'q':
                tron_surf.fill((5, 5, 5))
                car_x, car_y, car_angle = 200.0, 200.0, -90.0
                recorded_path = []
                is_recording = False
                last_sent_bits = "00000000"

        if event.type == pygame.KEYUP:
            k = event.unicode.lower()
            if k in key_states: 
                key_states[k] = 0
                if system_mode == 1: send_command()

    # BUFFERING: Store the UI intended bits while drawing
    if system_mode == 2 and is_recording and not is_replaying:
        recorded_path.append(ui_bits)

    update_tron_physics()

    # --- UI RENDERING ---
    mode_names = ["DISABLED", "ENABLED", "RECALL"]
    mode_colors = [RED, GREEN, YELLOW]
    pygame.draw.rect(screen, DARK_BLUE, (50, 30, 430, 80), 0, 15)
    pygame.draw.rect(screen, mode_colors[system_mode], (50, 30, 430, 80), 3, 15)
    draw_text(f"SYSTEM {mode_names[system_mode]}", 265, 70, mode_colors[system_mode], center=True, use_large=True)

    pygame.draw.rect(screen, DARK_BLUE, (500, 30, 400, 80), 0, 15)
    pygame.draw.rect(screen, BLUE, (500, 30, 400, 80), 2, 15)
    d_state = "REPLAYING" if is_replaying else ("RECORDING" if is_recording else ("ACTIVE" if system_mode == 1 else "STOPPED"))
    draw_text(f"DRIVE: {d_state}", 700, 70, YELLOW if is_replaying or is_recording else GREEN, center=True, use_large=True)

    bx, by = 80, 160
    draw_key("W", key_states['w'], bx + 65, by)
    draw_key("A", key_states['a'], bx, by + 80)
    draw_key("S", key_states['s'], bx + 65, by + 80)
    draw_key("D", key_states['d'], bx + 130, by + 80)
    draw_key("ECO(E)", eco_toggle, bx + 210, by)
    
    en_col = YELLOW if system_mode == 2 else (GREEN if system_mode == 1 else LIGHT_GRAY)
    draw_key("EN(M)", 1 if system_mode > 0 else 0, bx + 210, by + 80, custom_color=en_col)
    
    draw_key("ROT(R)", rot_toggle, bx + 285, by)
    draw_key("LED(L)", led_toggle, bx + 285, by + 80)

    draw_text("JOYSTICK POTS:", 480, 140, LIGHT_GRAY)
    draw_pot("V-POT", v_val, 500, 180, vertical=True)
    draw_pot("H-POT", h_val, 620, 245, vertical=False)

    draw_text("CONNECTION:", 80, 430, LIGHT_GRAY)
    pygame.draw.circle(screen, GREEN if is_connected else RED, (195, 437), 8)
    draw_text("ACTIVE" if is_connected else "DISCONNECTED", 210, 430, GREEN if is_connected else RED)
    
    draw_text("STATUS LOG:", 80, 470, LIGHT_GRAY)
    pygame.draw.rect(screen, (10, 10, 10), (80, 500, 750, 60), 0, 5)
    pygame.draw.rect(screen, LIGHT_GRAY, (80, 500, 750, 60), 1, 5)
    log_msg = mcu_feedback if not is_replaying else "STREAMING BUFFER TO MCU..."
    draw_text(log_msg, 100, 518, YELLOW if is_replaying else GREEN)

    tx, ty = 880, 180
    draw_text("CAR MOVEMENT TRACKER:", tx, 140, LIGHT_GRAY)
    tracker_border = TRON_CYAN if system_mode == 2 or is_replaying else GRAY
    pygame.draw.rect(screen, tracker_border, (tx-5, ty-5, 410, 410), 2, 5)
    
    if system_mode == 2 or is_replaying:
        screen.blit(tron_surf, (tx, ty))
        rad = math.radians(car_angle)
        car_pts = [(tx + car_x + (math.cos(rad)*12 - math.sin(rad)*7), ty + car_y + (math.sin(rad)*12 + math.cos(rad)*7)),
                   (tx + car_x + (math.cos(rad)*12 + math.sin(rad)*7), ty + car_y + (math.sin(rad)*12 - math.cos(rad)*7)),
                   (tx + car_x + (-math.cos(rad)*12 + math.sin(rad)*7), ty + car_y + (-math.sin(rad)*12 - math.cos(rad)*7)),
                   (tx + car_x + (-math.cos(rad)*12 - math.sin(rad)*7), ty + car_y + (-math.sin(rad)*12 + math.cos(rad)*7))]
        pygame.draw.polygon(screen, GRAY, car_pts)
        pygame.draw.polygon(screen, WHITE, car_pts, 1)
        pygame.draw.line(screen, RED, (tx+car_x, ty+car_y), (tx+car_x - math.cos(rad)*15, ty+car_y - math.sin(rad)*15), 3)

    buffer_color = RED if is_recording else (YELLOW if system_mode == 2 else WHITE)
    draw_text(f"BUFFER SIZE: {len(recorded_path)} steps", tx, ty + 420, buffer_color)
    
    # VISUAL PROOF: LIVE OUTPUT vs UI INTENT
    # During drawing in Recall, LIVE will be all 0s. During Replay, it will change.
    draw_text(f"LIVE HW OUTPUT: {last_sent_bits}", tx, ty + 445, TRON_CYAN)
    
    if system_mode == 2:
        draw_text("[B] TOGGLE RECORD | [N] REPLAY | [Q] RESET", tx, ty + 475, LIGHT_GRAY)

    draw_text("BITSTREAM DECODER (INTENDED):", 80, 580, LIGHT_GRAY)
    
    # Render decoder based on ui_bits (shows what you ARE DRAWING)
    for i, (label, _) in enumerate(get_bit_list()):
        val = int(ui_bits[i])
        x_p, y_p = 100 + (i * 90), 620
        c = GREEN if val else LIGHT_GRAY
        pygame.draw.rect(screen, c, (x_p, y_p, 40, 40), 0 if val else 1, 4)
        screen.blit(bit_font.render(str(val), True, BLACK if val else WHITE), (x_p+10, y_p+2))
        screen.blit(label_font.render(label, True, c), (x_p+5, y_p+45))

    pygame.display.flip()
    clock.tick(60)

pygame.quit()