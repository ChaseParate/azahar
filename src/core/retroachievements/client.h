// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Core {
class System;
}

namespace RetroAchievements {

using LoginCallback = std::function<void(bool success, const std::string& message)>;

struct AchievementEntry {
    std::string title;
    std::string description;
    std::string measured_progress; // e.g. "5/10" when partially complete
    float measured_percent;        // 0.0–100.0
    uint32_t id;
    uint32_t points;
    time_t unlock_time; // 0 if not unlocked
    uint8_t state;      // RC_CLIENT_ACHIEVEMENT_STATE_* values
    uint8_t bucket;     // RC_CLIENT_ACHIEVEMENT_BUCKET_* values
};

struct LeaderboardEntry {
    std::string title;
    std::string description;
    std::string tracker_value;
    uint32_t id;
    uint8_t state;
};

struct LeaderboardRankEntry {
    std::string user;
    std::string display; // formatted score string
    uint32_t rank;
    time_t submitted;
};

// Fired on the emulator thread when an achievement is unlocked.
using AchievementTriggeredCallback =
    std::function<void(const std::string& title, const std::string& description, uint32_t id)>;

class Client {
public:
    // system is stored as a raw pointer because Client is owned by System — no circular ownership.
    explicit Client(Core::System& system);
    ~Client();

    // Create rc_client and attempt auto-login from saved token.
    void Initialize();

    // Destroy rc_client. Safe to call even if Initialize was never called.
    void Shutdown();

    void LoginWithPassword(const std::string& username, const std::string& password,
                           LoginCallback callback);
    void LoginWithToken(const std::string& username, const std::string& token,
                        LoginCallback callback);
    void Logout();

    // Hash the ROM at filepath, query the RA server, and activate its achievement set.
    // No-op if not logged in.
    void LoadGame(const std::string& filepath);

    // Deactivate the current achievement set. Call when emulation stops.
    void UnloadGame();

    // Call once per emulated frame while a game is running.
    void DoFrame();

    // Apply the hardcore enabled/disabled state from Settings to rc_client.
    // rcheevos fires RC_CLIENT_EVENT_RESET if hardcore is turned on mid-game,
    // which we forward to the emulator as a soft reset.
    void ApplyHardcoreMode();

    // Returns the current rich presence string, or empty if unavailable.
    [[nodiscard]] std::string GetRichPresenceMessage() const;

    // Register a callback invoked (on the emulator thread) when an achievement unlocks.
    // The UI layer sets this to show a popup. Pass nullptr to clear.
    // Called when a leaderboard tracker should be shown/updated/hidden.
    // value is empty when the tracker should be hidden.
    using LeaderboardTrackerCallback = std::function<void(const std::string& value)>;
    using LeaderboardEntriesCallback =
        std::function<void(bool success, std::vector<LeaderboardRankEntry> entries)>;

    void SetAchievementTriggeredCallback(AchievementTriggeredCallback callback);
    void SetLeaderboardTrackerCallback(LeaderboardTrackerCallback callback);

    [[nodiscard]] std::vector<AchievementEntry> GetAchievements() const;
    void FetchLeaderboardEntries(uint32_t leaderboard_id, uint32_t count,
                                 LeaderboardEntriesCallback callback);
    [[nodiscard]] std::vector<LeaderboardEntry> GetLeaderboards() const;
    [[nodiscard]] bool IsHardcoreEnabled() const;
    [[nodiscard]] bool IsLoggedIn() const;
    [[nodiscard]] bool IsGameLoaded() const;
    [[nodiscard]] std::string GetUsername() const;
    [[nodiscard]] std::string GetDisplayName() const;
    [[nodiscard]] std::string GetLoginToken() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace RetroAchievements
