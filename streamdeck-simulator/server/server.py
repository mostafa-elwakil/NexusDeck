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
import shutil
import psutil
import platform
import os
import json
import re
import sys
import time
import threading
import socket  # إضافة المكتبة هنا

app = Flask(__name__)
# CORS enabled with restrictions for security
CORS(app, origins=['http://localhost:*', 'http://127.0.0.1:*', 'http://*.local:*', '127.0.0.1'])

# Server configuration
PORT = 8765
HOST = '0.0.0.0'
SIMULATOR_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def _obs_get_field(obj, *names):
    """Read a field from an obsws-python response regardless of key naming style.

    obsws-python returns AttrDict objects with snake_case keys, but some
    versions/fields may still expose camelCase - try both.
    """
    for name in names:
        if isinstance(obj, dict):
            if name in obj:
                return obj[name]
        else:
            value = getattr(obj, name, None)
            if value is not None:
                return value
    return None


def _obs_disconnect(client):
    """Best-effort disconnect to avoid leaking websocket threads per request."""
    try:
        if client is not None and hasattr(client, 'disconnect'):
            client.disconnect()
    except Exception:
        pass


def _obs_connect(data):
    """Create an obsws-python request client from request data or environment."""
    import obsws_python

    host = data.get('host') or os.getenv('OBS_WS_HOST', '127.0.0.1')
    raw_port = data.get('port') or os.getenv('OBS_WS_PORT', '4455')
    password = data.get('password') or os.getenv('OBS_WS_PASSWORD', '')

    try:
        port = int(raw_port)
    except (TypeError, ValueError):
        raise ValueError('OBS port must be a number')

    return obsws_python.ReqClient(host=host, port=port, password=password, timeout=5)


def execute_obs_action(data):
    """Execute an OBS WebSocket 5 action using obsws-python."""
    try:
        import obsws_python  # noqa: F401 - availability check
    except ImportError:
        return False, 'Install obsws-python (pip install obsws-python) and enable OBS WebSocket 5'

    operation = (data.get('operation') or '').strip()
    if not operation:
        return False, 'OBS operation is required'

    # Validate the operation BEFORE connecting so users get precise errors
    # even when OBS is not reachable.
    supported = {
        'set_scene', 'start_recording', 'stop_recording',
        'toggle_recording', 'set_source_visibility'
    }
    if operation not in supported:
        return False, f'Unsupported OBS operation: {operation}'

    # Validate required fields BEFORE connecting for precise error messages
    if operation == 'set_scene' and not (data.get('scene') or '').strip():
        return False, 'OBS scene name is required'
    if operation == 'set_source_visibility':
        if not (data.get('scene') or '').strip() or not (data.get('source') or '').strip():
            return False, 'OBS scene and source are required'

    client = None
    try:
        client = _obs_connect(data)

        if operation == 'set_scene':
            scene = (data.get('scene') or '').strip()
            if not scene:
                return False, 'OBS scene name is required'
            client.set_current_program_scene(scene)

        elif operation == 'start_recording':
            client.start_record()

        elif operation == 'stop_recording':
            response = client.stop_record()
            output_path = _obs_get_field(response, 'output_path', 'outputPath')
            if output_path:
                return True, f'Recording saved: {output_path}'

        elif operation == 'toggle_recording':
            client.toggle_record()

        elif operation == 'set_source_visibility':
            scene = (data.get('scene') or '').strip()
            source = (data.get('source') or '').strip()
            if not scene or not source:
                return False, 'OBS scene and source are required'
            items = client.get_scene_item_list(scene).scene_items
            item = next(
                (entry for entry in items
                 if _obs_get_field(entry, 'source_name', 'sourceName') == source),
                None
            )
            if item is None:
                return False, f'OBS source not found in scene "{scene}": {source}'
            client.set_scene_item_enabled(
                scene,
                _obs_get_field(item, 'scene_item_id', 'sceneItemId'),
                bool(data.get('visible', True))
            )

        return True, f'OBS {operation} completed'
    except ValueError as error:
        return False, str(error)
    except Exception as error:
        log_request('ERROR', f'OBS: {str(error)}')
        message = str(error)
        lowered = message.lower()
        if any(token in lowered for token in ('timed out', 'timeout', 'connection', 'refused', 'handshake', 'unreachable')):
            return False, ('Cannot reach OBS. Make sure OBS is running and WebSocket is '
                           'enabled (Tools > WebSocket Server Settings, default port 4455).')
        if any(token in lowered for token in ('auth', 'password', '401', '403')):
            return False, 'OBS authentication failed - check the WebSocket password.'
        if 'no source was found' in lowered:
            return False, ('OBS scene/source not found - the name must match exactly. '
                           'Check the OBS Scenes/Sources list for the correct name.')
        return False, f'OBS action failed: {message}'
    finally:
        _obs_disconnect(client)

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

