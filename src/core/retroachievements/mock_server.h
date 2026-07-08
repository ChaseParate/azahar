// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#ifdef ENABLE_RETROACHIEVEMENTS
#include <rc_client.h>

namespace RetroAchievements {

// Drop-in replacement for the real HTTP ServerCall. Returns canned JSON responses
// immediately (no network I/O) so the full rcheevos pipeline can be exercised
// without a real RA account or a registered game.
//
// Recognized endpoints (r= param):
//   login2 / tokenlogin  — fake login success
//   gameid               — returns mock game ID 999
//   patch                — returns 2 test achievements + 1 leaderboard
//   startsession         — success
//   awardachievement     — success
//   ping                 — success
//   everything else      — {"Success":false,"Error":"Mock: endpoint not implemented"}
void MockServerCall(const rc_api_request_t* request,
                    rc_client_server_callback_t callback,
                    void* callback_userdata,
                    rc_client_t* client);

} // namespace RetroAchievements
#endif // ENABLE_RETROACHIEVEMENTS
