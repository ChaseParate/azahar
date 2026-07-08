// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/retroachievements/mock_server.h"

#ifdef ENABLE_RETROACHIEVEMENTS

#include <string>
#include <string_view>
#include "common/logging/log.h"

namespace RetroAchievements {

// ── JSON responses ────────────────────────────────────────────────────────────

static constexpr std::string_view kLoginOk =
    R"({"Success":true,"User":"MockUser","Token":"MOCKTOKEN123","Score":100,)"
    R"("SoftcoreScore":50,"Messages":0,"Permissions":1,"AccountType":"Registered"})";


// r=achievementsets — modern single-call endpoint that replaces the old gameid+patch two-step.
// Field names differ from the old patch format: GameId/ConsoleId/ImageIconUrl, Sets[] wrapper.
// Format confirmed from rcheevos/test/test_rc_client.c patchdata_2ach_1lbd.
//
// Achievement MemAddr conditions (FCRAM-relative byte reads, mock memory = all zeros):
//   0xH0000=0.60.  — triggers after ~1 second at 60fps
//   0xH0000=0.300. — triggers after ~5 seconds; being the last achievement it also fires mastery
static constexpr std::string_view kAchievementSets = R"({"Success":true,)"
    R"("GameId":999,"Title":"Azahar Mock Test","ConsoleId":18,)"
    R"("ImageIconUrl":"http://retroachievements.org/Images/000001.png",)"
    R"("RichPresenceGameId":999,"RichPresencePatch":"Display:\r\nMock mode active",)"
    R"("Sets":[{)"
      R"("AchievementSetId":1,"GameId":999,"Title":null,"Type":"core",)"
      R"("ImageIconUrl":"http://retroachievements.org/Images/000001.png",)"
      R"("Achievements":[)"
        R"({"ID":1001,"Title":"Hello World","Description":"rcheevos read FCRAM successfully",)"
         R"("Points":5,"MemAddr":"0xH0000=0.60.","Author":"MockDev","BadgeName":"00000",)"
         R"("Created":1700000000,"Modified":1700000000,"Flags":3},)"
        R"({"ID":1002,"Title":"Still Alive","Description":"Memory reads working for ~5 seconds",)"
         R"("Points":10,"MemAddr":"0xH0000=0.300.","Author":"MockDev","BadgeName":"00000",)"
         R"("Created":1700000000,"Modified":1700000000,"Flags":3})"
      R"(],)"
      R"("Leaderboards":[)"
        R"({"ID":2001,"Title":"FCRAM Word at 0","Description":"32-bit value at FCRAM offset 0",)"
         R"("Mem":"STA:0xH0000=0.30.::CAN:0=1::SUB:0=1::VAL:0xX0000","Format":"VALUE"})"
      R"(])"
    R"(}]})";

static constexpr std::string_view kStartSession =
    R"({"Success":true,"ServerNow":1700000000})";

static constexpr std::string_view kAwardAchievement =
    R"({"Success":true,"Score":115,"SoftcoreScore":65,"AchievementID":0,"AchievementsRemaining":0})";

static constexpr std::string_view kPing =
    R"({"Success":true})";

static constexpr std::string_view kUnknown =
    R"({"Success":false,"Error":"Mock: endpoint not implemented"})";

// ── Endpoint routing ──────────────────────────────────────────────────────────

static std::string_view RouteRequest(const rc_api_request_t* request) {
    // The `r=` parameter appears in either the URL query string or the POST body.
    // Search both; POST body takes priority since some endpoints put params there.
    auto find_r = [](std::string_view s) -> std::string_view {
        const std::string_view needle = "r=";
        auto pos = s.find(needle);
        if (pos == std::string_view::npos) return {};
        pos += needle.size();
        auto end = s.find('&', pos);
        return s.substr(pos, end == std::string_view::npos ? end : end - pos);
    };

    std::string_view r_val;
    if (request->post_data && *request->post_data) {
        r_val = find_r(request->post_data);
    }
    if (r_val.empty() && request->url && *request->url) {
        r_val = find_r(request->url);
    }

    if (r_val == "login2" || r_val == "tokenlogin") return kLoginOk;
    if (r_val == "achievementsets")                  return kAchievementSets;
    if (r_val == "startsession")                     return kStartSession;
    if (r_val == "awardachievement")                 return kAwardAchievement;
    if (r_val == "ping")                             return kPing;

    LOG_WARNING(RetroAchievements, "[Mock] Unhandled endpoint — r='{}' | url='{}' | post='{}'",
                r_val, request->url ? request->url : "", request->post_data ? request->post_data : "");
    return kUnknown;
}

// ── Public entry point ────────────────────────────────────────────────────────

void MockServerCall(const rc_api_request_t* request,
                    rc_client_server_callback_t callback,
                    void* callback_userdata,
                    rc_client_t* /*client*/) {
    const std::string_view body = RouteRequest(request);

    rc_api_server_response_t response{};
    response.body = body.data();
    response.body_length = static_cast<uint32_t>(body.size());
    response.http_status_code = 200;

    callback(&response, callback_userdata);
}

} // namespace RetroAchievements
#endif // ENABLE_RETROACHIEVEMENTS
