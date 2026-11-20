# PR #25 rework - instructions

> **Temporary file.** It exists only so the rework can be finished in a later
> session without the original context. Delete it in the last commit before
> this branch is merged to `main`.

## 0. State on 2026-11-20

- Everything in sections 3 and 4 is done. Decisions: edit action as a footer button, shown
  only when Device Details is opened from the Overview; browser tests removed; details and
  alarm log open for every user, editing for administrators; alarming cards open the Alarm
  Log, which offers a "Details" button when opened from the Overview.
- The branch contains `release/v1.6.0` (merged in) and goes back into it with a merge commit.
- Checked in a browser against the real gateway through the frontend dev server: 35 checks
  passed (cards, edit with entity-ID rename, failing save, alarming card, list page, phone
  width). Not checked: a non-admin login, and the firmware build with this frontend.
- Left to do: section 7 on the flashed build, then delete this file.

## 1. Status

- Branch: `feat/overview-card-dialogs` (integration branch, not `main`).
- Content: `dccdd80` (tip of `main` when the branch was created) plus
  `5c0b331`, PR #25 by an external contributor, squash-merged **into this
  branch**.
- PR #25 is closed as merged. The maintainer's comment on the PR announces the
  changes in summary form and names three open design points:
  https://github.com/hmbacher/genius-gateway/pull/25#issuecomment-6959403699
- Check the PR thread for a reply from the contributor before starting.
- Nothing below is implemented yet. The branch is pushed up to `5c0b331`; this
  file is committed locally only.
- The PR was reviewed by reading the diff and the surrounding code. It was not
  built, `svelte-check` was not run, the new browser tests were not run, and
  the contributor did not test on real hardware.

## 2. What the PR does

- Overview page (`interface/src/routes/+page.svelte`): a detector card no
  longer links to the Smoke Detectors page. A click opens a dialog on top of
  the Overview: **Device Details** for normal cards, the **Alarm Log** for
  alarming cards (`openDevice()`, line 29).
- `DeviceDetailsDialog.svelte`: new optional prop `onEdit`. When set, a pencil
  button appears in the dialog header (lines 190-200) that closes the dialog
  and opens the edit dialog. Used from the Overview and from the list page.
- New module
  `interface/src/routes/gateway/smoke-detectors/deviceSave.svelte.ts`: about
  200 lines moved out of the list page (`+page.svelte` in the same folder) so
  both entry points share the save path: `apiPutDevice`, the fault warning,
  manual line entry for old FM modules, the offer to add an unknown alarm
  line, plus `openEditDevice`.
- Non-admins get plain, non-clickable cards (`{#if $user.admin}`, Overview
  line 97).
- Docs (`docs/features/overview.md`), changelog, and browser tests against a
  mock backend (`tests/test_overview_deeplink.py`, `tests/mock-ui-server.mjs`,
  `tests/Test-OverviewDeepLink.md`).

Checked and found fine:

- The list page no longer references any of the moved functions directly; all
  uses go through the imports from `deviceSave.svelte`.
- The Overview opens `AlarmLog` with the same props as the list page.
- The pencil follows the dialog's existing pattern (`modals.close()` followed
  by the callback), as `onSetLine` and `onReadout` already do.
- `DeviceStatusCard` contains no interactive elements, so wrapping it in a
  `<button>` is valid.
- The test fixture `tests/test-data-overview-card-states.json` has six devices,
  one of them alarming, matching the test's expectations.

## 3. Work items

Line numbers are as of `5c0b331`.

### 3.1 Required

- [ ] **Changelog section.** The entry "Overview Cards Open the Clicked
      Detector" (`CHANGELOG.md:12-17`) sits in `# v1.5.0`, which is released
      (tag exists). Move it to the next release. `main` has no unreleased
      section yet; other branches add `# v1.5.1` (HA topic fix) and `# v1.6.0`.
      Ask the maintainer which one applies when this branch is merged.
- [ ] **Unused imports** in the list page after the extraction:
      `import Edit from '~icons/tabler/pencil'` (line 62) and the type
      `AlarmLines` in the models import (line 15).
- [ ] **"DeepLink" naming.** The final implementation opens a dialog in
      place; nothing is a deep link. Rename
      `tests/Test-OverviewDeepLink.md` and `tests/test_overview_deeplink.py`
      (and the references between them), and drop the stale comment and the
      artificial 600 ms delay in `tests/mock-ui-server.mjs:16`
      ("delayed: deep link must wait for load"). Only if the tests are kept,
      see 4.2.
- [ ] **Docs.** `docs/features/device-management.md`, section
      "Viewing Device Details" (line 237), does not mention the edit action.
      The dialog screenshot
      `docs/assets/images/software/gg-gateway-devices-details.png` becomes
      outdated; the maintainer takes new screenshots. Adjust the wording in
      `docs/features/overview.md:25-30` and the changelog to the final
      decisions in section 4.
- [ ] **Wrong comment.** `openEditDevice`
      (`deviceSave.svelte.ts:207`) is described as "by id" but takes the
      device object.

### 3.2 Recommended

- [ ] **Save failure.** `openEditDevice` (`deviceSave.svelte.ts:208`) ignores
      the result of `apiPutDevice`: it closes the dialog and runs the
      follow-up dialogs even when the save failed. This was the behaviour of
      the old `handleEdit` too, but it now lives in the shared path. Stop on
      failure, as the call at list page line 448 already does.
