"""
StreamDeck Main Logic
Manages buttons, server sync, and user interaction
"""

import time
import gc
from lib.button import Button
from lib.utils import http_get, http_post


class StreamDeck:
    """Main StreamDeck controller"""

    def __init__(self, display, touch, server_url, grid_cols=4, grid_rows=3):
        self.display = display
        self.touch = touch
        self.server_url = server_url
        self.grid_cols = grid_cols
        self.grid_rows = grid_rows
        self.buttons = []
        self.last_sync = 0
        self.sync_interval = 1000  # 1 second (near real-time)
        self.profile_name = "Default"
        self.status = None

        # Calculate button dimensions
        self.button_width = display.width // grid_cols
        self.button_height = display.height // grid_rows

        # Initialize with empty buttons
        self.create_empty_buttons()

    def create_empty_buttons(self):
        """Create grid of empty buttons"""
        self.buttons = []
        for row in range(self.grid_rows):
            for col in range(self.grid_cols):
                x = col * self.button_width
                y = row * self.button_height
                btn = Button(
                    x, y,
                    self.button_width - 2,  # -2 for border spacing
                    self.button_height - 2,
                    text=f"{row * self.grid_cols + col + 1}",
                    bg_color=0x18C5  # Dark theme background #1a1a2e
                )
                self.buttons.append(btn)

    def draw_status(self, status="idle"):
        """Draw connection status indicator"""
        if self.status == status:
            return

        self.status = status
        colors = {
            "connected": 0x07E0,  # Green
            "syncing": 0xFFE0,    # Yellow
            "error": 0xF800       # Red
        }
        color = colors.get(status, 0x18C5)

        # Top-right corner indicator
        self.display.fill_rect(self.display.width - 10, 2, 8, 8, color)

    def draw(self):
        """Draw all buttons"""
        self.display.fill(0x18C5)  # Dark theme background #1a1a2e

        for btn in self.buttons:
            btn.draw(self.display)

        self.status = None
        self.draw_status("connected")

    def sync_with_server(self):
        """Fetch profile from server"""
        try:
            print("Syncing with server...")

            url = f"{self.server_url}/api/get-profile"
            profile = http_get(url, timeout=3)

            if profile:
                changed = self.load_profile(profile)
                if changed:
                    self.draw()
                self.draw_status("connected")
                print("Sync successful!")
                return changed
            else:
                self.draw_status("error")
                print("Sync failed!")
                return False

        except Exception as e:
            print(f"Sync error: {e}")
            self.draw_status("error")
            return False

    def load_profile(self, profile):
        """Load profile data into buttons"""
        try:
            buttons_data = profile.get('buttons', [])
            changed = False

            for i, btn_data in enumerate(buttons_data):
                if i >= len(self.buttons):
                    break

                btn = self.buttons[i]
                new_text = btn_data.get('text', btn_data.get('label', ''))
                new_icon = btn_data.get('icon', '')
                new_bg_color = self.parse_color(btn_data.get('bgColor', btn_data.get('color', '#1a1a2e')))
                new_action_type = btn_data.get('actionType')
                new_action_data = btn_data.get('actionData')

                if (btn.text != new_text or btn.icon != new_icon or
                        btn.bg_color != new_bg_color or
                        btn.action_type != new_action_type or
                        btn.action_data != new_action_data):
                    btn.text = new_text
                    btn.icon = new_icon
                    btn.bg_color = new_bg_color
                    btn.action_type = new_action_type
                    btn.action_data = new_action_data
                    changed = True

            new_profile_name = profile.get('name', 'Default')
            if self.profile_name != new_profile_name:
                self.profile_name = new_profile_name
                changed = True

            print(f"Loaded profile: {self.profile_name}")
            return changed

        except Exception as e:
            print(f"Profile load error: {e}")
            return False

    def parse_color(self, color_str):
        """Parse hex color string to RGB565"""
        try:
            if isinstance(color_str, str) and color_str.startswith('#'):
                color_str = color_str[1:]

            rgb = int(color_str, 16)
            r = (rgb >> 16) & 0xFF
            g = (rgb >> 8) & 0xFF
            b = rgb & 0xFF

            return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        except:
            return 0x18C5  # Default dark #1a1a2e

    def handle_touch(self):
        """Handle touch input"""
        touch_data = self.touch.get_touch()

        if touch_data:
            x, y, pressure = touch_data

            # Find touched button
            for btn in self.buttons:
                if btn.contains_point(x, y):
                    btn.on_press()
                    btn.draw(self.display)

                    # Wait for release
                    while self.touch.is_touched():
                        time.sleep(0.05)

                    # Execute action
                    action = btn.on_release()
                    btn.draw(self.display)

                    if action:
                        self.execute_action(action)

                    break

    def execute_action(self, action):
        """Send action to server for execution"""
        try:
            print(f"Executing: {action['type']}")

            url = f"{self.server_url}/api/execute-action"
            result = http_post(url, action, timeout=5)

            if result:
                print(f"Action result: {result.get('status', 'unknown')}")
            else:
                print("Action failed!")

        except Exception as e:
            print(f"Action error: {e}")

    def run(self):
        """Main loop"""
        print("StreamDeck running!")

        # Initial sync
        self.sync_with_server()
        self.draw()

        while True:
            try:
                # Check for sync
                now = time.ticks_ms()
                if time.ticks_diff(now, self.last_sync) > self.sync_interval:
                    self.sync_with_server()
                    self.last_sync = now
                    gc.collect()  # Clean up memory

                # Handle touch
                self.handle_touch()

                time.sleep(0.05)  # Small delay

            except KeyboardInterrupt:
                print("\nStopping StreamDeck...")
                break
            except Exception as e:
                print(f"Runtime error: {e}")
                time.sleep(1)
