// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/retroachievements/mock_server.h"

#ifdef ENABLE_RETROACHIEVEMENTS

#include <string>
#include <string_view>

namespace RetroAchievements {

// ── JSON responses ────────────────────────────────────────────────────────────

static constexpr std::string_view kLoginOk =
    R"({"Success":true,"User":"MockUser","Token":"MOCKTOKEN123","Score":100,)"
    R"("SoftcoreScore":50,"Messages":0,"Permissions":1,"AccountType":"Registered"})";

// r=gameid — rcheevos sends the ROM hash here to look up the game.
// We return a fixed game ID so it always "finds" a game.
static constexpr std::string_view kGameId =
    R"({"Success":true,"GameID":999})";

// r=patch — full game data including achievements and leaderboards.
//
// Achievement MemAddr conditions (FCRAM-relative byte reads):
//   0xH0000=0.60.  — triggers after FCRAM byte[0] has equalled 0 for 60 frames (~1 sec)
//   0xH0000=0.300. — triggers after 300 such frames (~5 sec)
// If byte[0] is not 0 in the game you load, nothing fires — that itself tells you something
// about the FCRAM layout. Edit the condition to match a known-zero address.
//
// Leaderboard Mem: starts tracking once byte[0]==0 for 30 frames, never cancels, never
// auto-submits (so you can close the game to end it). Value reads a 32-bit word at byte[0].
static constexpr std::string_view kPatch = R"({
  "Success": true,
  "PatchData": {
    "ID": 999,
    "Title": "Azahar Mock Test",
    "ConsoleID": 18,
    "ImageIcon": "/Images/000001.png",
    "RichPresencePatch": "Display:\r\nMock mode active - testing rcheevos pipeline",
    "Achievements": [
      {
        "ID": 1001,
        "Title": "Hello World",
        "Description": "rcheevos read FCRAM byte[0]==0 for 60 consecutive frames",
        "Points": 5,
        "MemAddr": "0xH0000=0.60.",
        "Author": "MockDev",
        "Modified": 1700000000,
        "Created": 1700000000,
        "BadgeName": "00000",
        "Flags": 3,
        "Type": null
      },
      {
        "ID": 1002,
        "Title": "Still Alive",
        "Description": "rcheevos has been reading FCRAM successfully for ~5 seconds",
        "Points": 10,
        "MemAddr": "0xH0000=0.300.",
        "Author": "MockDev",
        "Modified": 1700000000,
        "Created": 1700000000,
        "BadgeName": "00000",
        "Flags": 3,
        "Type": null
      }
    ],
    "Leaderboards": [
      {
        "ID": 2001,
        "Title": "FCRAM Word at 0",
        "Description": "Value of the 32-bit word at FCRAM offset 0",
        "Mem": "STA:0xH0000=0.30.::CAN:0=1::SUB:0=1::VAL:0xX0000",
        "Format": "VALUE",
        "LowerIsBetter": 0,
        "Hidden": 0
      }
    ]
  }
})";

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
    if (r_val == "gameid")                           return kGameId;
    if (r_val == "patch")                            return kPatch;
    if (r_val == "startsession")                     return kStartSession;
    if (r_val == "awardachievement")                 return kAwardAchievement;
    if (r_val == "ping")                             return kPing;
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
