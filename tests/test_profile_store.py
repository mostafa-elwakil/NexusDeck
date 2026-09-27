"""Regression tests: switching by name must open the customized profile,
not the pristine preset file.
Run with:  python -m unittest discover -s tests -v
"""
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'streamdeck-simulator', 'server'))

import server

CUSTOM = {
    'name': 'OBS Studio',
    'buttons': [{'label': 'MY-CUSTOM-BUTTON', 'action': {'type': 'open_app', 'app': 'code'}}],
}


class OverlayTest(unittest.TestCase):
    def test_customization_overrides_preset(self):
        with mock.patch.object(server, 'known_profiles', {'OBS Studio': CUSTOM}), \
             mock.patch.object(server, 'current_profile', {'name': 'Other', 'buttons': []}):
            profiles = server._list_all_profiles()
        match = next(p for p in profiles if p.get('name') == 'OBS Studio')
        self.assertEqual(match['buttons'][0]['label'], 'MY-CUSTOM-BUTTON')

    def test_valid_stored_profile(self):
        self.assertTrue(server._valid_stored_profile(CUSTOM))
        self.assertFalse(server._valid_stored_profile({'name': 'X'}))
        self.assertFalse(server._valid_stored_profile({'name': '', 'buttons': []}))
        self.assertFalse(server._valid_stored_profile(None))


class SetProfileRemembersTest(unittest.TestCase):
    def test_push_is_remembered(self):
        with mock.patch.object(server, '_persist_profile', lambda p: None), \
             mock.patch.object(server, '_persist_profiles_store', lambda: None), \
             mock.patch.object(server, 'known_profiles', {}):
            client = server.app.test_client()
            response = client.post('/api/set-profile', json=dict(CUSTOM))
            self.assertEqual(response.status_code, 200)
            self.assertIn('OBS Studio', server.known_profiles)
            self.assertEqual(
                server.known_profiles['OBS Studio']['buttons'][0]['label'],
                'MY-CUSTOM-BUTTON')
            # restore live state
            server.current_profile = server._load_persisted_profile()


class SwitchOpensCustomizedTest(unittest.TestCase):
    def test_switch_by_name_gets_custom(self):
        original = server.current_profile
        try:
            with mock.patch.object(server, '_persist_profile', lambda p: None), \
                 mock.patch.object(server, 'known_profiles', {'OBS Studio': CUSTOM}), \
                 mock.patch.object(server, 'current_profile', {'name': 'Other', 'buttons': []}):
                client = server.app.test_client()
                response = client.post('/api/execute-action', json={
                    'actionType': 'switch_profile',
                    'actionData': {'name': 'OBS Studio'},
                })
                self.assertEqual(response.status_code, 200)
                self.assertTrue(response.get_json().get('success'))
                self.assertEqual(
                    server.current_profile['buttons'][0]['label'], 'MY-CUSTOM-BUTTON')
        finally:
            server.current_profile = original


if __name__ == '__main__':
    unittest.main()
