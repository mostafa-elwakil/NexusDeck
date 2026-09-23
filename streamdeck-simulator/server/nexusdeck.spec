# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller spec for the NexusDeck companion server.

Builds a standalone executable with the web simulator UI bundled inside:
    cd streamdeck-simulator/server
    pyinstaller streamdeck.spec

Output: dist/NexusDeckCompanion(.exe)
"""
import os

# SPECPATH (directory containing this spec file) is provided by PyInstaller.
SIMULATOR_DIR = os.path.abspath(os.path.join(SPECPATH, '..'))

block_cipher = None

a = Analysis(
    [os.path.join(SPECPATH, 'server.py')],
    pathex=[SPECPATH],
    binaries=[],
    datas=[
        (os.path.join(SIMULATOR_DIR, 'index.html'), '.'),
        (os.path.join(SIMULATOR_DIR, 'css'), 'css'),
        (os.path.join(SIMULATOR_DIR, 'js'), 'js'),
        (os.path.join(SIMULATOR_DIR, 'presets'), 'presets'),
    ],
    hiddenimports=[
        'flask',
        'flask_cors',
        'psutil',
        'requests',
        'obsws_python',
        'websocket',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='NexusDeckCompanion',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    # Windowed (no console window): server logs go to companion.log
    # next to the executable instead. See server.py __main__ redirect.
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)
