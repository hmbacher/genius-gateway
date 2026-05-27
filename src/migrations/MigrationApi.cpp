/**
 * @file MigrationApi.cpp
 * @brief REST surface for inspecting and retrying config migrations.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <migrations/MigrationApi.h>
#include <migrations/MigrationService.h>

#include <ArduinoJson.h>
#include <esp_log.h>
#include <set>

namespace
{
const char *phaseName(MigrationPhase p)
{
    switch (p)
    {
    case MigrationPhase::PreServiceBegin:
        return "pre";
    case MigrationPhase::PostServiceBegin:
        return "post";
    }
    return "unknown";
}

const char *onFailureName(OnFailure f)
{
    switch (f)
    {
    case OnFailure::AbortBoot:
        return "abortBoot";
    case OnFailure::RetryNextBoot:
        return "retryNextBoot";
    case OnFailure::SkipAfterRetries:
        return "skipAfterRetries";
    }
    return "unknown";
}
} // namespace

MigrationApi::MigrationApi(PsychicHttpServer *server,
                           SecurityManager *securityManager,
                           MigrationService *service)
    : _server(server), _securityManager(securityManager), _service(service)
{
}

void MigrationApi::begin()
{
    _server->on(MIGRATIONS_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(
                    std::bind(&MigrationApi::_handleList, this, std::placeholders::_1),
                    AuthenticationPredicates::IS_AUTHENTICATED));
    _server->on(MIGRATIONS_RETRY_PATH,
                HTTP_POST,
                _securityManager->wrapRequest(
                    std::bind(&MigrationApi::_handleRetry, this, std::placeholders::_1),
                    AuthenticationPredicates::IS_ADMIN));
    ESP_LOGV(TAG, "Registered %s (GET) and %s (POST)", MIGRATIONS_PATH, MIGRATIONS_RETRY_PATH);
}

esp_err_t MigrationApi::_handleList(PsychicRequest *request)
{
    JsonDocument doc;
    JsonArray arr = doc["migrations"].to<JsonArray>();

    FS *fs = _service->getFS();
    const auto &appliedList = _service->applied();
    const auto &failedList = _service->failed();

    auto findApplied = [&](const String &id) -> const MigrationService::AppliedRecord * {
        for (const auto &a : appliedList)
            if (a.id == id)
                return &a;
        return nullptr;
    };
    auto findFailed = [&](const String &id) -> const MigrationService::FailedRecord * {
        for (const auto &f : failedList)
            if (f.id == id)
                return &f;
        return nullptr;
    };

    std::set<String> emitted;

    // Registered migrations: emit one row each with their resolved state.
    // Applied takes priority over Failed (a re-applied migration is removed
    // from _failed on success).
    for (const Migration &m : _service->all())
    {
        if (!m.id)
            continue;

        JsonObject o = arr.add<JsonObject>();
        o["id"] = m.id;
        o["phase"] = phaseName(m.phase);
        o["order"] = m.order;
        o["onFailure"] = onFailureName(m.onFailure);
        o["maxAttempts"] = m.maxAttempts;

        const auto *a = findApplied(m.id);
        const auto *f = findFailed(m.id);
        if (a)
        {
            o["state"] = "applied";
            o["appliedAt"] = a->appliedAt;
        }
        else if (f)
        {
            o["state"] = "failed";
            o["attempts"] = f->attempts;
            o["lastError"] = f->lastError;
        }
        else
        {
            bool willRun = m.shouldRun ? m.shouldRun(fs) : true;
            o["state"] = willRun ? "pending" : "notApplicable";
        }
        emitted.insert(m.id);
    }

    // Orphans: entries persisted in _applied / _failed whose registration was
    // dropped by a later firmware. Surface them with registered=false so the
    // user can tell history apart from current registry.
    for (const auto &a : appliedList)
    {
        if (emitted.count(a.id))
            continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"] = a.id;
        o["state"] = "applied";
        o["appliedAt"] = a.appliedAt;
        o["registered"] = false;
        emitted.insert(a.id);
    }
    for (const auto &f : failedList)
    {
        if (emitted.count(f.id))
            continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"] = f.id;
        o["state"] = "failed";
        o["attempts"] = f.attempts;
        o["lastError"] = f.lastError;
        o["registered"] = false;
    }

    PsychicJsonResponse response(request, false);
    response.getRoot().set(doc.as<JsonObjectConst>());
    return response.send();
}

esp_err_t MigrationApi::_handleRetry(PsychicRequest *request)
{
    _service->clearFailures();

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Failure records cleared. Retries take effect on next reboot.";

    PsychicJsonResponse response(request, false);
    response.getRoot().set(doc.as<JsonObjectConst>());
    return response.send();
}
