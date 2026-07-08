// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/retroachievements/client.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"

#ifdef ENABLE_RETROACHIEVEMENTS
#include <thread>
#include <httplib.h>
#include <rc_client.h>
#include <rc_consoles.h>
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"
#endif

namespace RetroAchievements {

// ─────────────────────────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────────────────────────

struct Client::Impl {
    Core::System& system;

    explicit Impl(Core::System& system_) : system(system_) {}

    AchievementTriggeredCallback on_achievement_triggered;

#ifdef ENABLE_RETROACHIEVEMENTS
    rc_client_t* rc_client = nullptr;

    // ── HTTP backend ─────────────────────────────────────────────────────────
    static void ServerCall(const rc_api_request_t* request,
                           rc_client_server_callback_t callback, void* callback_userdata,
                           rc_client_t* /*client*/) {
        std::string url(request->url ? request->url : "");
        static constexpr std::string_view kBase = "https://retroachievements.org";
        std::string path = url.size() > kBase.size() ? url.substr(kBase.size()) : "/";
        std::string post_data = request->post_data ? request->post_data : "";
        const bool is_post = !post_data.empty();

        std::thread([callback, callback_userdata, path, post_data, is_post]() {
            httplib::SSLClient cli("retroachievements.org");
            cli.set_connection_timeout(10, 0);
            cli.set_read_timeout(30, 0);

            httplib::Result res;
            if (is_post) {
                res = cli.Post(path, post_data, "application/x-www-form-urlencoded");
            } else {
                res = cli.Get(path);
            }

            rc_api_server_response_t server_response{};
            std::string body;
            if (res) {
                body = res->body;
                server_response.body = body.c_str();
                server_response.body_length = body.size();
                server_response.http_status_code = static_cast<uint32_t>(res->status);
            } else {
                LOG_WARNING(RetroAchievements, "HTTP request to '{}' failed: {}", path,
                            httplib::to_string(res.error()));
                server_response.http_status_code = 0;
            }
            callback(&server_response, callback_userdata);
        }).detach();
    }

    // ── Memory read ──────────────────────────────────────────────────────────
    // rcheevos passes a flat RA address. For 3DS, achievement authors target the
    // application heap which starts at virtual address 0x08000000. We add that
    // base so that RA address 0x0 maps to 3DS VAddr 0x08000000.
    static uint32_t ReadMemory(uint32_t address, uint8_t* buffer, uint32_t num_bytes,
                               rc_client_t* client) {
        auto* self = static_cast<Impl*>(rc_client_get_userdata(client));
        if (!self) return 0;

        auto process = self->system.Kernel().GetCurrentProcess();
        if (!process) return 0;

        static constexpr u32 kHeapBase = Memory::HEAP_VADDR; // 0x08000000
        const VAddr vaddr = static_cast<VAddr>(address) + kHeapBase;

        // ReadBlock returns void; treat any exception as a failed read.
        try {
            self->system.Memory().ReadBlock(*process, vaddr, buffer, num_bytes);
        } catch (...) {
            return 0;
        }
        return num_bytes;
    }

