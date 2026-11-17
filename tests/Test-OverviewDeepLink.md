# Test: Overview cards open the clicked detector

Clicking a detector card on the Overview page opens that detector on top of the Overview.
The page does not change (the URL stays on `/`):

| Card | Result |
|------|--------|
| normal | **Device Details** dialog of that detector (pencil in the header opens the edit dialog) |
| alarming | **Alarm Log** dialog of that detector |

Acoustic readout ("Update") and "Set Alarm Line manually" are not offered here, they stay on the
Smoke Detectors page. Details and alarm log are admin-only: non-admin users see plain,
non-clickable cards.

## Automated (no hardware needed)

The test drives the production UI build against a small mock backend that serves
`tests/test-data-overview-card-states.json` (covers healthy, warning, and alarming cards).

```bash
cd interface && npm install --no-package-lock && npm run build && cd ..
node tests/mock-ui-server.mjs interface/build tests/test-data-overview-card-states.json 4173 &
pip install playwright            # needs a Chrome/Chromium; set GG_CHROME=/path/to/chrome if not bundled
python3 tests/test_overview_deeplink.py   # GG_UI_URL overrides http://localhost:4173
```

Covered: card buttons per state (details vs. alarm log), the right dialog opens for the clicked
detector while the URL stays on the Overview, exactly one dialog opens, closing it returns to the
untouched Overview, the details dialog is read-only, non-admin cards are not clickable, mobile
viewport, and the Smoke Detectors list still opens no dialog on its own.

## Manual (real device)

1. Open the Overview with at least one healthy and one alarming (or test-imported) detector.
2. Click a healthy card: the **Device Details** dialog of exactly that detector opens over the Overview, the page behind stays the Overview.
3. Close the dialog: you are still on the Overview.
4. Click the alarming card: the **Alarm Log** of that detector opens, again over the Overview.
5. Log in as a non-admin user: cards are shown but are not clickable.
