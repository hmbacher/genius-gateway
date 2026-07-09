/**
 * @file MigrationService.h
 * @brief Central registry and runner for one-shot config migrations.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the Commons Clause restriction.
 *
 * "Commons Clause" License Condition v1.0
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition:
 * Without limiting other conditions in the License, the grant of rights
 * under the License will not include, and the License does not grant to you,
 * the right to Sell the Software.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <functional>
#include <utility>
#include <vector>

/// When a migration runs relative to the framework's service.begin() pass.
enum class MigrationPhase
{
    /// Runs once inside ESP32SvelteKit::begin() after the FS is mounted but
    /// before any settings service reads its persisted config. Use for
    /// file-level transforms (rename, split, reshape on disk).
    PreServiceBegin,
    /// Runs after all services have started. Use for cleanup that depends
    /// on successor files existing (e.g. drop a legacy file once the
    /// services that read its data have rewritten their own).
    PostServiceBegin
};

/// What happens when apply() returns false.
enum class OnFailure
{
    /// Crucial migration. Boot halts with a loud log so the system cannot
    /// run in a half-migrated state. Manual intervention required.
    AbortBoot,
    /// Default. Log a warning, leave the migration unmarked, retry on the
    /// next boot. Use for transient-failure tolerant transforms.
    RetryNextBoot,
    /// Try up to maxAttempts boots, then mark as failed and stop retrying.
    SkipAfterRetries
};

/// One migration registered with MigrationService.
struct Migration
{
    /// Stable, unique, persisted ID. Prefix with the firmware version that
    /// introduced it, e.g. "v1.3-split-mqtt-settings-ha". Never rename.
    const char *id = nullptr;

    /// When in the boot sequence this migration runs.
    MigrationPhase phase = MigrationPhase::PreServiceBegin;

    /// Execution order within a phase (ascending). Ties broken by
    /// registration order.
    uint16_t order = 0;

    /// What to do when apply() fails. Defaults to retry-next-boot.
    OnFailure onFailure = OnFailure::RetryNextBoot;

    /// Only consulted for OnFailure::SkipAfterRetries.
    uint8_t maxAttempts = 3;

    /// Cheap precondition. Return false to skip without marking applied -
    /// e.g. legacy file doesn't exist, or target file already exists.
    std::function<bool(FS *)> shouldRun;

    /// Do the work. Return true on success - only then is the migration
    /// recorded as applied. Should be idempotent where practical.
    std::function<bool(FS *)> apply;
};

/// Registry + runner. Construct one, register migrations, then call
/// runPhase() at the appropriate boot stage. Persists "applied" and
/// "failed" markers in a small JSON state file on the same FS.
class MigrationService
{
public:
    static constexpr const char *STATE_FILE = "/config/migrations.json";
    static constexpr const char *TAG = "Migrations";

    explicit MigrationService(FS *fs);

    /// Filesystem the service evaluates migrations against. Exposed so that
    /// inspection surfaces (e.g. MigrationApi) can re-run shouldRun() to
    /// distinguish "will run next boot" from "preconditions never met".
    FS *getFS() const { return _fs; }

    /// All registered migrations, regardless of applied/failed state. Order
    /// is registration order. Use the phase/order fields on each entry for
    /// execution ordering.
    const std::vector<Migration> &all() const { return _all; }

    /// Register a migration. Safe to call before the FS is mounted; the FS
    /// is only touched during runPhase().
    void add(Migration migration);

    /// Run every migration matching `phase` whose ID is not yet marked
    /// applied and whose shouldRun() returns true, in ascending `order`.
    void runPhase(MigrationPhase phase);

    /// Drop every entry from the failed list and persist. Migrations that
    /// were given up under SkipAfterRetries become eligible to retry on
    /// next boot; no migrations run synchronously here.
    void clearFailures();

    /// Read-only inspection - for future REST/UI surface.
    struct AppliedRecord
    {
        String id;
        String appliedAt;
    };
    struct FailedRecord
    {
        String id;
        uint8_t attempts = 0;
        String lastError;
    };
    const std::vector<AppliedRecord> &applied() const { return _applied; }
    const std::vector<FailedRecord> &failed() const { return _failed; }
    std::vector<const Migration *> pending(MigrationPhase phase) const;

private:
    FS *_fs;
    std::vector<Migration> _all;
    std::vector<AppliedRecord> _applied;
    std::vector<FailedRecord> _failed;
    bool _stateLoaded = false;

    void _loadState();
    bool _saveState();
    bool _isApplied(const char *id) const;
    void _markApplied(const char *id);
    uint8_t _attemptsOf(const char *id) const;
    void _recordFailure(const char *id, const char *reason);
    void _clearFailure(const char *id);
    [[noreturn]] void _abortBoot(const char *id);
};