    // ── Event handler ────────────────────────────────────────────────────────
    static void OnEvent(const rc_client_event_t* event, rc_client_t* client) {
        auto* self = static_cast<Impl*>(rc_client_get_userdata(client));

        switch (event->type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            LOG_INFO(RetroAchievements, "Achievement unlocked: {} ({})",
                     event->achievement->title, event->achievement->id);
            if (self && self->on_achievement_triggered) {
                self->on_achievement_triggered(
                    event->achievement->title       ? event->achievement->title       : "",
                    event->achievement->description ? event->achievement->description : "",
                    event->achievement->id);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
            LOG_INFO(RetroAchievements, "Leaderboard attempt started: {}",
                     event->leaderboard ? event->leaderboard->title : "unknown");
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
            LOG_INFO(RetroAchievements, "Leaderboard attempt failed: {}",
                     event->leaderboard ? event->leaderboard->title : "unknown");
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            LOG_INFO(RetroAchievements, "Leaderboard score submitted: {} — {}",
                     event->leaderboard ? event->leaderboard->title : "unknown",
                     event->leaderboard ? event->leaderboard->tracker_value : "");
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
            LOG_INFO(RetroAchievements, "Leaderboard scoreboard updated: {}",
                     event->leaderboard ? event->leaderboard->title : "unknown");
            break;

        case RC_CLIENT_EVENT_GAME_COMPLETED:
            LOG_INFO(RetroAchievements, "Game completed — all achievements earned!");
            break;

        case RC_CLIENT_EVENT_RESET:
            // rcheevos requests a reset when hardcore mode is enabled mid-game
            // to prevent save state abuse. We honour it with a soft reset.
            LOG_INFO(RetroAchievements, "Hardcore mode enabled — resetting emulator");
            if (self) {
                self->system.RequestReset();
            }
            break;

        case RC_CLIENT_EVENT_SERVER_ERROR:
            LOG_WARNING(RetroAchievements, "Server error: {}",
                        event->server_error ? event->server_error->error_message : "unknown");
            break;

        case RC_CLIENT_EVENT_DISCONNECTED:
            LOG_WARNING(RetroAchievements, "Disconnected — pending unlocks queued");
            break;

        case RC_CLIENT_EVENT_RECONNECTED:
            LOG_INFO(RetroAchievements, "Reconnected — pending unlocks submitted");
            break;

        default:
            break;
        }
    }

    // ── Login callback ───────────────────────────────────────────────────────
    struct LoginCallbackContext {
        LoginCallback user_callback;
    };

    static void OnLoginComplete(int result, const char* error_message,
                                rc_client_t* client, void* userdata) {
        auto* ctx = static_cast<LoginCallbackContext*>(userdata);
        LoginCallback cb = std::move(ctx->user_callback);
        delete ctx;

        if (result == RC_OK) {
            const rc_client_user_t* user = rc_client_get_user_info(client);
            if (user) {
                Settings::values.ra_username = user->username ? user->username : "";
                Settings::values.ra_token    = user->token    ? user->token    : "";
            }
            LOG_INFO(RetroAchievements, "Logged in as '{}'",
                     user && user->display_name ? user->display_name : "unknown");
            if (cb) cb(true, "");
        } else {
            const std::string msg = error_message ? error_message : "Unknown error";
            LOG_WARNING(RetroAchievements, "Login failed: {}", msg);
            if (cb) cb(false, msg);
        }
    }

    // ── Game identification callback ─────────────────────────────────────────
    static void OnGameIdentified(int result, const char* error_message,
                                 rc_client_t* client, void* /*userdata*/) {
        if (result == RC_OK) {
            const rc_client_game_t* game = rc_client_get_game_info(client);
            if (game) {
                LOG_INFO(RetroAchievements, "Game identified: '{}' (ID {})",
                         game->title ? game->title : "unknown", game->id);
            }
        } else if (result == RC_NO_GAME_LOADED) {
            LOG_INFO(RetroAchievements, "Game not found in RetroAchievements database");
        } else {
            LOG_WARNING(RetroAchievements, "Game identification failed: {}",
                        error_message ? error_message : "unknown error");
        }
    }
#endif // ENABLE_RETROACHIEVEMENTS
};

// ─────────────────────────────────────────────────────────────────────────────
// Client
// ─────────────────────────────────────────────────────────────────────────────

Client::Client(Core::System& system) : impl(std::make_unique<Impl>(system)) {}

Client::~Client() {
    Shutdown();
}

void Client::Initialize() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (impl->rc_client) {
        return; // already initialized
    }
    impl->rc_client = rc_client_create(Impl::ReadMemory, Impl::ServerCall);
    if (!impl->rc_client) {
        LOG_ERROR(RetroAchievements, "Failed to create rc_client");
        return;
    }

    rc_client_set_userdata(impl->rc_client, impl.get());
    rc_client_set_event_handler(impl->rc_client, Impl::OnEvent);

    LOG_INFO(RetroAchievements, "rc_client initialized");

    const std::string& username = Settings::values.ra_username;
    const std::string& token    = Settings::values.ra_token;
    if (!username.empty() && !token.empty()) {
        LOG_INFO(RetroAchievements, "Auto-logging in as '{}'", username);
        LoginWithToken(username, token, nullptr);
    }
#else
    LOG_INFO(RetroAchievements, "RetroAchievements support not compiled in");
#endif
}

void Client::Shutdown() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (impl->rc_client) {
        rc_client_destroy(impl->rc_client);
        impl->rc_client = nullptr;
        LOG_INFO(RetroAchievements, "rc_client destroyed");
    }
#endif
}

void Client::LoginWithPassword(const std::string& username, const std::string& password,
                               LoginCallback callback) {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) {
        if (callback) callback(false, "RetroAchievements client not initialized");
        return;
    }
    auto* ctx = new Impl::LoginCallbackContext{std::move(callback)};
    rc_client_begin_login_with_password(impl->rc_client, username.c_str(), password.c_str(),
                                        Impl::OnLoginComplete, ctx);
#else
    if (callback) callback(false, "RetroAchievements support not compiled in");