@app.route('/presets/<path:asset_path>', methods=['GET'])
def simulator_presets(asset_path):
    return send_from_directory(os.path.join(SIMULATOR_DIR, 'presets'), asset_path)

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


def validate_docker_container(container):
    """Validate Docker container name used by ESP32 docker_command actions."""
    if not isinstance(container, str) or not container.strip() or len(container) > 128:
        return False, 'Invalid container name'
    if not re.match(r'^[\w.\-*]+$', container.strip()):
        return False, 'Container name contains invalid characters'
    return True, None


APP_ALIASES = {
    'code': ['code', 'Code.exe', 'code.cmd'],
    'vscode': ['code', 'Code.exe', 'code.cmd'],
    'chrome': ['chrome.exe', 'google-chrome', 'chrome'],
    'firefox': ['firefox.exe', 'firefox'],
    'terminal': ['wt.exe', 'wt', 'gnome-terminal'],
    'notepad': ['notepad.exe', 'gedit'],
    'calc': ['calc.exe', 'gnome-calculator'],
    'explorer': ['explorer.exe', 'nautilus'],
    'obs': ['obs64.exe', 'obs'],
}


def _common_app_paths(app_name):
    """Known install locations so Windows apps launch even when PATH is incomplete."""
    if platform.system() != 'Windows':
        return []

    local = os.environ.get('LOCALAPPDATA', '')
    pf = os.environ.get('ProgramFiles', r'C:\Program Files')
    pf86 = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
    system32 = os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32')
    key = app_name.lower()

    locations = {
        'code': [
            os.path.join(local, r'Programs\Microsoft VS Code\Code.exe'),
            os.path.join(pf, r'Microsoft VS Code\Code.exe'),
        ],
        'vscode': [
            os.path.join(local, r'Programs\Microsoft VS Code\Code.exe'),
            os.path.join(pf, r'Microsoft VS Code\Code.exe'),
        ],
        'chrome': [
            os.path.join(pf, r'Google\Chrome\Application\chrome.exe'),
            os.path.join(local, r'Google\Chrome\Application\chrome.exe'),
            os.path.join(pf86, r'Google\Chrome\Application\chrome.exe'),
        ],
        'chrome.exe': [
            os.path.join(pf, r'Google\Chrome\Application\chrome.exe'),
            os.path.join(local, r'Google\Chrome\Application\chrome.exe'),
        ],
        'firefox': [os.path.join(pf, r'Mozilla Firefox\firefox.exe')],
        'firefox.exe': [os.path.join(pf, r'Mozilla Firefox\firefox.exe')],
        'obs': [os.path.join(pf, r'obs-studio\bin\64bit\obs64.exe')],
        'obs64.exe': [os.path.join(pf, r'obs-studio\bin\64bit\obs64.exe')],
        'vlc.exe': [
            os.path.join(pf, r'VideoLAN\VLC\vlc.exe'),
            os.path.join(pf86, r'VideoLAN\VLC\vlc.exe'),
        ],
        'wt.exe': [os.path.join(local, r'Microsoft\WindowsApps\wt.exe')],
        'terminal': [os.path.join(local, r'Microsoft\WindowsApps\wt.exe')],
        'notepad.exe': [os.path.join(system32, 'notepad.exe')],
        'calc.exe': [os.path.join(system32, 'calc.exe')],
        'taskmgr.exe': [os.path.join(system32, 'taskmgr.exe')],
        'explorer.exe': [os.path.join(system32, 'explorer.exe')],
        'excel.exe': [
            os.path.join(pf, r'Microsoft Office\root\Office16\EXCEL.EXE'),
            os.path.join(pf, r'Microsoft Office\Office16\EXCEL.EXE'),
        ],
        'winword.exe': [
            os.path.join(pf, r'Microsoft Office\root\Office16\WINWORD.EXE'),
            os.path.join(pf, r'Microsoft Office\Office16\WINWORD.EXE'),
        ],
        'teams.exe': [
            os.path.join(local, r'Microsoft\WindowsApps\ms-teams.exe'),
            os.path.join(local, r'Microsoft\Teams\current\Teams.exe'),
        ],
    }
    return [path for path in locations.get(key, []) if path and os.path.isfile(path)]


