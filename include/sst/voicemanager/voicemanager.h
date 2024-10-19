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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H

#include <array>
#include <cassert>

/**
 * \mainpage SST Voice Manager
 *
 * The SST Voice Manager abstracts the concept of voice management into a header-only class
 * which collaborates with your synth using 'responders', or objects which can undertake
 * activities around a single voice or single monophonic value.
 */

namespace sst::voicemanager
{
template <typename Cfg, typename Responder, typename MonoResponder> struct VoiceManager
{
    enum MIDI1Dialect
    {
        MIDI1,
        MIDI1_MPE
    } dialect{MIDI1};

    /*
     * If a key is struck twice while still gated or sustained, do we start a new voice
     * or do we re-use the voice (and move the note id)
     */
    enum RepeatedKeyMode
    {
        MULTI_VOICE,
        PIANO
    } repeatedKeyMode{MULTI_VOICE};

    int8_t mpeGlobalChannel{0};
    int8_t mpeTimbreCC{74};

    Responder &responder;
    MonoResponder &monoResponder;
    VoiceManager(Responder &r, MonoResponder &m);

    void registerVoiceEndCallback();

    bool processNoteOnEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                            float velocity, float retune);

    void processNoteOffEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             float velocity);

    void updateSustainPedal(int16_t port, int16_t channel, int8_t level);

    void routeMIDIPitchBend(int16_t port, int16_t channel, int16_t pb14bit);

    void routeMIDI1CC(int16_t port, int16_t channel, int8_t cc, int8_t val);

    void routePolyphonicAftertouch(int16_t port, int16_t channel, int16_t key, int8_t pat);

    void routeChannelPressure(int16_t port, int16_t channel, int8_t pat);
    void routeNoteExpression(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             int32_t expression, double value);

    void routePolyphonicParameterModulation(int16_t port, int16_t channel, int16_t key,
                                            int32_t noteid, uint32_t parameter, double value);

    size_t getVoiceCount() const;
    size_t getGatedVoiceCount() const;
    void allNotesOff();
    void allSoundsOff();

    static float midiToFloatVelocity(uint8_t vel) { return 1.f * vel / 127.f; }

  private:
    struct Details;
    Details details;
};
} // namespace sst::voicemanager

#include "voicemanager_impl.h"

#endif // INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
