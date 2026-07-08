// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <string>

namespace Core {
class System;
}

namespace RetroAchievements {

using LoginCallback = std::function<void(bool success, const std::string& message)>;

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
    void SetAchievementTriggeredCallback(AchievementTriggeredCallback callback);

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
