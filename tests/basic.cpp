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

TEST_CASE("Tests Configured") { REQUIRE(1 + 1 == 2); }
TEST_CASE("Can Instantiate Test Player")
{
    TestPlayer<32> tp;
    REQUIRE(tp.voiceManager.getVoiceCount() == 0);
}