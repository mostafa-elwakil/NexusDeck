"""
StreamDeck Companion Server
Lightweight Python server for system commands, resource monitoring, and application control
Enhanced with security, rate limiting, and improved error handling
"""

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
from functools import wraps
from collections import defaultdict
from datetime import datetime, timedelta
import subprocess
import psutil
import platform
import os
import json
import re
import time
import threading

app = Flask(__name__)
# CORS enabled with restrictions for security
CORS(app, origins=['http://localhost:*', 'http://127.0.0.1:*', 'http://*.local:*', '127.0.0.1'])

# Server configuration
PORT = 8765
HOST = '0.0.0.0'
SIMULATOR_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

@app.route('/', methods=['GET'])
def simulator_index():
    """Serve the browser simulator from the companion server root."""
    return send_from_directory(SIMULATOR_DIR, 'index.html')

@app.route('/css/<path:asset_path>', methods=['GET'])
def simulator_css(asset_path):
    return send_from_directory(os.path.join(SIMULATOR_DIR, 'css'), asset_path)

@app.route('/js/<path:asset_path>', methods=['GET'])
def simulator_javascript(asset_path):
    return send_from_directory(os.path.join(SIMULATOR_DIR, 'js'), asset_path)

# ===== Security Configuration =====
# Rate limiting
rate_limit_store = defaultdict(list)
RATE_LIMIT_REQUESTS = 100  # Requests
RATE_LIMIT_PERIOD = 60     # Seconds

# Input validation patterns
SAFE_APP_NAME_PATTERN = re.compile(r'^[\w\-\.]+$')
DANGEROUS_CHARS = ['&', '|', ';', '$', '`', '>', '<', '\n', '\r', '(', ')', '{', '}']
DANGEROUS_PATTERNS = [
    'rm -rf', 'del /', 'format', 'mkfs', 'dd if=',
    '> /dev/', 'curl | bash', 'wget | bash',
    'chmod 777', 'chown root'
]

# Allowed commands whitelist (for extra security)
ALLOWED_COMMANDS = [
    'ipconfig', 'ping', 'echo', 'dir', 'tasklist',
    'systeminfo', 'powershell', 'cmd'
]

def log_request(endpoint, data=None):
    """Log incoming requests"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print(f"[{timestamp}] {endpoint}", end='')
    if data:
        print(f" - {data}")
    else:
        print()

# ===== Security Functions =====

def rate_limit(f):
    """Rate limiting decorator"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        client_ip = request.remote_addr
        now = datetime.now()
        
        # Clean old requests
        rate_limit_store[client_ip] = [
            req_time for req_time in rate_limit_store[client_ip]
            if (now - req_time).total_seconds() < RATE_LIMIT_PERIOD
        ]
        
        # Check rate limit
        if len(rate_limit_store[client_ip]) >= RATE_LIMIT_REQUESTS:
            return jsonify({
                'success': False,
                'error': 'Rate limit exceeded. Too many requests.'
            }), 429
        
        rate_limit_store[client_ip].append(now)
        return f(*args, **kwargs)
    
    return decorated_function

def validate_app_name(app_name):
    """Validate application name format"""
    if not isinstance(app_name, str) or len(app_name) > 255:
        return False, 'Invalid app name'
    
    if not SAFE_APP_NAME_PATTERN.match(app_name):
        return False, 'App name contains invalid characters'
    
    return True, None

def validate_arguments(args):
    """Validate command arguments for dangerous characters"""
    if not isinstance(args, list):
        return False, 'Arguments must be a list'
    
    for arg in args:
        if not isinstance(arg, str):
            return False, 'All arguments must be strings'
        
        if len(arg) > 1024:
            return False, 'Argument exceeds maximum length'
        
        for char in DANGEROUS_CHARS:
            if char in arg:
                return False, 'Arguments contain dangerous characters'
    
    return True, None

def validate_command(command):
    """Validate shell command for dangerous patterns"""
    if not isinstance(command, str) or len(command) > 2048:
        return False, 'Invalid command'
    
    command_lower = command.lower()
    
    # Check dangerous patterns
    for pattern in DANGEROUS_PATTERNS:
        if pattern in command_lower:
            return False, f'Command blocked: contains \"{pattern}\"'
    
    # Check dangerous characters
    for char in ['|', '&', ';', '`', '$']:
        if char in command and 'powershell' in command_lower:
            # Allow pipes in PowerShell but be cautious
            pass
    
    return True, None

def validate_url(url):
    """Validate URL format"""
    if not isinstance(url, str) or len(url) > 2048:
        return False, 'Invalid URL'
    
    # Must start with http:// or https://
    if not (url.startswith('http://') or url.startswith('https://')):
        return False, 'URL must start with http:// or https://'
    
    return True, None