def resolve_app_executable(app_name):
    """Resolve a button app name to an executable Windows/Linux can actually launch."""
    names = APP_ALIASES.get(app_name.lower(), [app_name])
    search_names = []
    for name in names + [app_name]:
        if name and name not in search_names:
            search_names.append(name)

    for path in _common_app_paths(app_name):
        return path

    extra_path = os.environ.get('PATH', '')
    if platform.system() == 'Windows':
        local = os.environ.get('LOCALAPPDATA', '')
        extras = [
            os.path.join(local, r'Programs\Microsoft VS Code\bin'),
            os.path.join(local, r'Microsoft\WindowsApps'),
            os.path.join(os.environ.get('ProgramFiles', r'C:\Program Files'), r'Microsoft VS Code\bin'),
        ]
        extra_path = os.pathsep.join([extra_path] + [item for item in extras if os.path.isdir(item)])

    for name in search_names:
        found = shutil.which(name, path=extra_path)
        if found:
            return found
        if platform.system() == 'Windows' and not os.path.splitext(name)[1]:
            for ext in ('.exe', '.cmd', '.bat', '.com'):
                found = shutil.which(name + ext, path=extra_path)
                if found:
                    return found
    return None


def execute_open_app(app_name, args=None):
    app_name = (app_name or '').strip()
    args = args or []

    if not app_name:
        return False, 'Application name not provided'
    valid, error = validate_app_name(app_name)
    if not valid:
        return False, error
    valid, error = validate_arguments(args)
    if not valid:
        return False, error

    resolved_app = resolve_app_executable(app_name)
    if not resolved_app:
        return False, f'Application "{app_name}" not found'

    try:
        if platform.system() == 'Windows' and not args:
            os.startfile(resolved_app)
            return True, f'Successfully opened {app_name}'

        if platform.system() == 'Windows' and resolved_app.lower().endswith(('.cmd', '.bat')):
            subprocess.Popen(
                ['cmd', '/c', resolved_app] + args,
                creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
            )
        elif platform.system() == 'Windows':
            subprocess.Popen(
                [resolved_app] + args,
                creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
            )
        else:
            subprocess.Popen([resolved_app] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True, f'Successfully opened {app_name}'
    except FileNotFoundError:
        return False, f'Application "{app_name}" not found'
    except Exception as error:
        log_request('ERROR', f'open_app: {str(error)}')
        return False, 'Failed to open application'


def execute_run_command(command, shell_type='powershell', timeout=30):
    command = (command or '').strip()
    if not command:
        return False, 'Command not provided', None
    valid, error = validate_command(command)
    if not valid:
        return False, error, None

    if platform.system() == 'Windows':
        if (shell_type or 'powershell').lower() == 'powershell':
            shell_cmd = ['powershell', '-NoProfile', '-Command', command]
        else:
            shell_cmd = ['cmd', '/c', command]
    else:
        shell_cmd = ['bash', '-c', command]

    result = subprocess.run(
        shell_cmd,
        capture_output=True,
        text=True,
        timeout=timeout,
        shell=False
    )
    output = (result.stdout or '')[:5000]
    error_output = (result.stderr or '')[:5000] if result.returncode != 0 else None
    if result.returncode == 0:
        return True, output, None
    return False, error_output or 'Command execution failed', output


def execute_ping(host):
    host = (host or '8.8.8.8').strip()
    valid, error = validate_host(host)
    if not valid:
        return False, error, None

    if platform.system() == 'Windows':
        cmd = ['ping', '-n', '1', '-w', '3000', host]
    else:
        cmd = ['ping', '-c', '1', '-W', '3', host]

    start_time = datetime.now()
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=5, shell=False)
    latency = round((datetime.now() - start_time).total_seconds() * 1000, 2)
    if result.returncode == 0:
        return True, 'Host is reachable', latency
    return False, 'Host unreachable', latency


