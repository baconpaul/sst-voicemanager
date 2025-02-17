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

TEST_CASE("Note ID in Poly Mode")
{
    SECTION("No Overlapping PCK")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 62, 179, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 179; }) == 1);

        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 179; }) == 1);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(2, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 179; }) == 1);
        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 179; }) == 1);
        vm.processNoteOffEvent(0, 1, 62, 179, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 179; }) == 1);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Incorrect off note id doesn't end")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);

        tp.processFor(10);

        INFO("Bad NoteID doesn't turn off the note");
        vm.processNoteOffEvent(0, 1, 60, 188242, 0.8);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 173; }) == 1);
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 173; }) == 1);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Overlapping PCK (voice stacking)")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 179, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 184, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        REQUIRE_VOICE_MATCH_FN(3, [](const vc_t &v) { return v.key() == 60; });

        tp.processFor(20);
        vm.processNoteOffEvent(0, 1, 60, 179, 0.8);
        REQUIRE_VOICE_COUNTS(3, 2);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 173 && v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 179 && !v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && v.isGated; });
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(2, 1)
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 173 && !v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && v.isGated; });
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 1, 60, 184, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0)
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && !v.isGated; });

        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Overlapping PCK on with Wildcard Off")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 179, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 184, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        REQUIRE_VOICE_MATCH_FN(3, [](const vc_t &v) { return v.key() == 60; });

        vm.processNoteOffEvent(0, 1, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }
}

TEST_CASE("Note ID in Poly Piano Mode")
{
    // REQUIRE_INCOMPLETE_TEST;
}

TEST_CASE("Note ID in Mono Mode")
{
    // REQUIRE_INCOMPLETE_TEST;
}

TEST_CASE("Note ID in Mono Legato Mode")
{
    // REQUIRE_INCOMPLETE_TEST;
}