def validate_host(host):
    """Validate host/IP address"""
    if not isinstance(host, str) or len(host) > 255:
        return False, 'Invalid host'
    
    # Basic validation: alphanumeric, dots, hyphens, colons (for IPv6)
    if not re.match(r'^[\w\.\-:]+$', host):
        return False, 'Host contains invalid characters'
    
    return True, None

@app.route('/api/health', methods=['GET'])
@rate_limit
def health_check():
    """Health check endpoint"""
    log_request('GET /api/health')
    return jsonify({
        'status': 'ok',
        'server': 'StreamDeck Companion Server',
        'version': '2.0.0',
        'platform': platform.system(),
        'timestamp': datetime.now().isoformat()
    })

@app.route('/api/system-stats', methods=['GET'])
@rate_limit
def get_system_stats():
    """Get current system resource usage"""
    log_request('GET /api/system-stats')

    try:
        # Get CPU usage (average over 1 second)
        cpu_percent = psutil.cpu_percent(interval=1)

        # Get memory usage
        memory = psutil.virtual_memory()
        ram_percent = memory.percent

        # Get disk usage - Fixed for Windows compatibility
        if platform.system() == 'Windows':
            disk = psutil.disk_usage('C:\\')
        else:
            disk = psutil.disk_usage('/')
        disk_percent = disk.percent

        return jsonify({
            'success': True,
            'cpu': round(cpu_percent, 1),
            'ram': round(ram_percent, 1),
            'disk': round(disk_percent, 1),
            'timestamp': datetime.now().isoformat()
        })
    except Exception as e:
        log_request('ERROR', f'get_system_stats: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Failed to retrieve system stats'
        }), 500

@app.route('/api/open-app', methods=['POST'])
@rate_limit
def open_application():
    """Open an application"""
    data = request.json
    if not data:
        return jsonify({
            'success': False,
            'error': 'No JSON data provided'
        }), 400
    
    app_name = data.get('app', '').strip()
    args = data.get('args', [])

    log_request('POST /api/open-app', f'app={app_name}')

    if not app_name:
        return jsonify({
            'success': False,
            'error': 'Application name not provided'
        }), 400

    # Validate app name
    valid, error = validate_app_name(app_name)
    if not valid:
        return jsonify({
            'success': False,
            'error': error
        }), 400

    # Validate arguments
    valid, error = validate_arguments(args)
    if not valid:
        return jsonify({
            'success': False,
            'error': error
        }), 400

    try:
        # Handle common application shortcuts
        app_shortcuts = {
            'code': 'code',
            'vscode': 'code',
            'chrome': 'chrome.exe' if platform.system() == 'Windows' else 'google-chrome',
            'firefox': 'firefox.exe' if platform.system() == 'Windows' else 'firefox',
            'terminal': 'wt.exe' if platform.system() == 'Windows' else 'gnome-terminal',
            'notepad': 'notepad.exe' if platform.system() == 'Windows' else 'gedit',
            'calc': 'calc.exe' if platform.system() == 'Windows' else 'gnome-calculator',
            'explorer': 'explorer.exe' if platform.system() == 'Windows' else 'nautilus'
        }

        # Resolve shortcut if exists
        resolved_app = app_shortcuts.get(app_name.lower(), app_name)

        # Build command as list (safe, no shell injection)
        cmd = [resolved_app] + args

        # Start process - NO shell=True for security
        if platform.system() == 'Windows':
            subprocess.Popen(cmd, creationflags=subprocess.DETACHED_PROCESS)
        else:
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        return jsonify({
            'success': True,
            'app': app_name,
            'message': f'Successfully opened {app_name}'
        })
    except FileNotFoundError:
        return jsonify({
            'success': False,
            'error': f'Application "{app_name}" not found'
        }), 404
    except Exception as e:
        # Don't expose internal error details
        log_request('ERROR', f'open_application: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Failed to open application'
        }), 500

@app.route('/api/run-command', methods=['POST'])
@rate_limit
def run_command():
    """Execute a shell command"""
    data = request.json
    if not data:
        return jsonify({
            'success': False,
            'error': 'No JSON data provided'
        }), 400
    
    command = data.get('command', '').strip()
    shell_type = data.get('shell', 'powershell')

    log_request('POST /api/run-command', f'shell={shell_type}')

    if not command:
        return jsonify({
            'success': False,
            'error': 'Command not provided'
        }), 400

    # Validate command for dangerous patterns
    valid, error = validate_command(command)
    if not valid:
        return jsonify({
            'success': False,
            'error': error
        }), 400

    try:
        # Select shell - use array form to avoid shell injection
        if platform.system() == 'Windows':
            if shell_type.lower() == 'powershell':
                shell_cmd = ['powershell', '-NoProfile', '-Command', command]
            else:
                shell_cmd = ['cmd', '/c', command]
        else:
            shell_cmd = ['bash', '-c', command]

        # Execute command with timeout
        result = subprocess.run(
            shell_cmd,
            capture_output=True,
            text=True,
            timeout=30,
            shell=False  # Explicit: don't use shell
        )

        # Limit output size
        output = result.stdout[:5000]  # Max 5KB output
        error_output = result.stderr[:5000] if result.returncode != 0 else None

        return jsonify({
            'success': result.returncode == 0,
            'output': output,
            'error': error_output,
            'returncode': result.returncode
        })
    except subprocess.TimeoutExpired:
        return jsonify({
            'success': False,
            'error': 'Command execution timeout (30s limit)'
        }), 408
    except Exception as e:
        log_request('ERROR', f'run_command: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Failed to execute command'
        }), 500

