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

TEST_CASE("Routing Midi CC")
{
    auto tp = TestPlayer<32, true>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    REQUIRE(vm.dialect == TestPlayer<32, true>::voiceManager_t::MIDI1);

    // Test one: Does routing midi cc on cc 0 and 6 stay independent
    vm.routeMIDI1CC(0, 0, 0, 17);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 0);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 0, 6, 23);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 1, 6, 88);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 4, 6, 74);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 74);

    // And in MIDI1 mode make sure that CC 74 works properly
    vm.routeMIDI1CC(0, 0, 74, 63);
    REQUIRE(tp.midi1CC[0][74] == 63);
}

TEST_CASE("Routing Mono Pitch Bend")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.pitchBend[0] == 0);
    REQUIRE(tp.pitchBend[4] == 0);
    tp.processFor(3);

    vm.routeMIDIPitchBend(0, 0, 9000);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 0);

    vm.routeMIDIPitchBend(0, 2, 74);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 0);

    vm.routeMIDIPitchBend(0, 4, 4000);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 4000);
}

TEST_CASE("Routing Channel AT")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.channelPressure[0] == 0);
    tp.processFor(3);

    vm.routeChannelPressure(0, 0, 17);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 0);

    vm.routeChannelPressure(0, 2, 85);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 0);

    vm.routeChannelPressure(0, 4, 71);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 71);
}

TEST_CASE("Routing Poly AT")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    vm.routePolyphonicAftertouch(0, 0, 55, 17);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 0; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 0; }) == 3);

    vm.routePolyphonicAftertouch(0, 0, 85, 23);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    // Make sure off port channel key routing doesnt screw us up

    vm.routePolyphonicAftertouch(0, 2, 85, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    vm.routePolyphonicAftertouch(2, 0, 85, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    vm.routePolyphonicAftertouch(0, 0, 83, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);
}

TEST_CASE("Routing Note Expressions")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, 10455, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, 10485, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    // Test basic routing
    vm.routeNoteExpression(0, 0, 55, 10455, 3, 0.74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.empty(); }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 3);

    vm.routeNoteExpression(0, 0, 85, 10485, 2, 0.77);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.noteExpressionCache.find(2) != v.noteExpressionCache.end(); }) == 3);

    vm.routeNoteExpression(0, 0, 55, 10455, 2, 0.11);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }) ==
            3);

    // Does routing to the wrong note id not update me?

    vm.routeNoteExpression(0, 0, 55, 70455, 2, 0.99);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }) ==
            3);

    // Add a second note 55 and try and modulate it
    vm.processNoteOnEvent(0, 0, 55, 20455, 0.5, 0);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 1);

    vm.routeNoteExpression(0, 0, 55, 20455, 2, -0.33);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 0);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v) {
                    return v.key() == 55 &&
                           v.noteExpressionCache.find(2) != v.noteExpressionCache.end();
                }) == 2);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v)
                                {
                                    auto nexp = v.noteExpressionCache.at(2);
                                    return nexp == -0.33 || nexp == 0.11;
                                }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 10455; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));

    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 20455; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == -0.33; }));
}

TEST_CASE("Routing Poly Parameter Modulations")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, 10455, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, 10485, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    // Test basic routing
    vm.routePolyphonicParameterModulation(0, 0, 55, 10455, 3, 0.74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.empty(); }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 3);

    vm.routePolyphonicParameterModulation(0, 0, 85, 10485, 2, 0.77);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.paramModulationCache.find(2) != v.paramModulationCache.end(); }) == 3);

    vm.routePolyphonicParameterModulation(0, 0, 55, 10455, 2, 0.11);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.at(2) == 0.77; }) ==
            3);

    // Does routing to the wrong note id not update me?

    vm.routePolyphonicParameterModulation(0, 0, 55, 70455, 2, 0.99);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.at(2) == 0.77; }) ==
            3);

    // Add a second note 55 and try and modulate it
    vm.processNoteOnEvent(0, 0, 55, 20455, 0.5, 0);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 1);

    vm.routePolyphonicParameterModulation(0, 0, 55, 20455, 2, -0.33);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 0);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v) {
                    return v.key() == 55 &&
                           v.paramModulationCache.find(2) != v.paramModulationCache.end();
                }) == 2);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v)
                                {
                                    auto nexp = v.paramModulationCache.at(2);
                                    return nexp == -0.33 || nexp == 0.11;
                                }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 10455; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));

    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 20455; },
                                [](auto &v) { return v.paramModulationCache.at(2) == -0.33; }));
}
