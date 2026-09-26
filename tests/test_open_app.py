"""Regression test: apps must launch from their own folder.

Programs like OBS Studio resolve data files (locale/en-US.ini, ...)
relative to the working directory. Launching obs64.exe with arguments
from the server's folder produced "Failed to find locale/en-US.ini".
Run with:  python -m unittest discover -s tests -v
"""
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'streamdeck-simulator', 'server'))

import server


class OpenAppCwdTest(unittest.TestCase):
    def test_popen_uses_exe_directory(self):
        fake_exe = os.path.join('C:\\', 'Program Files', 'obs-studio',
                                'bin', '64bit', 'obs64.exe')
        with mock.patch.object(server, 'resolve_app_executable',
                               return_value=fake_exe), \
             mock.patch('subprocess.Popen') as popen:
            ok, _ = server.execute_open_app('obs64', ['--minimize-to-tray'])
        self.assertTrue(ok)
        _, kwargs = popen.call_args
        self.assertEqual(kwargs.get('cwd'),
                         os.path.dirname(os.path.abspath(fake_exe)))

    def test_exe_without_args_uses_popen_with_cwd(self):
        """No-args exe must NOT go through startfile (inherits server cwd,
        which breaks OBS locale lookup)."""
        fake_exe = os.path.join('C:\\', 'Program Files', 'obs-studio',
                                'bin', '64bit', 'obs64.exe')
        with mock.patch.object(server, 'resolve_app_executable',
                               return_value=fake_exe), \
             mock.patch('subprocess.Popen') as popen, \
             mock.patch.object(os, 'startfile', create=True) as startfile:
            ok, _ = server.execute_open_app('obs64.exe', [])
        self.assertTrue(ok)
        startfile.assert_not_called()
        _, kwargs = popen.call_args
        self.assertEqual(kwargs.get('cwd'),
                         os.path.dirname(os.path.abspath(fake_exe)))

    def test_absolute_exe_path_accepted(self):
        with mock.patch('subprocess.Popen') as popen:
            ok, _ = server.execute_open_app(sys.executable, [])
        self.assertTrue(ok)
        _, kwargs = popen.call_args
        self.assertEqual(kwargs.get('cwd'),
                         os.path.dirname(os.path.abspath(sys.executable)))

    def test_bat_uses_exe_directory(self):
        fake_bat = os.path.join('C:\\', 'tools', 'run.bat')
        with mock.patch.object(server, 'resolve_app_executable',
                               return_value=fake_bat), \
             mock.patch('subprocess.Popen') as popen:
            ok, _ = server.execute_open_app('run', ['arg1'])
        self.assertTrue(ok)
        _, kwargs = popen.call_args
        self.assertEqual(kwargs.get('cwd'),
                         os.path.dirname(os.path.abspath(fake_bat)))


class InstalledAppsTest(unittest.TestCase):
    def test_guess_exe_from_icon(self):
        self.assertEqual(
            server._guess_app_exe(f'"{sys.executable}",0', ''),
            os.path.abspath(sys.executable))

    def test_guess_exe_missing_returns_empty(self):
        self.assertEqual(
            server._guess_app_exe(r'C:\Nope\missing.exe,0', ''), '')

    def test_list_returns_sorted_names(self):
        apps = server._list_installed_apps()
        self.assertIsInstance(apps, list)
        names = [app.get('name', '') for app in apps]
        self.assertEqual(names, sorted(names, key=str.lower))
        for app in apps:
            self.assertIn('exe', app)


if __name__ == '__main__':
    unittest.main()
