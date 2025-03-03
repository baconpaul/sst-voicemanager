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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_RESPONDERPROXY_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_RESPONDERPROXY_H

// This is included in parent in a nasty way so is already in the namespace.
namespace details
{
template <typename VM_t> struct ResponderProxy
{
    VM_t &vm;

    using Cfg = typename VM_t::cfg_t;

    ResponderProxy(VM_t &vm) : vm(vm) {}

    void setVoiceEndCallback(std::function<void(typename Cfg::voice_t *)> f)
    {
        vm.responderUnderlyer.setVoiceEndCallback(f);
    }
    void retriggerVoiceWithNewNoteID(typename Cfg::voice_t *v, int32_t id, float f)
    {
        vm.responderUnderlyer.retriggerVoiceWithNewNoteID(v, id, f);
    }

    void moveVoice(typename Cfg::voice_t *v, uint16_t port, uint16_t channel, uint16_t key, float f)
    {
        vm.responderUnderlyer.moveVoice(v, port, channel, key, f);
    }

    void moveAndRetriggerVoice(typename Cfg::voice_t *v, uint16_t port, uint16_t channel,
                               uint16_t key, float f)
    {
        vm.responderUnderlyer.moveAndRetriggerVoice(v, port, channel, key, f);
    }

    void discardHostVoice(int32_t id) { vm.responderUnderlyer.discardHostVoice(id); }

    int32_t beginVoiceCreationTransaction(typename VoiceBeginBufferEntry<Cfg>::buffer_t &b,
                                          uint16_t port, uint16_t channel, uint16_t key,
                                          int32_t noteid, float vel)
    {
        return vm.responderUnderlyer.beginVoiceCreationTransaction(b, port, channel, key, noteid,
                                                                   vel);
    }

    int32_t initializeMultipleVoices(
        int32_t voices,
        const typename VoiceInitInstructionsEntry<Cfg>::buffer_t &voiceInitInstructionBuffer,
        typename VoiceInitBufferEntry<Cfg>::buffer_t &voiceInitWorkingBuffer, uint16_t port,
        uint16_t channel, uint16_t key, int32_t noteId, float velocity, float retune)
    {
        return vm.responderUnderlyer.initializeMultipleVoices(voices, voiceInitInstructionBuffer,
                                                              voiceInitWorkingBuffer, port, channel,
                                                              key, noteId, velocity, retune);
    }

    void endVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key, int32_t id,
                                     float f)
    {
        vm.responderUnderlyer.endVoiceCreationTransaction(port, channel, key, id, f);
    }
    void terminateVoice(typename Cfg::voice_t *v) { vm.responderUnderlyer.terminateVoice(v); }

    void releaseVoice(typename Cfg::voice_t *v, float f)
    {
        vm.responderUnderlyer.releaseVoice(v, f);
    }

    void setNoteExpression(typename Cfg::voice_t *v, int32_t e, double val)
    {
        vm.responderUnderlyer.setNoteExpression(v, e, val);
    }

    void setVoicePolyphonicParameterModulation(typename Cfg::voice_t *v, uint32_t e, double val)
    {
        vm.responderUnderlyer.setVoicePolyphonicParameterModulation(v, e, val);
    }

    void setVoiceMonophonicParameterModulation(typename Cfg::voice_t *v, uint32_t e, double val)
    {
        vm.responderUnderlyer.setVoiceMonophonicParameterModulation(v, e, val);
    }

    void setPolyphonicAftertouch(typename Cfg::voice_t *v, int8_t val)
    {
        vm.responderUnderlyer.setPolyphonicAftertouch(v, val);
    }
    void setVoiceMIDIMPEChannelPitchBend(typename Cfg::voice_t *v, uint16_t b)
    {
        vm.responderUnderlyer.setVoiceMIDIMPEChannelPitchBend(v, b);
    }
    void setVoiceMIDIMPEChannelPressure(typename Cfg::voice_t *v, int8_t p)
    {
        vm.responderUnderlyer.setVoiceMIDIMPEChannelPressure(v, p);
    }
    void setVoiceMIDIMPETimbre(typename Cfg::voice_t *v, int8_t t)
    {
        vm.responderUnderlyer.setVoiceMIDIMPETimbre(v, t);
    }
};

template <typename VM_t> struct MonoResponderProxy
{
    VM_t &vm;
    MonoResponderProxy(VM_t &vm) : vm(vm) {}

    void setMIDIPitchBend(int16_t ch, int16_t v)
    {
        vm.monoResponderUnderlyer.setMIDIPitchBend(ch, v);
    }
    void setMIDIChannelPressure(int16_t ch, int16_t v)
    {
        vm.monoResponderUnderlyer.setMIDIChannelPressure(ch, v);
    }
    void setMIDI1CC(int16_t ch, int16_t cc, int8_t val)
    {
        vm.monoResponderUnderlyer.setMIDI1CC(ch, cc, val);
    }
};
} // namespace details
#endif // VOICEMANAGER_RESPONDERPROXY_H
