/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2025 theelims
 *   Copyright (C) 2025 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <GitHubReleaseService.h>

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_certs_x509_crt_bundle_bin_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_certs_x509_crt_bundle_bin_end");

GitHubReleaseInfo GitHubReleaseService::queryLatestRelease(
    const String &repoOwner,
    const String &repoName,
    const String &userAgent,
    bool useInsecure)
{
    GitHubReleaseInfo result;
      
    // Build API URL
    char url[256];
    snprintf(url, sizeof(url), GITHUB_API_RELEASES_LATEST_URL, repoOwner.c_str(), repoName.c_str());
    
    WiFiClientSecure client;
    
    // Configure SSL certificate verification
    if (useInsecure)
    {
        ESP_LOGW(TAG, "Skipping SSL certificate verification for GitHub API");
        client.setInsecure();
    }
    else
    {
#if ESP_ARDUINO_VERSION_MAJOR == 3
        client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
#else
        client.setCACertBundle(rootca_crt_bundle_start);
#endif
    }
    
    client.setTimeout(12000);
    
    HTTPClient http;
    if (!userAgent.isEmpty())
    {
        http.setUserAgent(userAgent);
    }
    
    if (!http.begin(client, url))
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP connection to GitHub API");
        return result;
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK)
    {
        ESP_LOGE(TAG, "GitHub API request failed with code: %d", httpCode);
        http.end();
        return result;
    }
    
    String payload = http.getString();
    
    http.end();
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error)
    {
        ESP_LOGE(TAG, "Failed to parse GitHub API response: %s", error.c_str());
        return result;
    }
    
    // Extract version information
    String tagName = doc["tag_name"] | "";
    if (tagName.isEmpty())
    {
        ESP_LOGW(TAG, "No tag_name found in GitHub release");
        return result;
    }
    
    // Remove 'v' prefix if present (e.g., "v1.2.3" -> "1.2.3")
    if (tagName.startsWith("v") || tagName.startsWith("V"))
    {
        tagName = tagName.substring(1);
    }
    
    result.version = tagName;
    result.name = doc["name"] | "";
    
    // Find download URL for the binary (look for .bin file in assets)
    JsonArray assets = doc["assets"];
    
    for (JsonVariant asset : assets)
    {
        String assetName = asset["name"] | "";
        if (assetName.endsWith(".bin"))
        {
            result.downloadUrl = asset["browser_download_url"] | "";
            break;
        }
    }
    
    if (result.downloadUrl.isEmpty())
    {
        ESP_LOGW(TAG, "No .bin download URL found in GitHub release");
    }
    
    result.valid = true;
    
    ESP_LOGI(TAG, "GitHub query successful - Version: %s, Download URL: %s",
             result.version.c_str(), result.downloadUrl.isEmpty() ? "(none)" : result.downloadUrl.c_str());
    
    return result;
}

bool GitHubReleaseService::queryAllReleases(
    const String &repoOwner,
    const String &repoName,
    const String &userAgent,
    bool useInsecure,
    String &jsonResponse)
{
    // Build API URL for all releases
    char url[256];
    snprintf(url, sizeof(url), GITHUB_API_RELEASES_ALL_URL, repoOwner.c_str(), repoName.c_str());
    
    WiFiClientSecure client;
    
    // Configure SSL certificate verification
    if (useInsecure)
    {
        ESP_LOGW(TAG, "Skipping SSL certificate verification for GitHub API");
        client.setInsecure();
    }
    else
    {
#if ESP_ARDUINO_VERSION_MAJOR == 3
        client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
#else
        client.setCACertBundle(rootca_crt_bundle_start);
#endif
    }
    
    client.setTimeout(12000);
    
    HTTPClient http;
    if (!userAgent.isEmpty())
    {
        http.setUserAgent(userAgent);
    }
    
    if (!http.begin(client, url))
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP connection to GitHub API (all releases)");
        return false;
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK)
    {
        ESP_LOGE(TAG, "GitHub API request failed with code: %d", httpCode);
        http.end();
        return false;
    }
    
    jsonResponse = http.getString();
    http.end();
    
    ESP_LOGI(TAG, "GitHub query successful - Retrieved all releases (response size: %d bytes)", jsonResponse.length());
    
    return true;
}

bool GitHubReleaseService::isNewerVersion(const String &current, const String &latest)
{
    // Simple semantic version comparison (major.minor.patch)
    int curMajor = 0, curMinor = 0, curPatch = 0;
    int latMajor = 0, latMinor = 0, latPatch = 0;
    
    sscanf(current.c_str(), "%d.%d.%d", &curMajor, &curMinor, &curPatch);
    sscanf(latest.c_str(), "%d.%d.%d", &latMajor, &latMinor, &latPatch);
    
    // Compare versions
    if (latMajor > curMajor) return true;
    if (latMajor < curMajor) return false;
    
    if (latMinor > curMinor) return true;
    if (latMinor < curMinor) return false;
    
    if (latPatch > curPatch) return true;
    
    return false;
}
