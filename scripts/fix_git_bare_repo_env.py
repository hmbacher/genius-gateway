"""
Workaround: neutralize an injected git `safe.bareRepository=explicit` setting.

Why
---
Some agent/sandbox terminals (e.g. the Claude Code VS Code terminal) export git
configuration through the GIT_CONFIG_COUNT / GIT_CONFIG_KEY_* / GIT_CONFIG_VALUE_*
environment variables, including `safe.bareRepository=explicit`. These env entries
take precedence over every git config file, so they cannot be overridden with
`git config --global safe.bareRepository all`.

With that setting active, git refuses to operate on a *bare* repository that is
discovered via the working directory (no explicit --git-dir). The ESP-IDF component
manager keeps its git-sourced dependency (espressif/fb_gfx, pulled by the Arduino
hybrid `custom_sdkconfig` build) in a bare mirror under
<cache>/Espressif/ComponentManager/Cache/b_git_* and probes it with
`git config --get remote.origin.url` using cwd=<bare>. The probe then fails with
"fatal: not a git repository", and ESP-IDF's tools/cmake/build.cmake escalates that
to a fatal error during the dependency solve:

    fatal: not a git repository (or any of the parent directories): .git
    ERROR: 'git config --get remote.origin.url' failed with exit code 1

That aborts the hybrid sdkconfig regeneration. (Disabling the component manager
"fixes" the git error but strips component Kconfig such as CONFIG_LITTLEFS_PAGE_SIZE
/ CONFIG_MDNS_MAX_INTERFACES from the generated sdkconfig.h, which then breaks the
firmware build with 'LittleFS'/'MDNS' was not declared.)

We simply drop the injected git env entries for this build and the child IDF / CMake
/ component-manager processes. This is a no-op in environments that do not inject
them (ordinary terminals, CI).
"""

Import("env")  # noqa: F821  (provided by PlatformIO)
import os

if os.environ.get("GIT_CONFIG_COUNT"):
    _injected = ["GIT_CONFIG_COUNT"]
    try:
        _injected += [
            f"GIT_CONFIG_KEY_{i}" for i in range(int(os.environ["GIT_CONFIG_COUNT"]))
        ]
        _injected += [
            f"GIT_CONFIG_VALUE_{i}" for i in range(int(os.environ["GIT_CONFIG_COUNT"]))
        ]
    except ValueError:
        pass

    for _var in _injected:
        os.environ.pop(_var, None)
        # Also strip from the SCons-spawned subprocess environment so the ESP-IDF
        # CMake / component-manager build inherits the cleaned env.
        env["ENV"].pop(_var, None)

    print(
        "fix_git_bare_repo_env: dropped injected GIT_CONFIG_* "
        "(safe.bareRepository) so the IDF component manager can read its git cache"
    )