def execute_http_check(url, method='GET', timeout=5):
    url = (url or '').strip()
    method = (method or 'GET').upper()
    valid, error = validate_url(url)
    if not valid:
        return False, error, None
    if method not in ('GET', 'HEAD'):
        return False, f'Method "{method}" not allowed', None

    import urllib.request
    import urllib.error

    request = urllib.request.Request(url, method=method)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return True, f'Status {response.status}', response.status
    except urllib.error.HTTPError as error:
        return False, f'Status {error.code}', error.code
    except Exception:
        return False, 'HTTP check failed', None


def execute_docker_command(docker_action, container):
    docker_action = (docker_action or '').strip()
    container = (container or '').strip()
    valid, error = validate_docker_container(container)
    if not valid:
        return False, error, None

    if docker_action == 'status' and container in ('*', 'all'):
        command = 'docker ps -a'
    else:
        commands = {
            'start': f'docker start {container}',
            'stop': f'docker stop {container}',
            'restart': f'docker restart {container}',
            'status': f'docker ps -a --filter name={container}'
        }
        command = commands.get(docker_action)
        if not command:
            return False, 'Unknown Docker action', None

    success, message, output = execute_run_command(command, 'powershell', timeout=30)
    return success, message, output


def execute_copy_text(text):
    if not isinstance(text, str) or text == '':
        return False, 'No text to copy'
    if len(text) > 4096:
        return False, 'Text exceeds maximum length'

    try:
        if platform.system() == 'Windows':
            result = subprocess.run(
                ['powershell', '-NoProfile', '-Command', '[Console]::In.ReadToEnd() | Set-Clipboard'],
                input=text,
                text=True,
                timeout=5,
                capture_output=True
            )
            if result.returncode != 0:
                return False, 'Failed to copy text'
        else:
            result = subprocess.run(['xclip', '-selection', 'clipboard'], input=text, text=True, timeout=5)
            if result.returncode != 0:
                return False, 'Failed to copy text (xclip required)'
        return True, 'Text copied to clipboard'
    except FileNotFoundError:
        return False, 'Clipboard tool not available'
    except subprocess.TimeoutExpired:
        return False, 'Clipboard timeout'

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

@app.route('/api/obs-control', methods=['POST'])
@rate_limit
def obs_control():
    """Control OBS through its OBS WebSocket 5 server."""
    data = request.get_json(silent=True) or {}
    success, message = execute_obs_action(data)
    return jsonify({'success': success, 'message': message, 'error': None if success else message}), (200 if success else 400)

@app.route('/api/obs-status', methods=['POST'])
@rate_limit
def obs_status():
    """Check OBS WebSocket connectivity and current recording state."""
    data = request.get_json(silent=True) or {}

    try:
        import obsws_python  # noqa: F401 - availability check
    except ImportError:
        return jsonify({
            'success': False,
            'connected': False,
            'error': 'obsws-python is not installed (pip install obsws-python)'
        }), 503

    client = None
    try:
        client = _obs_connect(data)
        version = client.get_version()
        record = client.get_record_status()
        return jsonify({
            'success': True,
            'connected': True,
            'obs_version': _obs_get_field(version, 'obs_version', 'obsVersion') or 'unknown',
            'websocket_version': _obs_get_field(version, 'obs_web_socket_version', 'obsWebSocketVersion') or '',
            'recording': bool(_obs_get_field(record, 'output_active', 'outputActive')),
            'message': 'OBS connected'
        })
    except ValueError as error:
        return jsonify({'success': False, 'connected': False, 'error': str(error)}), 400
    except Exception as error:
        log_request('ERROR', f'OBS status: {str(error)}')
        return jsonify({
            'success': False,
            'connected': False,
            'error': 'Cannot reach OBS. Is it running with WebSocket enabled (Tools > WebSocket Server Settings)?'
        }), 503
    finally:
        _obs_disconnect(client)

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

    success, message = execute_open_app(app_name, args)
    status = 200 if success else 400
    return jsonify({
        'success': success,
        'app': app_name,
        'message': message if success else None,
        'error': None if success else message
    }), status

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

