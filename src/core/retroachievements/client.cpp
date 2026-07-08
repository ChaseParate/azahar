// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/retroachievements/client.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"

#ifdef ENABLE_RETROACHIEVEMENTS
#include <cstring>
#include <thread>
#include <httplib.h>
#include <rc_client.h>
#include <rc_consoles.h>
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
    LeaderboardTrackerCallback on_leaderboard_tracker;

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
    // rcheevos passes a flat RA address starting at 0. For 3DS, the convention
    // used by achievement authors is FCRAM-relative: RA address 0 maps to the
    // first byte of FCRAM (physical 0x20000000), which in virtual memory is the
    // start of the linear heap (0x14000000). We use GetFCRAMPointer to read
    // directly from FCRAM without any virtual-address translation, which is both
    // correct and avoids faulting on unmapped virtual pages.
    static uint32_t ReadMemory(uint32_t address, uint8_t* buffer, uint32_t num_bytes,
                               rc_client_t* client) {
        auto* self = static_cast<Impl*>(rc_client_get_userdata(client));
        if (!self) return 0;

        const std::size_t fcram_size = Memory::FCRAM_N3DS_SIZE; // 256 MB covers both old+new 3DS
        if (static_cast<std::size_t>(address) + num_bytes > fcram_size) return 0;

        const u8* src = self->system.Memory().GetFCRAMPointer(address);
        if (!src) return 0;

        std::memcpy(buffer, src, num_bytes);
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
            if (self && self->on_leaderboard_tracker) {
                self->on_leaderboard_tracker(""); // hide tracker
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            LOG_INFO(RetroAchievements, "Leaderboard score submitted: {} — {}",
                     event->leaderboard ? event->leaderboard->title : "unknown",
                     event->leaderboard ? event->leaderboard->tracker_value : "");
            if (self && self->on_leaderboard_tracker) {
                self->on_leaderboard_tracker(""); // hide tracker on submit
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
            LOG_INFO(RetroAchievements, "Leaderboard scoreboard updated: {}",
                     event->leaderboard ? event->leaderboard->title : "unknown");
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
            if (self && self->on_leaderboard_tracker && event->leaderboard_tracker) {
                self->on_leaderboard_tracker(event->leaderboard_tracker->display);
            }
            break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
            if (self && self->on_leaderboard_tracker) {
                self->on_leaderboard_tracker("");
            }
            break;

        case RC_CLIENT_EVENT_GAME_COMPLETED: {
            LOG_INFO(RetroAchievements, "Game completed — all achievements earned!");
            // Fire achievement callback with a synthetic mastery notification so the UI
            // layer can show an overlay without needing a separate callback type.
            if (self && self->on_achievement_triggered) {
                const rc_client_game_t* game = rc_client_get_game_info(client);
                const std::string title_str =
                    game && game->title ? std::string("Mastered: ") + game->title : "Game Mastered!";
                self->on_achievement_triggered(title_str, "All achievements earned!", 0);
            }
            break;
        }

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
                LOG_INFO(RetroAchievements, "Game identified: '{}' (ID {}, hash: {})",
                         game->title ? game->title : "unknown", game->id,
                         game->hash ? game->hash : "unknown");
            }

            // Log achievement set summary so we know what was downloaded.
            rc_client_achievement_list_t* list = rc_client_create_achievement_list(
                client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
            if (list) {
                uint32_t total = 0;
                for (uint32_t b = 0; b < list->num_buckets; ++b)
                    total += list->buckets[b].num_achievements;
                LOG_INFO(RetroAchievements, "Achievement set loaded: {} achievements", total);
                rc_client_destroy_achievement_list(list);
            }

            LOG_INFO(RetroAchievements,
                     "[POC] ReadMemory uses FCRAM-relative addressing. "
                     "RA address 0x0 = FCRAM byte 0 = virtual 0x14000000 (linear heap). "
                     "Check the log filter 'RetroAchievements' while playing to see events.");
        } else if (result == RC_NO_GAME_LOADED) {
            LOG_INFO(RetroAchievements,
                     "Game not found in RetroAchievements database — "
                     "ROM may not be supported or hash did not match.");
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

void Client::SetLeaderboardTrackerCallback(LeaderboardTrackerCallback callback) {
    impl->on_leaderboard_tracker = std::move(callback);
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

void Client::FetchLeaderboardEntries(uint32_t leaderboard_id, uint32_t count,
                                      LeaderboardEntriesCallback callback) {
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client || !IsGameLoaded()) {
        if (callback) callback(false, {});
        return;
    }

    struct FetchCtx {
        LeaderboardEntriesCallback cb;
    };
    auto* ctx = new FetchCtx{std::move(callback)};

    rc_client_begin_fetch_leaderboard_entries(
        impl->rc_client, leaderboard_id, 1, count,
        [](int result, const char* /*error_message*/,
           rc_client_leaderboard_entry_list_t* list,
           rc_client_t* /*client*/, void* userdata) {
            auto* ctx = static_cast<FetchCtx*>(userdata);
            std::vector<LeaderboardRankEntry> entries;
            if (result == RC_OK && list) {
                entries.reserve(list->num_entries);
                for (uint32_t i = 0; i < list->num_entries; ++i) {
                    const auto& e = list->entries[i];
                    entries.push_back({
                        e.user ? e.user : "",
                        std::string(e.display),
                        e.rank,
                        e.submitted,
                    });
                }
                rc_client_destroy_leaderboard_entry_list(list);
            }
            if (ctx->cb) ctx->cb(result == RC_OK, std::move(entries));
            delete ctx;
        },
        ctx);
#else
    if (callback) callback(false, {});
#endif
}

std::vector<AchievementEntry> Client::GetAchievements() const {
    std::vector<AchievementEntry> result;
#ifdef ENABLE_RETROACHIEVEMENTS
    if (!impl->rc_client || !IsGameLoaded()) return result;

    rc_client_achievement_list_t* list = rc_client_create_achievement_list(
        impl->rc_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list) return result;

    for (uint32_t b = 0; b < list->num_buckets; ++b) {
        const rc_client_achievement_bucket_t& bucket = list->buckets[b];
        for (uint32_t i = 0; i < bucket.num_achievements; ++i) {
            const rc_client_achievement_t* a = bucket.achievements[i];
            if (!a) continue;
            result.push_back({
                a->title       ? a->title       : "",
                a->description ? a->description : "",
                std::string(a->measured_progress),
                a->measured_percent,
                a->id,
                a->points,
                a->unlock_time,
                a->state,
                a->bucket,
            });
        }
    }

    rc_client_destroy_achievement_list(list);
#endif
    return result;
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
