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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_CONSTRAINTS_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_CONSTRAINTS_H

#include <type_traits>
#include <functional>
#include <array>
#include <cstdint>

namespace sst::voicemanager::details
{

#define FLD(XX, r, ...) std::is_same_v<decltype(&r::XX), __VA_ARGS__>

#define HASMEM(XX, r, ...)                                                                         \
    template <typename T> class has_##XX                                                           \
    {                                                                                              \
        typedef uint64_t yes;                                                                      \
        typedef uint8_t no;                                                                        \
        template <typename C> static yes &test(decltype(&C::XX));                                  \
        template <typename> static no &test(...);                                                  \
                                                                                                   \
      public:                                                                                      \
        static const bool value = sizeof(test<T>(nullptr)) == sizeof(yes);                         \
    };                                                                                             \
    static_assert(has_##XX<r>::value, "Responder missing function: " #XX);                         \
    static_assert(std::is_same<decltype(&r::XX), __VA_ARGS__>::value, "Signature "                 \
                                                                      "incorrect: " #XX);

template <typename Cfg, typename Responder, typename MonoResponder> struct Constraints
{
    static_assert(Cfg::maxVoiceCount > 0, "The voice manager requires a static voice count");
    static_assert(std::is_pointer_v<typename Cfg::voice_t *>,
                  "The voice manager Cfg must define voice_t which can be a pointer");

    HASMEM(setVoiceEndCallback, Responder,
           void (Responder::*)(std::function<void(typename Cfg::voice_t *)>))
    HASMEM(retriggerVoiceWithNewNoteID, Responder,
           void (Responder::*)(typename Cfg::voice_t *, int32_t, float))

    HASMEM(beginVoiceCreationTransaction, Responder,
           int32_t (Responder::*)(uint16_t, uint16_t, uint16_t, int32_t, float))
    HASMEM(endVoiceCreationTransaction, Responder,
           void (Responder::*)(uint16_t, uint16_t, uint16_t, int32_t, float))
    HASMEM(terminateVoice, Responder, void (Responder::*)(typename Cfg::voice_t *))
    HASMEM(initializeMultipleVoices, Responder,
           int32_t (Responder::*)(std::array<typename Cfg::voice_t *, Cfg::maxVoiceCount> &,
                                  uint16_t, uint16_t, uint16_t, int32_t, float, float))
    HASMEM(releaseVoice, Responder, void (Responder::*)(typename Cfg::voice_t *, float))
    HASMEM(setNoteExpression, Responder,
           void (Responder::*)(typename Cfg::voice_t *, int32_t, double))
    HASMEM(setVoicePolyphonicParameterModulation, Responder,
           void (Responder::*)(typename Cfg::voice_t *, uint32_t, double))
    HASMEM(setPolyphonicAftertouch, Responder, void (Responder::*)(typename Cfg::voice_t *, int8_t))

    HASMEM(setVoiceMIDIMPEChannelPitchBend, Responder,
           void (Responder::*)(typename Cfg::voice_t *, uint16_t))
    HASMEM(setVoiceMIDIMPEChannelPressure, Responder,
           void (Responder::*)(typename Cfg::voice_t *, int8_t))
    HASMEM(setVoiceMIDIMPETimbre, Responder, void (Responder::*)(typename Cfg::voice_t *, int8_t))

    HASMEM(setMIDIPitchBend, MonoResponder, void (MonoResponder::*)(int16_t, int16_t))
    HASMEM(setMIDI1CC, MonoResponder, void (MonoResponder::*)(int16_t, int16_t, int8_t))
    HASMEM(setMIDIChannelPressure, MonoResponder, void (MonoResponder::*)(int16_t, int16_t))

    static constexpr bool satisfies() { return true; }
};

#undef HASMEM

} // namespace sst::voicemanager::details

#endif // VOICEMANAGER_CONSTRAINTS_H