# Profile persistence: survive server restarts so the ESP32 device keeps
# its buttons even when the companion server is restarted.
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROFILE_STATE_FILE = os.path.join(CURRENT_DIR, 'profile_state.json')
SETTINGS_DB_FILE = os.path.join(CURRENT_DIR, 'server_settings.json')


def _load_server_settings():
    try:
        if os.path.exists(SETTINGS_DB_FILE):
            with open(SETTINGS_DB_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
                if isinstance(data, dict):
                    return data
    except Exception:
        pass
    return {
        "esp32_ip": "",
        "server_port": 8765,
        "auto_sync": True,
        "devices": []
    }


def _persist_server_settings(settings):
    try:
        with open(SETTINGS_DB_FILE, 'w', encoding='utf-8') as f:
            json.dump(settings, f, ensure_ascii=False, indent=2)
    except Exception as error:
        log_request('ERROR', f'persist settings: {str(error)}')


server_settings = _load_server_settings()


def _load_persisted_profile():
    """Load the last synced profile from disk (fallback: OBS-ready default)."""
    try:
        if os.path.exists(PROFILE_STATE_FILE):
            with open(PROFILE_STATE_FILE, 'r', encoding='utf-8') as file_handle:
                data = json.load(file_handle)
                if isinstance(data, dict) and 'buttons' in data:
                    return data
    except Exception as error:
        log_request('ERROR', f'load persisted profile: {str(error)}')

    # Fallback default: OBS Studio profile so a fresh install is useful
    return {
        "name": "OBS Studio",
        "size": "cyd",
        "rows": 3,
        "cols": 4,
        "buttons": [
            {"label": "OBS", "icon": "🎥", "color": "#302e31", "action": {"type": "open_app", "app": "obs64.exe"}},
            {"label": "Scene: Game", "icon": "🎮", "color": "#2496ed", "action": {"type": "obs_control", "operation": "set_scene", "scene": "Game"}},
            {"label": "Scene: Chat", "icon": "💬", "color": "#6264a7", "action": {"type": "obs_control", "operation": "set_scene", "scene": "Chatting"}},
            {"label": "Scene: Desktop", "icon": "🖥️", "color": "#007acc", "action": {"type": "obs_control", "operation": "set_scene", "scene": "Desktop"}},
            {"label": "Camera On", "icon": "📷", "color": "#1db954", "action": {"type": "obs_control", "operation": "set_source_visibility", "scene": "Game", "source": "Camera", "visible": True}},
            {"label": "Camera Off", "icon": "🚫", "color": "#3e1a1a", "action": {"type": "obs_control", "operation": "set_source_visibility", "scene": "Game", "source": "Camera", "visible": False}},
            {"label": "Start Rec", "icon": "⏺️", "color": "#ff0000", "action": {"type": "obs_control", "operation": "start_recording"}},
            {"label": "Stop Rec", "icon": "⏹️", "color": "#8a1a1a", "action": {"type": "obs_control", "operation": "stop_recording"}},
            {"label": "Rec Toggle", "icon": "⏯️", "color": "#ea4335", "action": {"type": "obs_control", "operation": "toggle_recording"}},
            {"label": "Scene", "icon": "🖥️", "color": "#1a2e3e", "action": {"type": "obs_control", "operation": "set_scene", "scene": "Scene"}},
            {"label": "Pause Rec", "icon": "⏸️", "color": "#8a6a1a", "action": {"type": "obs_control", "operation": "toggle_recording"}},
            {"label": "Stop Only", "icon": "⏹️", "color": "#5a1a1a", "action": {"type": "obs_control", "operation": "stop_recording"}}
        ]
    }


def _persist_profile(profile):
    """Save the current profile to disk (best-effort)."""
    try:
        with open(PROFILE_STATE_FILE, 'w', encoding='utf-8') as file_handle:
            json.dump(profile, file_handle, ensure_ascii=False)
    except Exception as error:
        log_request('ERROR', f'persist profile: {str(error)}')


current_profile = _load_persisted_profile()

# ESP32 device sync tracking (updated when hardware polls /api/get-profile)
esp32_device_state = {
    'last_sync': None,
    'profile_name': None,
    'button_count': 0,
}


def _record_esp32_sync(profile):
    """Track the last time the ESP32 hardware pulled the active profile."""
    esp32_device_state['last_sync'] = datetime.now().isoformat()
    esp32_device_state['profile_name'] = profile.get('name')
    esp32_device_state['button_count'] = len(profile.get('buttons') or [])


@app.route('/api/get-profile', methods=['GET'])
def get_profile():
    """Get current profile for ESP32 synchronization"""
    log_request('GET /api/get-profile')

    client = (request.headers.get('X-StreamDeck-Client') or '').strip().lower()
    if client == 'esp32':
        _record_esp32_sync(current_profile)
        esp32_ip = request.remote_addr
        if esp32_ip and server_settings.get('esp32_ip') != esp32_ip:
            server_settings['esp32_ip'] = esp32_ip
            _persist_server_settings(server_settings)

    # جلب الـ IP الخاص بالجهاز
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    response = jsonify(current_profile)
    # إضافة الـ IP في الـ Header أو إرساله كجزء من البروفايل
    response.headers['X-Server-IP'] = local_ip
    return response


@app.route('/api/settings', methods=['GET', 'POST'])
def handle_settings():
    global server_settings
    log_request(f'{request.method} /api/settings')
    if request.method == 'POST':
        data = request.json or {}
        server_settings.update(data)
        _persist_server_settings(server_settings)
        return jsonify({'success': True, 'settings': server_settings})
    return jsonify({'success': True, 'settings': server_settings})



@app.route('/api/device-status', methods=['GET'])
@rate_limit
def device_status():
    """Report companion + ESP32 hardware sync status for the web UI."""
    log_request('GET /api/device-status')

    last_sync = esp32_device_state.get('last_sync')
    seconds_ago = None
    connected = False

    if last_sync:
        try:
            sync_time = datetime.fromisoformat(last_sync)
            seconds_ago = round((datetime.now() - sync_time).total_seconds(), 1)
            connected = seconds_ago <= 20
        except ValueError:
            seconds_ago = None

    return jsonify({
        'success': True,
        'esp32': {
            'connected': connected,
            'last_sync': last_sync,
            'seconds_ago': seconds_ago,
            'profile_name': esp32_device_state.get('profile_name'),
            'button_count': esp32_device_state.get('button_count', 0),
        },
        'profile_name': current_profile.get('name'),
        'timestamp': datetime.now().isoformat()
    })

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
    _persist_profile(current_profile)

    return jsonify({
        'success': True,
        'message': 'Profile updated'
    })


@app.route('/api/set-background', methods=['POST'])
def set_background():
    """Update ESP32 deck background color on the active profile."""
    global current_profile
    data = request.json or {}
    color = (data.get('backgroundColor') or data.get('color') or '').strip()

    log_request('POST /api/set-background', f'color={color}')

    if not re.match(r'^#[0-9a-fA-F]{6}$', color):
        return jsonify({
            'success': False,
            'error': 'backgroundColor must be a hex color like #000000'
        }), 400

    current_profile['backgroundColor'] = color
    _persist_profile(current_profile)

    return jsonify({
        'success': True,
        'backgroundColor': color
    })

@app.route('/api/set-esp-ip', methods=['POST'])
def set_esp_ip():
    data = request.json
    esp_ip = data.get('ip')
    # حفظ الـ IP في ملف مؤقت أو في الذاكرة
    with open('esp_ip.txt', 'w') as f:
        f.write(esp_ip)
    return jsonify({'success': True})


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
        if action_type == 'open_url':
            url = action_config.get('url', '').strip()
            valid, error = validate_url(url)
            if not valid:
                return jsonify({'success': False, 'error': f'Invalid URL: {error}'}), 400
            import webbrowser
            webbrowser.open(url)
            return jsonify({'success': True, 'message': 'Opened URL'})

        elif action_type == 'open_app':
            success, message = execute_open_app(action_config.get('app', ''), action_config.get('args', []))
            return jsonify({'success': success, 'message': message, 'error': None if success else message}), (200 if success else 400)

        elif action_type == 'run_command':
            success, message, output = execute_run_command(
                action_config.get('command', ''),
                action_config.get('shell', 'powershell')
            )
            return jsonify({
                'success': success,
                'message': message if success else None,
                'output': output if success else message,
                'error': None if success else message
            }), (200 if success else 400)

        elif action_type == 'obs_control':
            success, message = execute_obs_action({
                **action_config,
                'operation': action_config.get('operation', '')
            })
            return jsonify({'success': success, 'message': message, 'error': None if success else message}), (200 if success else 400)

        elif action_type == 'ping':
            success, message, latency = execute_ping(action_config.get('host', '8.8.8.8'))
            return jsonify({
                'success': success,
                'message': message,
                'latency': latency,
                'error': None if success else message
            }), (200 if success else 400)

        elif action_type == 'http_check':
            success, message, status = execute_http_check(
                action_config.get('url', ''),
                action_config.get('method', 'GET')
            )
            return jsonify({
                'success': success,
                'message': message,
                'status': status,
                'error': None if success else message
            }), (200 if success else 400)

        elif action_type == 'switch_profile':
            global current_profile
            # Check if specific profile name was requested in actionData
            target_name = action_config.get('name', '').strip().lower()
            
            presets_dir = os.path.join(SIMULATOR_DIR, 'presets')
            print(f"DEBUG: presets_dir={presets_dir}")
            preset_files = sorted([f for f in os.listdir(presets_dir) if f.endswith('.json')]) if os.path.exists(presets_dir) else []
            print(f"DEBUG: preset_files={preset_files}")
            
            all_profiles = []
            for pf in preset_files:
                pf_path = os.path.join(presets_dir, pf)
                try:
                    with open(pf_path, 'r', encoding='utf-8') as f:
                        pdata = json.load(f)
                        all_profiles.append(pdata)
                except Exception as e:
                    print(f"DEBUG: Error loading {pf}: {e}")
                    pass
            
            # Ensure current_profile is always in the list
            if not any(p.get('name') == current_profile.get('name') for p in all_profiles):
                all_profiles.insert(0, current_profile)
                
            new_profile = None
            if target_name:
                # Try to find specific profile
                for p in all_profiles:
                    if p.get('name', '').lower() == target_name:
                        new_profile = p
                        break
                if not new_profile:
                    print(f"DEBUG: Profile '{target_name}' not found")
            
            if not new_profile:
                # Cycle to next
                current_name = current_profile.get('name', '')
                curr_idx = 0
                for idx, p in enumerate(all_profiles):
                    if p.get('name', '') == current_name:
                        curr_idx = idx
                        break
                next_idx = (curr_idx + 1) % len(all_profiles) if all_profiles else 0
                new_profile = all_profiles[next_idx] if all_profiles else current_profile
                print(f"DEBUG: Cycling to next profile: {new_profile.get('name')}")
            
            current_profile = new_profile
            _persist_profile(current_profile)
            return jsonify({'success': True, 'message': f'Switched to profile: {current_profile.get("name")}'})

        elif action_type == 'docker_command':
            success, message, output = execute_docker_command(
                action_config.get('dockerAction', ''),
                action_config.get('container', '')
            )
            return jsonify({
                'success': success,
                'message': message if success else None,
                'output': output if success else message,
                'error': None if success else message
            }), (200 if success else 400)

        elif action_type == 'copy_text':
            success, message = execute_copy_text(action_config.get('text', ''))
            return jsonify({'success': success, 'message': message, 'error': None if success else message}), (200 if success else 400)

        elif action_type in ('custom', 'navigate', 'macro', 'widget'):
            return jsonify({
                'success': False,
                'error': f'Action type "{action_type}" can only run in the browser simulator'
            }), 400

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
    # Make console output encoding-safe on Windows (prevents UnicodeEncodeError
    # with cp1252/cp850 consoles or redirected output when printing emojis)
    for _stream in (sys.stdout, sys.stderr):
        try:
            if _stream and hasattr(_stream, 'reconfigure'):
                _stream.reconfigure(errors='replace')
        except Exception:
            pass

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
