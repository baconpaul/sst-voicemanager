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

        SST_VOICEMANAGER_RT_REGION;

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

TEST_CASE("Retune Parameter Reaches Voice")
{
    INFO("The 6th arg of processNoteOnEvent (retune) is forwarded to initializeMultipleVoices "
         "and must arrive verbatim on the created voice.");

    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.37f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 60; },
                                [](auto &v) { return v.retune == 0.37f; }));

    // A second note with a different (negative) retune; the first voice is unaffected
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, -1.5f);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 64; },
                                [](auto &v) { return v.retune == -1.5f; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 60; },
                                [](auto &v) { return v.retune == 0.37f; }));
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

TEST_CASE("AllNotesOff Releases Sustained And Mono Voices")
{
    SECTION("Sustain-Held Voices Are Released")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f);
        vm.updateSustainPedal(0, 0, 127);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2f);
        // 60 is now held only by sustain, so it is still counted as gated
        REQUIRE_VOICE_COUNTS(2, 2);

        vm.allNotesOff();
        // Both voices ungate, including the sustain-held one
        REQUIRE_VOICE_COUNTS(2, 0);

        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("Mono Voice Is Released")
    {
        using vm_t = TestPlayer<32>::voiceManager_t;
        using MF = vm_t::MonoPlayModeFeatures;

        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_MONO);
        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f); // mono: still one voice
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.allNotesOff();
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }
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

TEST_CASE("AllSoundsOffMatching")
{
    SECTION("Predicate Matching None Leaves All Voices Intact")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        for (int i = 0; i < 5; ++i)
            vm.processNoteOnEvent(0, 0, 60 + i, -1, 0.5, 0);

        REQUIRE_VOICE_COUNTS(5, 5);

        vm.allSoundsOffMatching([](auto *) { return false; });

        REQUIRE_VOICE_COUNTS(5, 5);
    }

    SECTION("Predicate Matching Subset Terminates Only Matching Voices")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        // Start keys 60-64 (5 voices)
        for (int i = 0; i < 5; ++i)
            vm.processNoteOnEvent(0, 0, 60 + i, -1, 0.5, 0);

        REQUIRE_VOICE_COUNTS(5, 5);

        // Terminate even keys: 60, 62, 64
        vm.allSoundsOffMatching([](auto *v) { return v->key() % 2 == 0; });

        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 61; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 63; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 0; }) == 0);
    }

    SECTION("Predicate Matching All Terminates Everything")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        for (int i = 0; i < 6; ++i)
            vm.processNoteOnEvent(0, 0, 58 + i, -1, 0.5, 0);

        REQUIRE_VOICE_COUNTS(6, 6);

        vm.allSoundsOffMatching([](auto *) { return true; });

        REQUIRE_NO_VOICES;
    }

    SECTION("Predicate Matching Releasing Voices Terminates Them Early")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.5, 0);
        vm.processNoteOnEvent(0, 0, 62, -1, 0.5, 0);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.5, 0);
        REQUIRE_VOICE_COUNTS(3, 3);

        // Release 60 and 64 — they are now releasing but still consuming slots
        vm.processNoteOffEvent(0, 0, 60, -1, 0.5);
        vm.processNoteOffEvent(0, 0, 64, -1, 0.5);
        REQUIRE_VOICE_COUNTS(3, 1);

        // Terminate only the releasing voices (not gated)
        vm.allSoundsOffMatching([](auto *v) { return !v->isGated; });

        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 62; }) == 1);
    }
}

TEST_CASE("AllSoundsOff Mono Mode")
{
    using vm_t = TestPlayer<32>::voiceManager_t;
    using MF = vm_t::MonoPlayModeFeatures;

    SECTION("Terminates The Mono Voice And Does Not Resurrect On Release")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_LEGATO);
        REQUIRE_NO_VOICES;

        // Two keys held; legato mono collapses to a single voice
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.allSoundsOff();
        REQUIRE_NO_VOICES;

        // allSoundsOff terminates instantly, so releasing a still-"held" key must
        // not retrigger a fresh voice from the leftover held-key state
        vm.processNoteOffEvent(0, 0, 64, -1, 0.2f);
        REQUIRE_NO_VOICES;
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2f);
        REQUIRE_NO_VOICES;
    }

    SECTION("allSoundsOffMatching across mono groups")
    {
        // Duophonic: even keys -> group 1, odd keys -> group 2, each its own mono voice
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        tp.polyGroupForKey = [](auto k) { return k % 2 == 0 ? 1 : 2; };
        vm.setPlaymode(1, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_MONO);
        vm.setPlaymode(2, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_MONO);
        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f); // group 1
        vm.processNoteOnEvent(0, 0, 61, -1, 0.8f, 0.f); // group 2
        REQUIRE_VOICE_COUNTS(2, 2);

        // Terminate only the even-key (group 1) mono voice
        vm.allSoundsOffMatching([](auto *v) { return v->key() % 2 == 0; });
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 61; }) == 1);
    }
}

TEST_CASE("Cross Channel Sustain Pedal")
{
    SECTION("One Note, Sus Other Channel, MIDI1")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.updateSustainPedal(0, 2, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(0, 0);

        vm.updateSustainPedal(0, 2, 0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
        ;
    }

    SECTION("Two Note, Sus One of the Channels")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 2, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.updateSustainPedal(0, 2, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(2, 1);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(0, 2, 64, -1, 0.4);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(0, 2, 0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
        ;
    }

    SECTION("MPE Mode Global Channel")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1_MPE;
        vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 2, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 1, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(2, 2);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(2, 2);

        vm.processNoteOffEvent(0, 2, 64, -1, 0.4);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(2, 0);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }
}

TEST_CASE("VM Voice Count drops post release")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    REQUIRE(vm.getVoiceCount() == 0);
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    tp.processFor(3);
    REQUIRE(vm.getVoiceCount() == 1);
    vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
    tp.processFor(3);
    REQUIRE(vm.getVoiceCount() == 1);
    tp.processFor(50);
    REQUIRE(vm.getVoiceCount() == 0);
}

TEST_CASE("HeldMIDIKeyByChannel Basic State")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    // Initial state: no keys held
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][60]);

    // Press key 60 ch 0
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE(vm.heldMIDIKeyByChannel[0][60]);
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][61]);

    // Press key 61 ch 1
    vm.processNoteOnEvent(0, 1, 61, -1, 0.8, 0.0);
    REQUIRE(vm.heldMIDIKeyByChannel[1][61]);
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][61]); // different channel

    // Release key 60 ch 0
    vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][60]);
    REQUIRE(vm.heldMIDIKeyByChannel[1][61]); // still held on ch 1
}

TEST_CASE("HeldMIDIKeyByChannel Note Off Without Note On")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    // Spurious note-off for a key that was never pressed — must not crash
    vm.processNoteOffEvent(0, 0, 60, -1, 0.4);

    // State should remain false
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][60]);
}

TEST_CASE("HeldMIDIKeyByChannel Double Note On")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    // Press key 60 ch 0 twice without a note-off in between
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE(vm.heldMIDIKeyByChannel[0][60]);

    // One note-off should be enough to clear it
    vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
    REQUIRE_FALSE(vm.heldMIDIKeyByChannel[0][60]);
}