/**
 * @file MigrationApi.h
 * @brief REST surface for inspecting and retrying config migrations.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#pragma once

#include <Arduino.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>

class MigrationService;

#define MIGRATIONS_PATH "/rest/migrations"
#define MIGRATIONS_RETRY_PATH "/rest/migrations/retry"

/// REST endpoint pair for the migration service.
/// - GET  /rest/migrations         : authenticated users — applied/pending/failed
/// - POST /rest/migrations/retry   : admin — clear failure records
class MigrationApi
{
public:
    MigrationApi(PsychicHttpServer *server,
                 SecurityManager *securityManager,
                 MigrationService *service);

    void begin();

private:
    static constexpr const char *TAG = "MigrationsApi";

    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    MigrationService *_service;

    esp_err_t _handleList(PsychicRequest *request);
    esp_err_t _handleRetry(PsychicRequest *request);
};
