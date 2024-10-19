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

TEST_CASE("Basic Poly Note On Note Off")
{
    SECTION("OnOff")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60};
        int32_t noteid{-1};
        float retune{0.f}, velocity{0.8}, rvelocity{0.2};

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(1, 1);

        REQUIRE(tp.getActiveVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getGatedVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            tp.process();
        }

        // send a midi message note off, see voice stay constant but gated voice drop down
        vm.processNoteOffEvent(port, channel, key, noteid, rvelocity);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.getActiveVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.activeVoicesMatching(
                    [key, velocity, rvelocity](auto &v)
                    {
                        auto res = v.key() == key;
                        res = res && (!v.isGated);
                        res = res && (v.velocity == velocity);
                        res = res && (v.releaseVelocity == rvelocity);
                        return res;
                    }) == 1);

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            tp.process();
        }

        REQUIRE_NO_VOICES;
    }

    SECTION("OnOff at Layer Region")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{84};
        int32_t noteid{-1};
        float retune{0.f}, velocity{0.8};

        REQUIRE_NO_VOICES

        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(3, 3);

        REQUIRE(tp.getActiveVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getActiveVoicePCKNS()[1] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getActiveVoicePCKNS()[2] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getGatedVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getGatedVoicePCKNS()[1] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getGatedVoicePCKNS()[2] == std::make_tuple(port, channel, key, noteid));

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            tp.process();
        }

        // send a midi message note off, see voice stay constant but gated voice drop down
        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE_VOICE_COUNTS(3, 0);

        REQUIRE(tp.getActiveVoicePCKNS()[0] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getActiveVoicePCKNS()[1] == std::make_tuple(port, channel, key, noteid));
        REQUIRE(tp.getActiveVoicePCKNS()[2] == std::make_tuple(port, channel, key, noteid));

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            tp.process();
        }

        REQUIRE_NO_VOICES;
    }

    SECTION("Three Note Chord")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key0{60};
        int32_t noteid{-1};
        float retune{0.f}, velocity{0.7};

        for (int i = 0; i < 3; ++i)
        {
            vm.processNoteOnEvent(port, channel, key0 + i * 4, noteid, velocity, retune);
            REQUIRE_VOICE_COUNTS(i + 1, i + 1);
            for (int q = 0; q < 10; ++q)
                tp.process();

            // It happens they are ordered here so back works but that's not a requirement
            REQUIRE(tp.getActiveVoicePCKNS().back() ==
                    std::make_tuple(port, channel, key0 + i * 4, noteid));
        }

        for (int q = 0; q < 10; ++q)
            tp.process();

        for (int i = 0; i < 3; ++i)
        {
            vm.processNoteOffEvent(port, channel, key0 + i * 4, noteid, velocity);
            REQUIRE_VOICE_COUNTS(3, 2 - i);
            for (int q = 0; q < 1; ++q)
                tp.process();
        }

        REQUIRE(vm.getGatedVoiceCount() == 0);

        auto vc = vm.getVoiceCount();
        for (int i = 0; i < 20; ++i)
        {
            tp.process();
            REQUIRE(vm.getVoiceCount() <= vc);
            vc = vm.getVoiceCount();
        }
        REQUIRE_NO_VOICES;
    }
}

TEST_CASE("Sustain Pedal")
{
    SECTION("Pedal Down Pre Note")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60}, velocity{90};
        int32_t noteid{-1};
        float retune{0.f};

        REQUIRE_NO_VOICES;

        vm.updateSustainPedal(port, channel, 127);
        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(4);

        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(port, channel, 0);
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("Pedal Down When Gated")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60}, velocity{90};
        int32_t noteid{-1};
        float retune{0.f};

        REQUIRE_NO_VOICES

        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(4);
        vm.updateSustainPedal(port, channel, 127);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(4);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.getActiveVoicePCKNS().back() == std::make_tuple(port, channel, key, noteid));

        vm.updateSustainPedal(port, channel, 0);
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("Capture Mix and Match")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60};
        int32_t noteid{-1};
        float velocity{0.8}, rvelocity{0.7}, retune{0.f};

        REQUIRE_NO_VOICES

        vm.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        vm.processNoteOnEvent(port, channel, key + 1, noteid, velocity, retune);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching(
                    [key, velocity](auto &v)
                    {
                        auto res = v.key() == key || v.key() == key + 1;
                        res = res && (v.velocity == velocity);
                        return res;
                    }) == 2);

        tp.processFor(4);
        vm.processNoteOffEvent(port, channel, key + 1, noteid, rvelocity);
        REQUIRE_VOICE_COUNTS(2, 1);

        vm.updateSustainPedal(port, channel, 127);
        REQUIRE_VOICE_COUNTS(2, 1);

        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(2, 1); // still releasing the one voice

        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1); // now its gone but the sustain pedal voice remains

        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE_VOICE_COUNTS(1, 1);

        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(port, channel, 0);
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }
}

TEST_CASE("AllNotesOff")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    REQUIRE_NO_VOICES;

    for (int i = 0; i < 8; ++i)
    {
        vm.processNoteOnEvent(0, 0, 58 + i, -1, 0.5, 0);
        tp.processFor(3);
    }

    REQUIRE_VOICE_COUNTS(8, 8);

    vm.allNotesOff();

    REQUIRE_VOICE_COUNTS(8, 0);
}

TEST_CASE("AllSoundsOff")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    REQUIRE_NO_VOICES;

    for (int i = 0; i < 8; ++i)
    {
        vm.processNoteOnEvent(0, 0, 58 + i, -1, 0.5, 0);
        tp.processFor(3);
    }

    REQUIRE_VOICE_COUNTS(8, 8);

    vm.allSoundsOff();

    REQUIRE_VOICE_COUNTS(0, 0);
}