- [ ] **Module scope.** `deviceSave.svelte.ts` also exports `authHeaders`
      (line 28) and the alarm-line API calls (`fetchAlarmLines`,
      `postAlarmLines`, `registerAlarmLine`, `fetchRegisteredLineIds`). The
      list page imports `authHeaders` from it for delete, reorder and import
      requests that have nothing to do with saving a device. Split this so the
      module name matches its content. The Overview imports the module across
      route folders (`./gateway/smoke-detectors/...`); there is precedent for
      cross-route imports (the list page imports `EditAlarmLine` from
      `../alarm-lines/`), but a shared module under `$lib` would be cleaner.
- [ ] **Remaining duplicates.** Four places in the list page still spell out
      "open `EditSmokeDetector`, PUT, close, `afterDeviceSaved`" by hand
      (lines 313, 448, 680, 725). They differ in title and icon. Consider
      giving `openEditDevice` options for those and reusing it.
- [ ] **Empty location.** The card's `aria-label` (Overview line 101) reads
      "Open details of " without a name when the location is empty. Fall back
      to the serial number.

## 4. Open decisions for the maintainer

Each has a recommendation from the review; none is decided.

### 4.1 Placement of the edit action

The pencil sits in the dialog header right next to the close button. No other
dialog in the project has an action there; actions live in the footer (in this
dialog: "Close" at line 561 and, when `onReadout` is set, "Update" at line
565). Recommendation: an "Edit" button in the footer instead.

On the list page the dialog then offers two routes to the same edit dialog,
because `onSetLine` and `onEdit` both call `handleEdit(index)` (lines
865-866). Check whether that is still wanted.

### 4.2 Keep the browser tests?

They are the first UI tests in the repository and bring Python Playwright and
a mock server with them. `interface/package.json` has no test tooling, only
`check`, `lint` and `format`.

- The mock answers every other REST and WebSocket path with `{}` and will
  break easily when the UI changes.
- The instructions use `npm install --no-package-lock`; the project has a
  `package-lock.json`, so `npm ci` is the right command.
- Both files are written very compactly (one-liners, semicolons), unlike the
  existing suite `tests/test_ha_integration.py`.

If they are kept: rename them (3.1), reformat them, fix the install command.
If not: remove the three files and the references to them.

### 4.3 Admin-only details

Device Details is a read-only view, but non-admins get no click action at all.
Alternative: show the details to everyone and offer the edit action only to
admins. The Smoke Detectors page itself stays admin-only either way.

### 4.4 Alarming cards

An alarming card only opens the Alarm Log. Details and editing are not
reachable from the Overview while a detector is alarming. Decide whether that
is intended.

## 5. Other branches

- `main` and `feat/ha-rename-entity-ids` (rework of PR #24): this branch
  merges cleanly with both and does not depend on the HA topic fix.
- `feat/rf-device-status` (untested at the time of writing): conflicts in two
  files, `interface/src/routes/gateway/smoke-detectors/+page.svelte` and
  `DeviceDetailsDialog.svelte`. Both sides add a new callback to the details
  dialog (`onResetStatus` there, `onEdit` here) and touch the same call in the
  list page. The conflicts are small. That branch also adds `apiResetStatus`
  to the list page, which uses `authHeaders()` and `jsonDateReviver`; after
  this PR `authHeaders` is an import, so keep that in mind when resolving.
  Whichever branch reaches `main` second resolves them. On the Overview the
  details dialog would then have no `onResetStatus`, which matches its
  read-only role.

## 6. Constraints and conventions

- Commits: Conventional Commits (`feat(ui): ...`, `fix(ui): ...`), matching
  the history. No tool or AI attribution anywhere: no `Co-Authored-By`
  trailers, no "generated with" footers, also not in PR texts or comments.
- Hyphens, not em-dashes, in sources and docs.
- Frontend: Prettier with tabs, single quotes, print width 100
  (`interface/.prettierrc`). `npm run check` and `npm run lint` in
  `interface/` are the available checks.
- Modal dialogs show a coloured icon left of the title (`text-primary`,
  `h-7 w-7`), matching the button that opens them.
- The maintainer builds and flashes locally. Do not rely on running the
  PlatformIO build in an agent session; say clearly what was not built or
  tested.
- Do not push, retarget, merge or comment on GitHub without the maintainer's
  go-ahead.
- If shared components under `interface/src/lib/components` are changed, ask
  the maintainer whether the change has to be mirrored to the framework fork.
- If the primary checkout is busy with other work, use a separate git worktree
  for this branch.

## 7. Verification

- [ ] `npm run check` in `interface/` passes (confirms the extraction left no
      dangling references or type errors).
- [ ] On real hardware, as admin: normal card opens Device Details of the
      clicked detector; alarming card opens its Alarm Log; the page stays on
      the Overview.
- [ ] Edit from the Overview: save succeeds, the card updates, the follow-up
      dialogs appear as on the list page (fault warning, manual line entry for
      an old FM module, unknown alarm line).
- [ ] Edit with a failing save: no follow-up dialogs.
- [ ] List page: every edit/add/replace flow still works, since the save path
      was moved.
- [ ] Non-admin: behaviour matches the decision in 4.3.
- [ ] Mobile width: card click and dialogs work.

## 8. Finishing

1. Decisions in section 4 taken, items in section 3 done, test results
   reported to the maintainer.
2. Delete this file.
3. Rebase onto `main` if it has moved; resolve the conflicts from section 5 if
   `feat/rf-device-status` landed first.
4. Open a PR to `main`. Optionally add a short note to PR #25 that the rework
   is complete.
