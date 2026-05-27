/**
 * @file MigrationService.cpp
 * @brief Implementation of the central migration registry and runner.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <migrations/MigrationService.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AppVersion.h>
#include <algorithm>
#include <esp_log.h>

MigrationService::MigrationService(FS *fs) : _fs(fs) {}

void MigrationService::add(Migration migration)
{
    _all.push_back(std::move(migration));
}

void MigrationService::runPhase(MigrationPhase phase)
{
    _loadState();

    // Stable-sort indices by `order` so ties fall back to registration order.
    std::vector<size_t> idx;
    idx.reserve(_all.size());
    for (size_t i = 0; i < _all.size(); ++i)
        idx.push_back(i);
    std::stable_sort(idx.begin(), idx.end(), [this](size_t a, size_t b) {
        return _all[a].order < _all[b].order;
    });

    bool stateDirty = false;

    for (size_t i : idx)
    {
        Migration &m = _all[i];
        if (m.phase != phase)
            continue;
        if (!m.id)
        {
            ESP_LOGW(TAG, "migration without id at slot %u — skipping", (unsigned)i);
            continue;
        }
        if (_isApplied(m.id))
        {
            ESP_LOGD(TAG, "skip %s (already applied)", m.id);
            continue;
        }
        if (m.shouldRun && !m.shouldRun(_fs))
        {
            ESP_LOGD(TAG, "skip %s (precondition false)", m.id);
            continue;
        }

        ESP_LOGI(TAG, "applying %s", m.id);
        bool ok = m.apply ? m.apply(_fs) : false;

        if (ok)
        {
            ESP_LOGI(TAG, "applied %s", m.id);
            _markApplied(m.id);
            _clearFailure(m.id);
            stateDirty = true;
            continue;
        }

        switch (m.onFailure)
        {
        case OnFailure::AbortBoot:
            _recordFailure(m.id, "apply returned false");
            _saveState();
            _abortBoot(m.id);
            break;
        case OnFailure::RetryNextBoot:
            _recordFailure(m.id, "apply returned false");
            stateDirty = true;
            ESP_LOGW(TAG, "migration %s failed — will retry next boot", m.id);
            break;
        case OnFailure::SkipAfterRetries:
        {
            _recordFailure(m.id, "apply returned false");
            stateDirty = true;
            uint8_t attempts = _attemptsOf(m.id);
            if (attempts >= m.maxAttempts)
                ESP_LOGE(TAG, "migration %s reached max attempts (%u) — giving up",
                         m.id, (unsigned)m.maxAttempts);
            else
                ESP_LOGW(TAG, "migration %s failed (attempt %u/%u) — retrying next boot",
                         m.id, (unsigned)attempts, (unsigned)m.maxAttempts);
            break;
        }
        }
    }

    if (stateDirty)
        _saveState();
}

void MigrationService::clearFailures()
{
    _loadState();
    if (_failed.empty())
        return;
    _failed.clear();
    _saveState();
    ESP_LOGI(TAG, "cleared all failure records");
}

std::vector<const Migration *> MigrationService::pending(MigrationPhase phase) const
{
    std::vector<const Migration *> out;
    for (const auto &m : _all)
    {
        if (m.phase != phase)
            continue;
        if (m.id && _isApplied(m.id))
            continue;
        out.push_back(&m);
    }
    return out;
}

void MigrationService::_loadState()
{
    if (_stateLoaded)
        return;
    _stateLoaded = true;

    if (!_fs->exists(STATE_FILE))
        return;

    File in = _fs->open(STATE_FILE, "r");
    if (!in)
        return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, in);
    in.close();
    if (err)
    {
        ESP_LOGW(TAG, "state file %s parse error: %s — ignoring", STATE_FILE, err.c_str());
        return;
    }

    for (JsonObject obj : doc["applied"].as<JsonArray>())
    {
        AppliedRecord rec;
        rec.id = obj["id"] | "";
        rec.appliedAt = obj["appliedAt"] | "";
        if (!rec.id.isEmpty())
            _applied.push_back(rec);
    }
    for (JsonObject obj : doc["failed"].as<JsonArray>())
    {
        FailedRecord rec;
        rec.id = obj["id"] | "";
        rec.attempts = obj["attempts"] | 0;
        rec.lastError = obj["lastError"] | "";
        if (!rec.id.isEmpty())
            _failed.push_back(rec);
    }
}

bool MigrationService::_saveState()
{
    JsonDocument doc;
    JsonArray appliedArr = doc["applied"].to<JsonArray>();
    for (const auto &a : _applied)
    {
        JsonObject obj = appliedArr.add<JsonObject>();
        obj["id"] = a.id;
        obj["appliedAt"] = a.appliedAt;
    }
    JsonArray failedArr = doc["failed"].to<JsonArray>();
    for (const auto &f : _failed)
    {
        JsonObject obj = failedArr.add<JsonObject>();
        obj["id"] = f.id;
        obj["attempts"] = f.attempts;
        obj["lastError"] = f.lastError;
    }

    File out = _fs->open(STATE_FILE, "w");
    if (!out)
    {
        ESP_LOGE(TAG, "could not open %s for writing", STATE_FILE);
        return false;
    }
    size_t written = serializeJson(doc, out);
    out.close();
    if (written == 0)
    {
        ESP_LOGE(TAG, "failed to serialize state to %s", STATE_FILE);
        return false;
    }
    return true;
}

bool MigrationService::_isApplied(const char *id) const
{
    for (const auto &a : _applied)
        if (a.id == id)
            return true;
    return false;
}

void MigrationService::_markApplied(const char *id)
{
    if (_isApplied(id))
        return;
    AppliedRecord rec;
    rec.id = id;
    rec.appliedAt = APP_VERSION_FULL;
    _applied.push_back(rec);
}

uint8_t MigrationService::_attemptsOf(const char *id) const
{
    for (const auto &f : _failed)
        if (f.id == id)
            return f.attempts;
    return 0;
}

void MigrationService::_recordFailure(const char *id, const char *reason)
{
    for (auto &f : _failed)
    {
        if (f.id == id)
        {
            f.attempts++;
            f.lastError = reason ? String(reason) : String();
            return;
        }
    }
    FailedRecord rec;
    rec.id = id;
    rec.attempts = 1;
    rec.lastError = reason ? String(reason) : String();
    _failed.push_back(rec);
}

void MigrationService::_clearFailure(const char *id)
{
    _failed.erase(std::remove_if(_failed.begin(), _failed.end(),
                                 [&](const FailedRecord &f) { return f.id == id; }),
                  _failed.end());
}

void MigrationService::_abortBoot(const char *id)
{
    ESP_LOGE(TAG, "================================================");
    ESP_LOGE(TAG, "  CRITICAL MIGRATION FAILED: %s", id);
    ESP_LOGE(TAG, "  Boot halted to avoid running on inconsistent state.");
    ESP_LOGE(TAG, "  Manual intervention required.");
    ESP_LOGE(TAG, "================================================");
    while (true)
    {
        delay(10000);
        ESP_LOGE(TAG, "boot halted — critical migration %s failed", id);
    }
}
