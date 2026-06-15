/*
 * sst-voicemanager - a header only library providing synth
 * voice management in response to midi and clap event streams
 * with support for a variety of play, trigger, and midi nodes
 *
 * Copyright 2023-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-voicemanager is released under the MIT license, available
 * as LICENSE.md in the root of this repository.
 *
 * All source in sst-voicemanager available at
 * https://github.com/surge-synthesizer/sst-voicemanager
 */

#include "catch2.hpp"
#include "sst/voicemanager/voicemanager.h"
#include "test_player.h"

#if SST_VOICEMANAGER_RTSAN_ACTIVE
// Keep running after the first rtsan violation so we see every allocation, not just the first.
extern "C" const char *__rtsan_default_options() { return "halt_on_error=false"; }
extern "C" void __sanitizer_report_error_summary(const char *error_summary)
{
    fprintf(stderr, "%s\n", error_summary);
    /* do other custom things */
    sst::voicemanager::test::RealtimeRegionGuard::errorCount++;
}
int sst::voicemanager::test::RealtimeRegionGuard::errorCount = 0;
#endif

TEST_CASE("Tests Configured") { REQUIRE(1 + 1 == 2); }
TEST_CASE("Can Instantiate Test Player")
{
    TestPlayer<32> tp;
    REQUIRE(tp.voiceManager.getVoiceCount() == 0);
}