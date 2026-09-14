"""
Button Widget for StreamDeck
Simple button with touch detection and visual feedback
"""

class Button:
    """Interactive button widget"""

    def __init__(self, x, y, width, height, text="", icon="",
                 bg_color=0x2196F3, text_color=0xFFFF,
                 action_type=None, action_data=None):
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.text = text
        self.icon = icon
        self.bg_color = bg_color
        self.text_color = text_color
        self.action_type = action_type
        self.action_data = action_data
        self.pressed = False

    def contains_point(self, px, py):
        """Check if point is inside button"""
        return (self.x <= px < self.x + self.width and
                self.y <= py < self.y + self.height)

    def draw(self, display):
        """Draw button on display"""
        # Background
        color = self.darken_color(self.bg_color) if self.pressed else self.bg_color
        display.fill_rect(self.x, self.y, self.width, self.height, color)

        # Border
        display.rect(self.x, self.y, self.width, self.height, 0xFFFF)

        # Icon (centered, simplified)
        if self.icon:
            icon_x = self.x + (self.width - len(self.icon) * 8) // 2
            icon_y = self.y + self.height // 3
            display.text(self.icon, icon_x, icon_y, self.text_color)

        # Text (centered below icon)
        if self.text:
            # Simple centering for short text
            text_x = self.x + (self.width - len(self.text) * 8) // 2
            text_y = self.y + 2 * self.height // 3
            display.text(self.text, text_x, text_y, self.text_color)

    def on_press(self):
        """Handle button press"""
        self.pressed = True

    def on_release(self):
        """Handle button release"""
        self.pressed = False
        # Return action data if available
        if self.action_type:
            return {
                "type": self.action_type,
                "data": self.action_data
            }
        return None

    @staticmethod
    def darken_color(color):
        """Darken RGB565 color by ~30%"""
        r = (color >> 11) & 0x1F
        g = (color >> 5) & 0x3F
        b = color & 0x1F

        r = int(r * 0.7)
        g = int(g * 0.7)
        b = int(b * 0.7)

        return (r << 11) | (g << 5) | b
