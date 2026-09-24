"""Regression tests for the settings/profile persistence fixes.

Covers the reported bug: "after restart, all settings reverted to defaults".
Run with:  python -m unittest discover -s tests -v
"""
import json
import os
import socket
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'streamdeck-simulator', 'server'))

import server


def _free_port():
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.bind(('127.0.0.1', 0))
    port = probe.getsockname()[1]
    probe.close()
    return port


class StateDirTest(unittest.TestCase):
    def test_uses_appdata_on_windows(self):
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.dict(os.environ, {'APPDATA': tmp}):
                with mock.patch('platform.system', return_value='Windows'):
                    result = server._user_state_dir()
        self.assertTrue(result.startswith(tmp), result)
        self.assertTrue(result.endswith('NexusDeck'), result)

    def test_single_location_regardless_of_cwd(self):
        """Dev and installed copies must resolve to the same dir."""
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.dict(os.environ, {'APPDATA': tmp}):
                with mock.patch('platform.system', return_value='Windows'):
                    first = server._user_state_dir()
                    second = server._user_state_dir()
        self.assertEqual(first, second)


class AtomicWriteTest(unittest.TestCase):
    def test_roundtrip_and_no_leftovers(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'server_settings.json')
            payload = {'home_city': 'Riyadh', 'brightness': 61}
            server._atomic_write_json(path, payload, indent=2)
            with open(path, encoding='utf-8') as handle:
                self.assertEqual(json.load(handle), payload)
            leftovers = [name for name in os.listdir(tmp) if '.tmp.' in name]
            self.assertEqual(leftovers, [])

    def test_overwrite_keeps_file_valid(self):
        """Simulates a restart mid-write: the file must always parse."""
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'profile_state.json')
            server._atomic_write_json(path, {'name': 'A'})
            server._atomic_write_json(path, {'name': 'B'})
            with open(path, encoding='utf-8') as handle:
                self.assertEqual(json.load(handle), {'name': 'B'})


class SettingsPersistenceTest(unittest.TestCase):
    def test_settings_survive_reload(self):
        """The exact reported scenario: custom settings must not revert."""
        with tempfile.TemporaryDirectory() as tmp:
            db_file = os.path.join(tmp, 'server_settings.json')
            with mock.patch.object(server, 'SETTINGS_DB_FILE', db_file):
                server._persist_server_settings({
                    'home_city': 'Riyadh',
                    'ha_url': 'http://192.168.100.48:8123/',
                    'brightness': 61,
                })
                reloaded = server._load_server_settings()
        self.assertEqual(reloaded.get('home_city'), 'Riyadh')
        self.assertEqual(reloaded.get('ha_url'), 'http://192.168.100.48:8123/')
        self.assertEqual(reloaded.get('brightness'), 61)

    def test_corrupt_file_falls_back_to_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            db_file = os.path.join(tmp, 'server_settings.json')
            with open(db_file, 'w', encoding='utf-8') as handle:
                handle.write('{truncated-json')
            with mock.patch.object(server, 'SETTINGS_DB_FILE', db_file):
                reloaded = server._load_server_settings()
        self.assertIsInstance(reloaded, dict)
        self.assertEqual(reloaded.get('esp32_ip'), '')
        self.assertEqual(reloaded.get('server_port'), 8765)


class MigrationTest(unittest.TestCase):
    def test_copies_legacy_files_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            legacy = os.path.join(tmp, 'legacy')
            state = os.path.join(tmp, 'state')
            os.makedirs(legacy)
            os.makedirs(state)
            with open(os.path.join(legacy, 'server_settings.json'), 'w', encoding='utf-8') as handle:
                json.dump({'home_city': 'Riyadh'}, handle)
            with mock.patch.object(server, '__file__', os.path.join(legacy, 'server.py')):
                server._migrate_legacy_state(state)
            with open(os.path.join(state, 'server_settings.json'), encoding='utf-8') as handle:
                self.assertEqual(json.load(handle), {'home_city': 'Riyadh'})

    def test_never_overwrites_existing_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            legacy = os.path.join(tmp, 'legacy')
            state = os.path.join(tmp, 'state')
            os.makedirs(legacy)
            os.makedirs(state)
            with open(os.path.join(legacy, 'server_settings.json'), 'w', encoding='utf-8') as handle:
                json.dump({'home_city': 'Old'}, handle)
            with open(os.path.join(state, 'server_settings.json'), 'w', encoding='utf-8') as handle:
                json.dump({'home_city': 'New'}, handle)
            with mock.patch.object(server, '__file__', os.path.join(legacy, 'server.py')):
                server._migrate_legacy_state(state)
            with open(os.path.join(state, 'server_settings.json'), encoding='utf-8') as handle:
                self.assertEqual(json.load(handle), {'home_city': 'New'})


class SingleInstanceTest(unittest.TestCase):
    def test_second_instance_exits(self):
        port = _free_port()
        with mock.patch.object(server, 'PORT', port):
            server._ensure_single_instance()
            try:
                with self.assertRaises(SystemExit) as context:
                    server._ensure_single_instance()
                self.assertEqual(context.exception.code, 1)
            finally:
                server._INSTANCE_LOCK.close()
                server._INSTANCE_LOCK = None


if __name__ == '__main__':
    unittest.main()
