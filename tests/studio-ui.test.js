/**
 * Tests for StudioUI pure helpers (no DOM needed).
 * Run with:  node --test tests/studio-ui.test.js
 */
const { describe, it } = require('node:test');
const assert = require('node:assert/strict');

const StudioUI = require('../streamdeck-simulator/js/studio-ui.js');

function makeCtx(names) {
    return {
        profiles: { profiles: names.map((name) => ({ name })) },
        escapeHtml: (value) => String(value ?? '')
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;'),
    };
}

describe('profileOptions', () => {
    it('lists profiles with the current one selected', () => {
        const html = StudioUI.prototype.profileOptions.call(
            makeCtx(['Media Control', 'OBS Studio']), 'OBS Studio');
        assert.ok(html.includes('value="OBS Studio" selected'));
        assert.ok(html.includes('value="Media Control"'));
    });

    it('dedupes repeated names', () => {
        const html = StudioUI.prototype.profileOptions.call(
            makeCtx(['A', 'B', 'A']), '');
        assert.equal((html.match(/<option/g) || []).length, 2);
    });

    it('handles no profiles', () => {
        const html = StudioUI.prototype.profileOptions.call(makeCtx([]), '');
        assert.equal(html, '');
    });
});

describe('suggestLabelIcon', () => {
    const suggest = (action) => StudioUI.prototype.suggestLabelIcon.call({}, action);

    it('names apps and picks known icons', () => {
        assert.deepEqual(suggest({ type: 'open_app', app: 'obs64.exe' }),
            { label: 'Obs64', icon: '🎥' });
        assert.deepEqual(suggest({ type: 'open_app', app: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe' }),
            { label: 'Chrome', icon: '🌐' });
    });

    it('uses domains for urls', () => {
        assert.deepEqual(suggest({ type: 'open_url', url: 'https://www.youtube.com/watch?v=1' }),
            { label: 'youtube.com', icon: '🌐' });
    });

    it('names profiles and cycles', () => {
        assert.deepEqual(suggest({ type: 'switch_profile', name: 'Media Control' }),
            { label: 'Media Control', icon: '🔄' });
        assert.deepEqual(suggest({ type: 'switch_profile', name: '' }),
            { label: 'Switch Profile', icon: '🔄' });
    });

    it('summarizes commands, shortcuts and entities', () => {
        assert.deepEqual(suggest({ type: 'run_command', command: 'notepad notes.txt' }),
            { label: 'notepad', icon: '⌨️' });
        assert.deepEqual(suggest({ type: 'keyboard_shortcut', keys: 'ctrl+shift+t' }),
            { label: 'ctrl+shift+t', icon: '⌨️' });
        assert.deepEqual(suggest({ type: 'home_assistant', entity_id: 'light.living_room' }),
            { label: 'Living Room', icon: '🏠' });
    });

    it('labels obs operations', () => {
        assert.deepEqual(suggest({ type: 'obs_control', operation: 'start_recording' }),
            { label: 'Start Rec', icon: '⏺️' });
        assert.deepEqual(suggest({ type: 'obs_control', operation: 'set_scene', scene: 'Game' }),
            { label: 'Game', icon: '🎬' });
    });

    it('labels calendar and music', () => {
        assert.deepEqual(suggest({ type: 'calendar' }),
            { label: 'Calendar', icon: '📅' });
        assert.deepEqual(suggest({ type: 'music' }),
            { label: 'Music', icon: '🎵' });
    });

    it('falls back safely', () => {
        assert.deepEqual(suggest(null), { label: '', icon: '' });
        assert.deepEqual(suggest({ type: 'macro' }), { label: 'Macro', icon: '⚙️' });
    });
});

describe('profileNames', () => {
    it('dedupes and drops empties', () => {
        const names = StudioUI.prototype.profileNames.call(
            makeCtx(['B', 'A', 'B', '']));
        assert.deepEqual(names, ['B', 'A']);
    });

    it('handles missing manager', () => {
        assert.deepEqual(
            StudioUI.prototype.profileNames.call({}), []);
    });
});

describe('keyComboFromEvent', () => {
    const combo = (event) => StudioUI.prototype.keyComboFromEvent.call({}, event);

    it('builds ctrl+shift+s', () => {
        assert.deepEqual(
            combo({ key: 'S', ctrlKey: true, shiftKey: true }),
            { combo: 'ctrl+shift+s' });
    });

    it('handles win+l and function keys', () => {
        assert.deepEqual(combo({ key: 'l', metaKey: true }), { combo: 'win+l' });
        assert.deepEqual(combo({ key: 'F5' }), { combo: 'f5' });
    });

    it('maps special and media keys', () => {
        assert.deepEqual(combo({ key: ' ' }), { combo: 'space' });
        assert.deepEqual(combo({ key: 'ArrowUp', altKey: true }), { combo: 'alt+up' });
        assert.deepEqual(combo({ key: 'MediaPlayPause' }), { combo: 'play_pause' });
        assert.deepEqual(combo({ key: 'Enter', ctrlKey: true }), { combo: 'ctrl+enter' });
    });

    it('stays armed on pure modifiers, cancels on bare Esc', () => {
        assert.deepEqual(combo({ key: 'Control', ctrlKey: true }),
            { armed: true, hint: 'ctrl' });
        assert.equal(combo({ key: 'Escape' }), null);
        assert.equal(combo({ key: 'Dead' }), null);
    });
});
