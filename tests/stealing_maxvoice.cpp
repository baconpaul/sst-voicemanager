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

TEST_CASE("Stealing at Max Voice - Simplest Case")
{
    auto tp = TestPlayer<4>();
    auto &vm = tp.voiceManager;

    int16_t port{0}, channel{0}, key{50}, noteid{-1};
    float velocity{0.9}, retune{0.f};
    for (int i = 0; i < 10; ++i)
    {
        vm.processNoteOnEvent(port, channel, key + i, noteid, velocity, retune);
        tp.process();
        REQUIRE_VOICE_COUNTS(std::min(4, i + 1), std::min(4, i + 1));
        tp.dumpAllVoices();

        // Right now stealing is always oldest for physical stealing.
        if (i >= 3)
        {
            REQUIRE_VOICE_COUNTS(4, 4);
            std::set<int16_t> s;
            for (auto j = i - 3; j <= i; ++j)
            {
                s.insert(key + i);
            }
            REQUIRE(tp.hasKeysActive(s));
        }
    }
}

TEST_CASE("Stealing at Max Voice - multi-voice coordination")
{
    SECTION("9 at 3")
    {
        INFO(
            "This test creates 3-note clusters in a 9-voice player, which means we should steal in "
            "groups of 3");
        auto tp = TestPlayer<9>();
        auto &vm = tp.voiceManager;

        int16_t port{0}, channel{0}, key{90}, noteid{-1};
        float velocity{0.9}, retune{0.f};

        REQUIRE_NO_VOICES;
        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(3, 3);
        vm.processNoteOnEvent(port, channel, key + 1, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(6, 6);
        vm.processNoteOnEvent(port, channel, key + 2, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);

        INFO("Crucially this is not 10,10 since the terminate shoudl terminate the 'like' voices");
        vm.processNoteOnEvent(port, channel, key + 3, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);
    }

    SECTION("10 at 3")
    {
        INFO("This test creates 3-note clusters in a 10-voice player, which means we should steal "
             "in "
             "groups of 3");
        auto tp = TestPlayer<10>();
        auto &vm = tp.voiceManager;

        int16_t port{0}, channel{0}, key{90}, noteid{-1};
        float velocity{0.9}, retune{0.f};

        REQUIRE_NO_VOICES;
        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(3, 3);
        vm.processNoteOnEvent(port, channel, key + 1, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(6, 6);
        vm.processNoteOnEvent(port, channel, key + 2, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);

        INFO("Crucially this is not 10,10 since the terminate shoudl terminate the 'like' voices");
        vm.processNoteOnEvent(port, channel, key + 3, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);
    }

    SECTION("11 at 3")
    {
        INFO("This test creates 3-note clusters in a 11-voice player, which means we should steal "
             "in "
             "groups of 3");
        auto tp = TestPlayer<11>();
        auto &vm = tp.voiceManager;

        int16_t port{0}, channel{0}, key{90}, noteid{-1};
        float velocity{0.9}, retune{0.f};

        REQUIRE_NO_VOICES;
        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(3, 3);
        vm.processNoteOnEvent(port, channel, key + 1, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(6, 6);
        vm.processNoteOnEvent(port, channel, key + 2, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);

        INFO("Crucially this is not 10,10 since the terminate shoudl terminate the 'like' voices");
        vm.processNoteOnEvent(port, channel, key + 3, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(9, 9);
    }
}