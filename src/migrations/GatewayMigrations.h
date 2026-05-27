/**
 * @file GatewayMigrations.h
 * @brief Registers all Genius Gateway config migrations with MigrationService.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#pragma once

class MigrationService;

/// Register every gateway-side migration with the supplied service. Call
/// once at startup, before MigrationService::runPhase(). Migration IDs are
/// stable — never rename, only add.
void registerGatewayMigrations(MigrationService &service);
