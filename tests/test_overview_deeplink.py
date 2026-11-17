"""Browser tests for the Overview detector cards: a click opens Device Details / Alarm Log
on top of the Overview (no navigation to the Smoke Detectors page).

See tests/Test-OverviewDeepLink.md for how to run them against tests/mock-ui-server.mjs.
"""
import os, sys
from playwright.sync_api import sync_playwright, expect

BASE = os.environ.get('GG_UI_URL', 'http://localhost:4173')
CHROME = os.environ.get('GG_CHROME')  # optional path to a Chrome/Chromium binary
fixture_cards = 6                      # tests/test-data-overview-card-states.json
HEALTHY_LOCATION, ALARMING_LOCATION = 'Test 1 - Healthy', None  # alarming card is found via its aria-label

def login(ctx, admin=True):
    ctx.add_init_script("localStorage.setItem('user', JSON.stringify({bearer_token:'t', username:'u', admin:%s}))" % ('true' if admin else 'false'))

results = []
def check(name, fn):
    try: fn(); results.append((name, True, ''))
    except Exception as e: results.append((name, False, str(e).splitlines()[0][:200]))

def on_overview(pg):
    assert pg.url.rstrip('/') == BASE, f'left the overview: {pg.url}'

with sync_playwright() as p:
    b = p.chromium.launch(executable_path=CHROME, args=['--no-sandbox'])

    def new_page(admin=True, width=1280, height=900):
        ctx = b.new_context(viewport={'width': width, 'height': height}); login(ctx, admin)
        pg = ctx.new_page(); pg.goto(BASE + '/')
        pg.wait_for_selector('button[aria-label^="Open"]' if admin else 'text=Test 1 - Healthy')
        return ctx, pg

    def t_cards():
        ctx, pg = new_page()
        labels = [x.get_attribute('aria-label') for x in pg.query_selector_all('button[aria-label^="Open"]')]
        assert len(labels) == fixture_cards, labels
        assert sum(l.startswith('Open alarm log') for l in labels) == 1, labels
        assert pg.locator('a[href*="smoke-detectors"]:visible', has=pg.locator('xpath=self::*[ancestor::main]')).count() == 0
        ctx.close()
    check('cards are buttons: one "alarm log", the rest "details"', t_cards)

    def t_details():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open details of Test 1 - Healthy"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        on_overview(pg)
        assert pg.get_by_text('Test 1 - Healthy').locator('visible=true').count() >= 2, 'dialog should show the clicked detector over the overview cards'
        ctx.close()
    check('normal card -> Device Details dialog, stays on the overview', t_details)

    def t_alarm():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open alarm log"]:visible').first.click()
        expect(pg.get_by_text('Alarms log').first).to_be_visible(timeout=8000)
        on_overview(pg)
        ctx.close()
    check('alarming card -> Alarm Log dialog, stays on the overview', t_alarm)

    def t_readonly():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open details of Test 1 - Healthy"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        assert pg.get_by_text('Set Alarm Line manually').count() == 0
        assert pg.get_by_role('button', name='Update', exact=True).count() == 0
        ctx.close()
    check('overview details dialog is read-only (no readout "Update" button, no set-line)', t_readonly)

    def t_single_and_close():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open details of Test 1 - Healthy"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        pg.wait_for_timeout(1000)
        assert pg.get_by_text('Device Details').count() == 1, 'expected exactly one dialog'
        pg.locator('button:visible', has_text='Close').first.click(timeout=5000)
        expect(pg.get_by_text('Device Details')).to_have_count(0, timeout=5000)
        on_overview(pg)
        expect(pg.locator('button[aria-label^="Open"]:visible').first).to_be_visible()
        ctx.close()
    check('exactly one dialog; closing returns to the untouched overview', t_single_and_close)

    def t_non_admin():
        ctx, pg = new_page(admin=False)
        assert pg.locator('button[aria-label^="Open"]').count() == 0, 'non-admin cards must not be clickable'
        ctx.close()
    check('non-admin: cards are not clickable', t_non_admin)

    def t_mobile():
        ctx, pg = new_page(width=390, height=800)
        pg.locator('button[aria-label^="Open details of Test 1 - Healthy"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        on_overview(pg)
        ctx.close()
    check('mobile viewport: card click opens details on the overview', t_mobile)

    def t_list_untouched():
        ctx = b.new_context(viewport={'width': 1280, 'height': 900}); login(ctx)
        pg = ctx.new_page(); pg.goto(BASE + '/gateway/smoke-detectors')
        expect(pg.get_by_text('Test 1 - Healthy').locator('visible=true').first).to_be_visible(timeout=8000)
        assert pg.get_by_text('Device Details').count() == 0 and pg.get_by_text('Alarms log').count() == 0
        ctx.close()
    check('smoke detector list opens no dialog by itself (regression)', t_list_untouched)

    def edit_flow(pg, expect_url_suffix):
        pg.locator('button[aria-label="Edit smoke detector"]:visible', has_text='').last.click()
        expect(pg.get_by_text('Edit smoke detector').first).to_be_visible(timeout=8000)
        expect(pg.get_by_text('Device Details')).to_have_count(0, timeout=5000)
        assert pg.url.rstrip('/') == BASE + expect_url_suffix, f'unexpected url {pg.url}'

    def t_edit_overview():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open details of Test 1 - Healthy"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        edit_flow(pg, '')
        pg.get_by_role('button', name='Save').locator('visible=true').first.click()
        expect(pg.get_by_text('Smoke detector saved.')).to_be_visible(timeout=8000)
        on_overview(pg)
        assert len(ctx.request.get(BASE + '/__puts').json()) == 1, 'expected exactly one PUT'
        ctx.close()
    check('overview details: pencil opens edit dialog; save PUTs and stays on the overview', t_edit_overview)

    def t_edit_list():
        ctx = b.new_context(viewport={'width': 1280, 'height': 900}); login(ctx)
        pg = ctx.new_page(); pg.goto(BASE + '/gateway/smoke-detectors')
        expect(pg.get_by_text('Test 1 - Healthy').locator('visible=true').first).to_be_visible(timeout=8000)
        pg.locator('button[aria-label="Device details"]:visible').first.click()
        expect(pg.get_by_text('Device Details').first).to_be_visible(timeout=8000)
        edit_flow(pg, '/gateway/smoke-detectors')
        ctx.close()
    check('list page details: pencil opens edit dialog', t_edit_list)

    def t_no_pencil_alarm_log():
        ctx, pg = new_page()
        pg.locator('button[aria-label^="Open alarm log"]:visible').first.click()
        expect(pg.get_by_text('Alarms log').first).to_be_visible(timeout=8000)
        assert pg.locator('button[aria-label="Edit smoke detector"]').count() == 0
        ctx.close()
    check('alarm log dialog has no edit pencil', t_no_pencil_alarm_log)

    b.close()

for n, ok, msg in results: print(('PASS ' if ok else 'FAIL ') + n + (('  -> ' + msg) if msg else ''))
sys.exit(0 if all(ok for _, ok, _ in results) else 1)