@app.route('/api/ping', methods=['POST'])
@rate_limit
def ping_host():
    """Ping a host and return latency"""
    data = request.json
    if not data:
        return jsonify({
            'success': False,
            'error': 'No JSON data provided'
        }), 400
    
    host = data.get('host', '8.8.8.8').strip()

    log_request('POST /api/ping', f'host={host}')

    # Validate host
    valid, error = validate_host(host)
    if not valid:
        return jsonify({
            'success': False,
            'error': error
        }), 400

    try:
        # Build ping command based on platform
        if platform.system() == 'Windows':
            cmd = ['ping', '-n', '1', '-w', '3000', host]
        else:
            cmd = ['ping', '-c', '1', '-W', '3', host]

        start_time = datetime.now()
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=5,
            shell=False
        )
        end_time = datetime.now()

        latency = (end_time - start_time).total_seconds() * 1000

        return jsonify({
            'success': result.returncode == 0,
            'host': host,
            'latency': round(latency, 2),
            'message': 'Host is reachable' if result.returncode == 0 else 'Host unreachable'
        })
    except subprocess.TimeoutExpired:
        return jsonify({
            'success': False,
            'error': 'Ping timeout (5s)',
            'latency': None
        }), 408
    except Exception as e:
        log_request('ERROR', f'ping_host: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Failed to ping host',
            'latency': None
        }), 500

@app.route('/api/http-proxy', methods=['POST'])
@rate_limit
def http_proxy():
    """HTTP proxy to bypass CORS restrictions"""
    data = request.json
    if not data:
        return jsonify({
            'success': False,
            'error': 'No JSON data provided'
        }), 400
    
    url = data.get('url', '').strip()
    method = data.get('method', 'GET').upper()

    log_request('POST /api/http-proxy', f'url={url}')

    if not url:
        return jsonify({
            'success': False,
            'error': 'URL not provided'
        }), 400

    # Validate URL
    valid, error = validate_url(url)
    if not valid:
        return jsonify({
            'success': False,
            'error': error
        }), 400

    try:
        import requests

        # Only allow GET and POST
        if method not in ['GET', 'POST']:
            return jsonify({
                'success': False,
                'error': f'Method "{method}" not allowed'
            }), 400

        # Make request with timeout
        if method == 'GET':
            response = requests.get(url, timeout=10)
        else:  # POST
            response = requests.post(url, json=data.get('body'), timeout=10)

        # Limit response size
        response_text = response.text[:5000]  # Max 5KB

        return jsonify({
            'success': True,
            'status': response.status_code,
            'body': response_text,
            'headers': dict(list(response.headers.items())[:10])  # Limit headers
        })
    except requests.exceptions.Timeout:
        return jsonify({
            'success': False,
            'error': 'HTTP request timeout (10s)'
        }), 408
    except requests.exceptions.ConnectionError:
        return jsonify({
            'success': False,
            'error': 'Connection error'
        }), 503
    except Exception as e:
        log_request('ERROR', f'http_proxy: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Failed to make HTTP request'
        }), 500

# ===== ESP32 Integration Endpoints =====

# Store current profile for ESP32 sync
current_profile = {
    "name": "Default Profile",
    "size": "cyd",
    "buttons": []
}

@app.route('/api/get-profile', methods=['GET'])
def get_profile():
    """Get current profile for ESP32 synchronization"""
    log_request('GET /api/get-profile')
    return jsonify(current_profile)

@app.route('/api/set-profile', methods=['POST'])
def set_profile():
    """Update current profile from web interface"""
    global current_profile
    data = request.json

    log_request('POST /api/set-profile')

    if not data or 'buttons' not in data:
        return jsonify({
            'success': False,
            'error': 'Invalid profile data'
        }), 400

    current_profile = data

    return jsonify({
        'success': True,
        'message': 'Profile updated'
    })

