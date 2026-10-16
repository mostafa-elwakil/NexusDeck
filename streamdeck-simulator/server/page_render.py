"""Render NexusDeck profiles to 320x240 JPEG pages (OpenDeck-style graphics).

The ESP32 displays the finished image instead of drawing primitives, so the
web Studio RtWYSIWYG matches the device pixel-geometry. Layout constants
mirror esp32-firmware/src/main.cpp exactly (do not drift!).
"""
import io

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover
    Image = None

SCREEN_W, SCREEN_H = 320, 240
STATUS_H = 18
GRID_COLS, GRID_ROWS = 4, 3
PADDING = 4
BUTTON_W = (SCREEN_W - PADDING * (GRID_COLS + 1)) // GRID_COLS
BUTTON_H = ((SCREEN_H - STATUS_H) - PADDING * (GRID_ROWS + 1)) // GRID_ROWS

# Emoji have no DejaVu glyphs: map to the same short labels the firmware uses.
ICON_TEXT = {
    '▶️': '>', '⏸️': '||', '⏹️': '[]', '⏺️': 'REC', '⏯️': 'TOG',
    '🎥': 'OBS', '🎮': 'GAME', '💬': 'CHAT', '🖥️': 'DESK', '📷': 'CAM',
    '🚫': 'OFF', '📧': 'MAIL', '📅': 'CAL', '📄': 'DOC', '👥': 'TEAM',
    '📸': 'SNAP', '🔢': 'NUM', '⚙️': 'SET', '🎵': 'MUSIC', '📺': 'TV',
    '📝': 'NOTE', '💻': 'PC', '⚡': 'CMD', '🐳': 'DOCKER', '🐙': 'GH',
    '🌐': 'WEB', '📡': 'PING', '📊': 'STAT', '🔄': 'SYNC', '📋': 'LIST',
    '🗑️': 'DEL', '🔥': 'CPU', '💾': 'RAM', '⏲️': 'TMR', '⏱️': 'SW',
    '🕐': 'TIME', '🍅': 'POMO', '💡': 'LIGHT', '🏠': 'HOME', '🔌': 'PLUG',
    '🌡️': 'TEMP', '🔒': 'LOCK', '🔓': 'OPEN', '⏰': 'ALARM', '🔋': 'BATT',
    '📶': 'WIFI', '⌨️': 'KEYS', '🎹': 'PIANO', '🔍': 'FIND', '📌': 'PIN',
    '⭐': 'STAR', '✅': 'DONE', '❌': 'NO', '➕': 'ADD', '🔔': 'BELL',
    '🐛': 'BUG', '🔧': 'TOOL', '📦': 'BOX', '🚀': 'GO', '🧪': 'TEST',
    '🏷️': 'TAG', '🔗': 'LINK', '📈': 'UP', '📉': 'DOWN', '🚗': 'CAR',
    '🛒': 'CART', '💰': 'CASH', '🎲': 'DICE', '🔴': 'LIVE', '🟢': 'ON',
    '🎬': 'SCENE', '🎧': 'AUDIO', '🎤': 'MIC', '🔊': 'VOL', '🔇': 'MUTE',
}


def _font(size):
    import os
    candidates = [
        os.path.join(os.path.dirname(ImageFont.__file__), 'DejaVuSans-Bold.ttf'
                     if size >= 16 else 'DejaVuSans.ttf'),
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _shape_arabic(text):
    try:
        import arabic_reshaper
        from bidi.algorithm import get_display
        return get_display(arabic_reshaper.reshape(text))
    except Exception:
        return text


def _parse_color(value, fallback=(26, 26, 46)):
    try:
        text = str(value or '').strip().lstrip('#')
        if len(text) == 6:
            return tuple(int(text[i:i + 2], 16) for i in (0, 2, 4))
    except (ValueError, TypeError):
        pass
    return fallback


def _center_text(draw, cx, y, text, font, fill):
    text = _shape_arabic(text or '')
    if not text:
        return
    bbox = draw.textbbox((0, 0), text, font=font)
    draw.text((cx - (bbox[2] - bbox[0]) / 2, y), text, font=font, fill=fill)


def render_profile_page(profile, quality=75):
    """Render a profile dict to JPEG bytes (320x240)."""
    if Image is None:
        raise RuntimeError('Pillow is not installed')
    profile = profile or {}
    buttons = profile.get('buttons') or []
    image = Image.new('RGB', (SCREEN_W, SCREEN_H),
                      _parse_color(profile.get('backgroundColor')))
    draw = ImageDraw.Draw(image)
    font_big = _font(16)
    font_small = _font(13)

    # Status bar
    draw.text((28, 2), _shape_arabic(profile.get('name', '')),
              font=font_small, fill=(255, 255, 255))
    draw.text((SCREEN_W - 52, 2), '< HOME', font=font_small,
              fill=(0, 255, 255))

    for index in range(GRID_COLS * GRID_ROWS):
        col, row = index % GRID_COLS, index // GRID_COLS
        x = PADDING + col * (BUTTON_W + PADDING)
        y = STATUS_H + PADDING + row * (BUTTON_H + PADDING)
        btn = buttons[index] if index < len(buttons) else {}
        color = _parse_color((btn or {}).get('color'), (26, 26, 46))
        draw.rounded_rectangle([x, y, x + BUTTON_W, y + BUTTON_H],
                               radius=6, fill=color, outline=(255, 255, 255))
        action = ((btn or {}).get('action') or {})
        icon = ((btn or {}).get('icon') or '').strip()
        label = ((btn or {}).get('label') or '').strip()
        if action.get('type') == 'switch_profile' and not label:
            label = action.get('name', '')
        icon_text = ICON_TEXT.get(icon, icon[:8])
        if icon_text:
            _center_text(draw, x + BUTTON_W / 2, y + 6, icon_text,
                         font_big, (255, 255, 255))
        if label:
            _center_text(draw, x + BUTTON_W / 2, y + BUTTON_H - 30, label,
                         font_small, (255, 255, 255))

    buffer = io.BytesIO()
    image.save(buffer, format='JPEG', quality=quality)
    return buffer.getvalue()
