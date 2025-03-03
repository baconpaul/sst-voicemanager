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
#include <cstdint>
#include <cstddef>
#include <limits>
#include <algorithm>

/**
 * \mainpage SST Voice Manager
 *
 * The SST Voice Manager abstracts the concept of voice management into a header-only class
 * which collaborates with your synth using 'responders', or objects which can undertake
 * activities around a single voice or single monophonic value.
 */

namespace sst::voicemanager
{

/**
 * VoiceInitBufferEntry is the object which the responder needs to populate
 * in the voice initiation creation lifecycle.
 *
 * @tparam Cfg the voice manager configuration trait
 */
template <typename Cfg> struct VoiceInitBufferEntry
{
    typename Cfg::voice_t *voice{nullptr}; ///< A pointer to the voice, owned by the responder

    /**
     * buffer_t is the typedef for the working group array you will receive
     * in init voices. Use `buffer_t &` rather than the array expansions.
     */
    using buffer_t = std::array<VoiceInitBufferEntry<Cfg>, Cfg::maxVoiceCount>;
};

/**
 * VoiceInitInstructionsEntry is how the voice manager gives instructions to the synth
 * to start or restart a voice.
 */
template <typename Cfg> struct VoiceInitInstructionsEntry
{
    enum struct Instruction
    {
        START, ///< Start a new voice at this entry
        SKIP,  ///< Skip this voice altogether. The voice manager has discarded it
    } instruction{Instruction::START};

    using buffer_t = std::array<VoiceInitInstructionsEntry<Cfg>, Cfg::maxVoiceCount>;
};

/**
 * VoiceBeginBufferEntry is the object which the responder needs to populate
 * in the voice begin creation lifecycle.
 *
 * @tparam Cfg the voice manager configuration trait
 */
template <typename Cfg> struct VoiceBeginBufferEntry
{
    uint64_t polyphonyGroup; ///< The polyphony group in which this voice participates

    /**
     * buffer_t is the typedef for the working group array you will receive
     * in init voices. Use `buffer_t &` rather than the array expansions.
     */
    using buffer_t = std::array<VoiceBeginBufferEntry<Cfg>, Cfg::maxVoiceCount>;
};

#include "voicemanager_responderproxy.h"

/**
 * VoiceManager is the main class used for voice management, and the sole public API.
 * The voice manager has the following features
 * - blah
 * - blah
 * - blah
 *
 * The VoiceManager collaborates with your synth through three
 *
 * @tparam Cfg A configuration trait, setting the hard (physical) voice limit and voice type
 * @tparam Responder A class responsible for responding to voice-level activities such as creation,
 * termination, setting of voice level properties, and so forth
 * @tparam MonoResponder A class responsible for responding to monophonic activities such as MIDI CC
 * and Channel PitchBend (which are monophonic in non-MPE mode)
 */
template <typename Cfg, typename Responder, typename MonoResponder> struct VoiceManager
{
    using cfg_t = Cfg;
    using responder_t = Responder;
    using monoResponder_t = MonoResponder;

    typedef VoiceInitInstructionsEntry<Cfg> initInstruction_t;

    enum struct MIDI1Dialect
    {
        MIDI1,
        MIDI1_MPE
    } dialect{MIDI1Dialect::MIDI1};

    /**
     * If a key is struck twice while still gated or sustained, do we start a new voice
     * or do we re-use the voice (and move the note id) or do we trigger a second voice
     */
    enum struct RepeatedKeyMode
    {
        MULTI_VOICE,
        PIANO
    } repeatedKeyMode{RepeatedKeyMode::MULTI_VOICE};

    /**
     * The voice manager can either run in a mode where it manages to voice limits in a polyphonic
     * mode (but multi-voice notes still steal together) or can manage to a single note with
     * essentially unlimited voices. This is controlled by the group PlayMode.
     */
    enum struct PlayMode
    {
        POLY_VOICES, ///< The voice manager manages voice counts from any number of keys in piano
                     ///< mode
        MONO_NOTES ///< The voice manager makes sure the consequence of only one key is playing at a
                   ///< time, independent of voices
    };

    /**
     * Mono Playmode is somewhat ambiguous, meaning many things. So let's enumerate the features.
     * Hope we have fewer than 64. Even though these are using the bits as features they are not all
     * possible to be active simultaneously. As well as distinct bit values, we provide a few
     * preset | combinations of flags for common use cases.
     */
    enum struct MonoPlayModeFeatures : uint64_t
    {
        NONE = 0,
        MONO_RETRIGGER = 1 << 0, ///< A new keypress triggers a new voice
        MONO_LEGATO = 1 << 1,    ///< A new keypress moves the playing voice

        ON_RELEASE_TO_LATEST = 1 << 2,  ///< mono release return to latest
        ON_RELEASE_TO_HIGHEST = 1 << 3, ///< release to highest
        ON_RELEASE_TO_LOWEST = 1 << 4,  ///< release to lowest

        NATURAL_MONO = MONO_RETRIGGER | ON_RELEASE_TO_LATEST, ///< What a 'mono' button would do
        NATURAL_LEGATO = MONO_LEGATO | ON_RELEASE_TO_LATEST,  ///< What a 'legato' button would do
    };

    /**
     * StealingPriorityMode determines how to pick a voice to steal when an appropriate voice or
     * note limit is met.  HIGHEST and LOWEST are in midi-key space.
     */
    enum struct StealingPriorityMode
    {
        OLDEST,
        HIGHEST,
        LOWEST
    };

    int8_t mpeGlobalChannel{0};
    int8_t mpeTimbreCC{74};

    Responder &responderUnderlyer;
    MonoResponder &monoResponderUnderlyer;

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
                                            int32_t voiceid, uint32_t parameter, double value);
    void routeMonophonicParameterModulation(int16_t port, int16_t channel, int16_t key,
                                            uint32_t parameter, double value);

    [[nodiscard]] size_t getVoiceCount() const;
    [[nodiscard]] size_t getGatedVoiceCount() const;
    void allNotesOff();
    void allSoundsOff();

    void guaranteeGroup(uint64_t groupId);
    void setPolyphonyGroupVoiceLimit(uint64_t groupId, int32_t limit);
    void setPlaymode(uint64_t groupId, PlayMode pm,
                     uint64_t features = static_cast<uint64_t>(MonoPlayModeFeatures::NONE));
    void setStealingPriorityMode(uint64_t groupId, StealingPriorityMode pm);

  private:
    using responderProxy_t = details::ResponderProxy<VoiceManager<Cfg, Responder, MonoResponder>>;
    responderProxy_t responder;
    using monoResponderProxy_t =
        details::MonoResponderProxy<VoiceManager<Cfg, Responder, MonoResponder>>;
    monoResponderProxy_t monoResponder;

    struct Details;
    Details details;
};
} // namespace sst::voicemanager

#include "voicemanager_impl.h"

#endif // INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
