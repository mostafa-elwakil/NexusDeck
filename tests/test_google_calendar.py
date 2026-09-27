"""Tests for Google Calendar OAuth + events (all network mocked).
Run with:  python -m unittest discover -s tests -v
"""
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'streamdeck-simulator', 'server'))

import server


class FakeResponse:
    def __init__(self, status_code, payload):
        self.status_code = status_code
        self._payload = payload

    def json(self):
        return self._payload


EVENTS_PAYLOAD = {
    'items': [
        {'summary': 'Team standup',
         'start': {'dateTime': '2030-05-01T09:30:00+03:00'}},
        {'summary': 'Holiday', 'start': {'date': '2030-05-02'}},
    ]
}


class AuthUrlTest(unittest.TestCase):
    def test_needs_client_id(self):
        with mock.patch.object(server, 'server_settings', {}):
            client = server.app.test_client()
            response = client.get('/api/google/auth-url')
            self.assertEqual(response.status_code, 400)

    def test_builds_consent_url(self):
        settings = {'google_client_id': 'test-id.apps.googleusercontent.com'}
        with mock.patch.object(server, 'server_settings', settings):
            client = server.app.test_client()
            response = client.get('/api/google/auth-url')
            self.assertEqual(response.status_code, 200)
            url = response.get_json()['url']
            self.assertIn('accounts.google.com', url)
            self.assertIn('calendar.readonly', url)
            self.assertIn('state=', url)


class CallbackTest(unittest.TestCase):
    def test_rejects_unknown_state(self):
        client = server.app.test_client()
        response = client.get('/api/google/callback?code=abc&state=nope')
        self.assertIn(b'Failed', response.data)

    def test_exchanges_code_and_stores_refresh(self):
        server._google_oauth_states['good-state'] = 9999999999.0
        settings = {'google_client_id': 'id', 'google_client_secret': 'secret'}
        token = FakeResponse(200, {'refresh_token': 'rt-123',
                                   'access_token': 'at-123', 'expires_in': 3600})
        with mock.patch.object(server, 'server_settings', settings), \
             mock.patch.object(server, '_persist_server_settings', lambda s: None), \
             mock.patch('requests.post', return_value=token):
            client = server.app.test_client()
            response = client.get('/api/google/callback?code=abc&state=good-state')
            self.assertIn(b'Connected', response.data)
            self.assertEqual(settings.get('google_refresh_token'), 'rt-123')


class EventsTest(unittest.TestCase):
    def test_normalizes_events(self):
        settings = {'google_client_id': 'id', 'google_client_secret': 's',
                    'google_refresh_token': 'rt'}
        server._google_access['token'] = 'cached-token'
        server._google_access['expiry'] = 9999999999.0
        server._google_events_cache['events'] = None
        with mock.patch.object(server, 'server_settings', settings), \
             mock.patch('requests.get',
                        return_value=FakeResponse(200, EVENTS_PAYLOAD)):
            events, error = server._google_events(5)
            self.assertIsNone(error)
            self.assertEqual(len(events), 2)
            self.assertEqual(events[0]['summary'], 'Team standup')
            self.assertIn('09:30', events[0]['when'])
            self.assertEqual(events[1]['when'], '02 May')
        server._google_access['token'] = None

    def test_refreshes_expired_token(self):
        settings = {'google_client_id': 'id', 'google_client_secret': 's',
                    'google_refresh_token': 'rt'}
        server._google_access['token'] = None
        server._google_events_cache['events'] = None
        token = FakeResponse(200, {'access_token': 'fresh', 'expires_in': 3600})
        with mock.patch.object(server, 'server_settings', settings), \
             mock.patch('requests.post', return_value=token) as post, \
             mock.patch('requests.get',
                        return_value=FakeResponse(200, {'items': []})):
            events, error = server._google_events(5)
            self.assertIsNone(error)
            self.assertEqual(events, [])
            self.assertTrue(post.called)
        server._google_access['token'] = None

    def test_disconnected_error(self):
        with mock.patch.object(server, 'server_settings', {}):
            server._google_access['token'] = None
            events, error = server._google_events(5)
            self.assertIsNone(events)
            self.assertTrue(error)


class CalendarActionTest(unittest.TestCase):
    def test_action_returns_next_event(self):
        payload = [{'summary': 'Lunch', 'when': '12:30', 'location': ''}]
        with mock.patch.object(server, '_google_events',
                               return_value=(payload, None)):
            client = server.app.test_client()
            response = client.post('/api/execute-action', json={
                'actionType': 'calendar', 'actionData': {}})
            self.assertEqual(response.status_code, 200)
            body = response.get_json()
            self.assertTrue(body['success'])
            self.assertIn('Lunch', body['message'])


if __name__ == '__main__':
    unittest.main()
