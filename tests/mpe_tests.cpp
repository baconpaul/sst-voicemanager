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

TEST_CASE("MPE Basic")
{
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = TestPlayer<32, false>::voiceManager_t::MIDI1_MPE;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 2, 62, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(2, 2);

    REQUIRE(tp.activeVoicesMatching(
                [](auto &v) {
                    return (v.key() == 60 && v.channel() == 1) ||
                           (v.key() == 62 && v.channel() == 2);
                }) == 2);
    vm.routeMIDIPitchBend(0, 1, 9000);
    tp.dumpAllVoices();

    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000); }) == 1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 62 && v.channel() == 2 && v.mpeBend == 0); }) == 1);

    vm.routeChannelPressure(0, 2, 77);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 62 && v.channel() == 2 && v.mpePressure == 77); }) == 1);

    vm.routeMIDI1CC(0, 1, 74, 13);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 60 && v.channel() == 1 && v.mpeTimbre == 13); }) == 1);
}

TEST_CASE("MPE After Release")
{
    INFO("MPE has one gated voice per channel. Test that by doing a release and send");
    auto tp = TestPlayer<32, false>();

    auto &vm = tp.voiceManager;
    vm.dialect = TestPlayer<32, false>::voiceManager_t::MIDI1_MPE;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(1, 1);

    REQUIRE(tp.activeVoicesMatching([](auto &v) { return (v.key() == 60 && v.channel() == 1); }) ==
            1);
    vm.routeMIDIPitchBend(0, 1, 9000);
    vm.routeChannelPressure(0, 1, 88);
    vm.routeMIDI1CC(0, 1, 74, 17);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && v.isGated);
                }) == 1);
    tp.processFor(3);
    vm.processNoteOffEvent(0, 1, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && !v.isGated);
                }) == 1);
    tp.processFor(1);

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(2, 1);

    REQUIRE(tp.activeVoicesMatching([](auto &v) { return (v.key() == 60 && v.channel() == 1); }) ==
            2);
    vm.routeMIDIPitchBend(0, 1, 7000);
    vm.routeChannelPressure(0, 1, 14);
    vm.routeMIDI1CC(0, 1, 74, 55);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && !v.isGated);
                }) == 1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 7000 &&
                            v.mpePressure == 14 && v.mpeTimbre == 55 && v.isGated);
                }) == 1);
}