@app.route('/api/execute-action', methods=['POST'])
@rate_limit
def execute_action():
    """Execute action triggered from ESP32 or web interface"""
    data = request.json
    if not data:
        return jsonify({
            'success': False,
            'error': 'No JSON data provided'
        }), 400
    
    action_type = data.get('actionType', '').strip()
    action_data = data.get('actionData', '{}')

    if isinstance(action_data, str):
        try:
            action_config = json.loads(action_data) if action_data else {}
        except json.JSONDecodeError:
            return jsonify({
                'success': False,
                'error': 'Invalid action data JSON'
            }), 400
    else:
        action_config = action_data or {}

    log_request('POST /api/execute-action', f'type={action_type}')

    if not action_type:
        return jsonify({
            'success': False,
            'error': 'Action type not provided'
        }), 400

    try:
        # Execute based on action type
        if action_type == 'open_url':
            url = action_config.get('url', '').strip()
            
            # Validate URL
            valid, error = validate_url(url)
            if not valid:
                return jsonify({
                    'success': False,
                    'error': f'Invalid URL: {error}'
                }), 400
            
            import webbrowser
            webbrowser.open(url)
            return jsonify({'success': True, 'message': f'Opened URL'})

        elif action_type == 'open_app':
            app_name = action_config.get('app', '').strip()
            
            # Validate app name
            valid, error = validate_app_name(app_name)
            if not valid:
                return jsonify({
                    'success': False,
                    'error': error
                }), 400
            
            # Use shortcuts
            app_shortcuts = {
                'code': 'code',
                'vscode': 'code',
                'chrome': 'chrome.exe' if platform.system() == 'Windows' else 'google-chrome',
            }
            resolved_app = app_shortcuts.get(app_name.lower(), app_name)
            
            # NO shell=True for security
            if platform.system() == 'Windows':
                subprocess.Popen([resolved_app], creationflags=subprocess.DETACHED_PROCESS)
            else:
                subprocess.Popen([resolved_app], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
            return jsonify({'success': True, 'message': f'Opened {app_name}'})

        elif action_type == 'run_command':
            command = action_config.get('command', '').strip()
            shell_type = action_config.get('shell', 'powershell')
            
            # Validate command
            valid, error = validate_command(command)
            if not valid:
                return jsonify({
                    'success': False,
                    'error': error
                }), 400

            if command:
                if platform.system() == 'Windows':
                    shell_cmd = ['powershell', '-NoProfile', '-Command', command] if shell_type == 'powershell' else ['cmd', '/c', command]
                else:
                    shell_cmd = ['bash', '-c', command]

                result = subprocess.run(
                    shell_cmd,
                    capture_output=True,
                    text=True,
                    timeout=10,
                    shell=False
                )
                
                return jsonify({
                    'success': result.returncode == 0,
                    'output': result.stdout[:2000],
                    'error': result.stderr[:2000] if result.returncode != 0 else None
                })

        return jsonify({
            'success': False,
            'error': f'Unsupported action type: {action_type}'
        }), 400

    except json.JSONDecodeError:
        return jsonify({
            'success': False,
            'error': 'Invalid action data JSON'
        }), 400
    except subprocess.TimeoutExpired:
        return jsonify({
            'success': False,
            'error': 'Action timeout (10s)'
        }), 408
    except Exception as e:
        log_request('ERROR', f'execute_action: {str(e)}')
        return jsonify({
            'success': False,
            'error': 'Action execution failed'
        }), 500

@app.errorhandler(404)
def not_found(error):
    """Handle 404 errors"""
    return jsonify({
        'success': False,
        'error': 'Endpoint not found'
    }), 404

@app.errorhandler(500)
def internal_error(error):
    """Handle 500 errors"""
    log_request('ERROR', f'Internal error: {str(error)}')
    return jsonify({
        'success': False,
        'error': 'Internal server error'
    }), 500

if __name__ == '__main__':
    print("=" * 70)
    print("StreamDeck Companion Server v2.0.0")
    print("=" * 70)
    print(f"Platform: {platform.system()} {platform.release()}")
    print(f"Python: {platform.python_version()}")
    print(f"Server URL: http://localhost:{PORT}")
    print(f"Rate Limit: {RATE_LIMIT_REQUESTS} requests per {RATE_LIMIT_PERIOD}s")
    print("=" * 70)
    print("⚙️  Security Features Enabled:")
    print("  ✅ Rate Limiting (100 req/min per IP)")
    print("  ✅ CORS Restricted (localhost only)")
    print("  ✅ Input Validation & Sanitization")
    print("  ✅ Command Injection Protection")
    print("  ✅ Output Size Limits (5KB max)")
    print("  ✅ Timeout Protection (30s max)")
    print("=" * 70)
    print("Press Ctrl+C to stop the server")
    print("=" * 70)

    try:
        app.run(
            host=HOST,
            port=PORT,
            debug=False,
            threaded=True
        )
    except KeyboardInterrupt:
        print("\n\n✋ Server stopped by user")
    except Exception as e:
        print(f"\n\n❌ Server error: {e}")