#endif
}

void Client::LoginWithToken(const std::string& username, const std::string& token,
                             LoginCallback callback) {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) {
        if (callback) callback(false, "RetroAchievements client not initialized");
        return;
    }
    auto* ctx = new Impl::LoginCallbackContext{std::move(callback)};
    rc_client_begin_login_with_token(impl->rc_client, username.c_str(), token.c_str(),
                                     Impl::OnLoginComplete, ctx);
#else
    if (callback) callback(false, "RetroAchievements support not compiled in");
#endif
}

void Client::Logout() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (impl->rc_client) {
        rc_client_logout(impl->rc_client);
    }
#endif
    Settings::values.ra_username.clear();
    Settings::values.ra_token.clear();
    LOG_INFO(RetroAchievements, "Logged out");
}

void Client::LoadGame(const std::string& filepath) {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) {
        LOG_WARNING(RetroAchievements, "LoadGame called before Initialize");
        return;
    }
    if (!IsLoggedIn()) {
        LOG_DEBUG(RetroAchievements, "Not logged in — skipping game identification");
        return;
    }
    ApplyHardcoreMode();
    LOG_INFO(RetroAchievements, "Identifying game: {}", filepath);
    rc_client_begin_identify_and_load_game(impl->rc_client, RC_CONSOLE_NINTENDO_3DS,
                                           filepath.c_str(), nullptr, 0,
                                           Impl::OnGameIdentified, nullptr);
#endif
}

void Client::UnloadGame() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (impl->rc_client) {
        rc_client_unload_game(impl->rc_client);
        LOG_INFO(RetroAchievements, "Game unloaded");
    }
#endif
}

void Client::DoFrame() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (impl->rc_client && IsGameLoaded()) {
        rc_client_do_frame(impl->rc_client);
    }
#endif
}

void Client::ApplyHardcoreMode() {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return;
    const bool hardcore = Settings::values.ra_hardcore_mode.GetValue();
    rc_client_set_hardcore_enabled(impl->rc_client, hardcore ? 1 : 0);
    LOG_INFO(RetroAchievements, "Hardcore mode: {}", hardcore ? "enabled" : "disabled");
#endif
}

std::string Client::GetRichPresenceMessage() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client || !IsGameLoaded()) return {};
    char buffer[256]{};
    rc_client_get_rich_presence_message(impl->rc_client, buffer, sizeof(buffer));
    return buffer;
#else
    return {};
#endif
}

bool Client::IsHardcoreEnabled() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return false;
    return rc_client_get_hardcore_enabled(impl->rc_client) != 0;
#else
    return false;
#endif
}

void Client::SetAchievementTriggeredCallback(AchievementTriggeredCallback callback) {
    impl->on_achievement_triggered = std::move(callback);
}

bool Client::IsLoggedIn() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return false;
    return rc_client_get_user_info(impl->rc_client) != nullptr;
#else
    return false;
#endif
}

bool Client::IsGameLoaded() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return false;
    return rc_client_is_game_loaded(impl->rc_client) != 0;
#else
    return false;
#endif
}

std::string Client::GetUsername() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return {};
    const rc_client_user_t* user = rc_client_get_user_info(impl->rc_client);
    return user && user->username ? user->username : std::string{};
#else
    return {};
#endif
}

std::string Client::GetDisplayName() const {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client) return {};
    const rc_client_user_t* user = rc_client_get_user_info(impl->rc_client);
    return user && user->display_name ? user->display_name : std::string{};
#else
    return {};
#endif
}

std::string Client::GetLoginToken() const {
    return Settings::values.ra_token;
}

std::vector<LeaderboardEntry> Client::GetLeaderboards() const {
    std::vector<LeaderboardEntry> result;
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client || !IsGameLoaded()) return result;

    rc_client_leaderboard_list_t* list =
        rc_client_create_leaderboard_list(impl->rc_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
    if (!list) return result;

    for (uint32_t b = 0; b < list->num_buckets; ++b) {
        const rc_client_leaderboard_bucket_t& bucket = list->buckets[b];
        for (uint32_t i = 0; i < bucket.num_leaderboards; ++i) {
            const rc_client_leaderboard_t* lb = bucket.leaderboards[i];
            if (!lb) continue;
            result.push_back({
                lb->title         ? lb->title         : "",
                lb->description   ? lb->description   : "",
                lb->tracker_value ? lb->tracker_value : "",
                lb->id,
                lb->state,
            });
        }
    }

    rc_client_destroy_leaderboard_list(list);
#endif
    return result;
}

} // namespace RetroAchievements
