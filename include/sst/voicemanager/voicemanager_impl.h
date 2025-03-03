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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_IMPL_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_IMPL_H

#include <type_traits>

#include "voicemanager_constraints.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace sst::voicemanager
{
static constexpr bool vmLog{false};
#define VML(...)                                                                                   \
    {                                                                                              \
        if constexpr (vmLog)                                                                       \
        {                                                                                          \
            std::cout << __FILE__ << ":" << __LINE__ << " " << __VA_ARGS__ << std::endl;           \
        }                                                                                          \
    }

template <typename Cfg, typename Responder, typename MonoResponder>
struct VoiceManager<Cfg, Responder, MonoResponder>::Details
{
    VoiceManager<Cfg, Responder, MonoResponder> &vm;
    Details(VoiceManager<Cfg, Responder, MonoResponder> &in) : vm(in)
    {
        std::fill(lastPBByChannel.begin(), lastPBByChannel.end(), 0);
        std::fill(sustainOn.begin(), sustainOn.end(), false);

        keyStateByPort[0] = {};
        guaranteeGroup(0);
    }

    int64_t mostRecentVoiceCounter{1};
    int64_t mostRecentTransactionID{1};

    struct VoiceInfo
    {
        int16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1}; // The note id is the id of the current playing note. In poly mode it is
                            // same as voice id while gated
        int32_t voiceId{
            -1}; // The voice id is the id of the current voice. When voices are re-cycled in legato
                 // and piano modes it can differ from note id. It is used for clap polymod.

        static constexpr size_t noteIdStackSize{256};
        std::array<int32_t, noteIdStackSize> noteIdStack{};
        size_t noteIdStackPos{0};

        int16_t originalPort{0}, originalChannel{0}, originalKey{0};

        int64_t voiceCounter{0}, transactionId{0};

        bool gated{false};
        bool gatedDueToSustain{false};

        uint64_t polyGroup{0};

        typename Cfg::voice_t *activeVoiceCookie{nullptr};

        bool matches(int16_t pt, int16_t ch, int16_t k, int32_t nid)
        {
            auto res = (activeVoiceCookie != nullptr);
            res = res && (pt == -1 || port == -1 || pt == port);
            res = res && (ch == -1 || channel == -1 || ch == channel);
            res = res && (k == -1 || key == -1 || k == key);
            if (nid != -1 && noteId != -1)
            {
                bool anyMatch{false};
                for (auto i = 0U; i < noteIdStackPos; ++i)
                    anyMatch = anyMatch || (noteIdStack[i] == nid);
                res = res && anyMatch;
            }
            return res;
        }

        bool matchesVoiceId(int32_t vid)
        {
            auto res = (activeVoiceCookie != nullptr);
            res = res && (vid == -1 || voiceId == -1 || vid == voiceId);
            return res;
        }

        void snapOriginalToCurrent()
        {
            originalPort = port;
            originalChannel = channel;
            originalKey = key;
        }

        void removeNoteIdFromStack(int32_t nid)
        {
            VML("- Remove note id from stack " << nid << " " << this);
            int shift{0};
            for (auto i = 0U; i < noteIdStackPos; ++i)
            {
                VML("   - NIDSTack at " << i << " " << noteIdStack[i] << " " << nid
                                        << " shift=" << shift);
                if (noteIdStack[i] == nid)
                {
                    shift = 1;
                }
                else
                {
                    noteIdStack[i] = noteIdStack[i + shift];
                }
            }
            noteIdStackPos--;
            if constexpr (vmLog)
            {
                VML("   - NIDSTack pos is now " << noteIdStackPos);
                for (auto i = 0U; i < noteIdStackPos; ++i)
                {
                    VML("      - " << i << " -> " << noteIdStack[i]);
                }
            }
        }
    };
    std::array<VoiceInfo, Cfg::maxVoiceCount> voiceInfo{};
    std::unordered_map<uint64_t, int32_t> polyLimits{};
    std::unordered_map<uint64_t, int32_t> usedVoices{};
    std::unordered_map<uint64_t, StealingPriorityMode> stealingPriorityMode{};
    std::unordered_map<uint64_t, PlayMode> playMode{};
    std::unordered_map<uint64_t, uint64_t> playModeFeatures{};
    int32_t totalUsedVoices{0};

    struct IndividualKeyState
    {
        int64_t transaction{0};
        float inceptionVelocity{0.f};
        bool heldBySustain{false};
    };
    using keyState_t =
        std::array<std::array<std::unordered_map<uint64_t, IndividualKeyState>, 128>, 16>;
    std::unordered_map<int32_t, keyState_t> keyStateByPort{};

    void guaranteeGroup(uint64_t groupId)
    {
        if (polyLimits.find(groupId) == polyLimits.end())
            polyLimits[groupId] = Cfg::maxVoiceCount;
        if (usedVoices.find(groupId) == usedVoices.end())
            usedVoices[groupId] = 0;
        if (stealingPriorityMode.find(groupId) == stealingPriorityMode.end())
            stealingPriorityMode[groupId] = StealingPriorityMode::OLDEST;
        if (playMode.find(groupId) == playMode.end())
            playMode[groupId] = PlayMode::POLY_VOICES;
        if (playModeFeatures.find(groupId) == playModeFeatures.end())
            playModeFeatures[groupId] = static_cast<uint64_t>(MonoPlayModeFeatures::NONE);
    }

    typename VoiceBeginBufferEntry<Cfg>::buffer_t voiceBeginWorkingBuffer{};
    typename VoiceInitBufferEntry<Cfg>::buffer_t voiceInitWorkingBuffer{};
    typename VoiceInitInstructionsEntry<Cfg>::buffer_t voiceInitInstructionsBuffer{};
    std::array<std::array<uint16_t, 128>, 16> midiCCCache{};
    std::array<bool, 16> sustainOn{};
    std::array<int16_t, 16> lastPBByChannel{};

    void doMonoPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        if (channel >= 0 && channel < static_cast<int16_t>(lastPBByChannel.size()))
            lastPBByChannel[channel] = static_cast<int16_t>(pb14bit - 8192);

        vm.monoResponder.setMIDIPitchBend(channel, pb14bit);
    }

    void doMPEPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, -1, -1) &&
                vi.gated) // all keys and notes on a channel for midi PB
            {
                vm.responder.setVoiceMIDIMPEChannelPitchBend(vi.activeVoiceCookie, pb14bit);
            }
        }
    }

    void doMonoChannelPressure(int16_t port, int16_t channel, int8_t val)
    {
        vm.monoResponder.setMIDIChannelPressure(channel, val);
    }

    void doMPEChannelPressure(int16_t port, int16_t channel, int8_t val)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.activeVoiceCookie && vi.port == port && vi.channel == channel && vi.gated)
            {
                vm.responder.setVoiceMIDIMPEChannelPressure(vi.activeVoiceCookie, val);
            }
        }
    }

    void endVoice(typename Cfg::voice_t *v)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.activeVoiceCookie == v)
            {
                --usedVoices.at(vi.polyGroup);
                --totalUsedVoices;
                VML("  - Ending voice " << vi.activeVoiceCookie << " pg=" << vi.polyGroup
                                        << " used now is " << usedVoices.at(vi.polyGroup) << " ("
                                        << totalUsedVoices << ")");
                vi.activeVoiceCookie = nullptr;
            }
        }
    }

    int32_t findNextStealableVoiceInfo(uint64_t polygroup, StealingPriorityMode pm,
                                       bool ignorePolygroup = false)
    {
        int32_t oldestGated{-1}, oldestNonGated{-1};
        int64_t gi{std::numeric_limits<int64_t>::max()}, ngi{gi};
        if (pm == StealingPriorityMode::HIGHEST)
        {
            gi = std::numeric_limits<int64_t>::min();
            ngi = gi;
        }

        VML("- Finding stealable from " << polygroup << " with ignore " << ignorePolygroup);

        for (int32_t vi = 0; vi < static_cast<int32_t>(voiceInfo.size()); ++vi)
        {
            const auto &v = voiceInfo[vi];
            if (!v.activeVoiceCookie)
            {
                VML("   - Skipping no-cookie at " << vi);
                continue;
            }
            if (v.polyGroup != polygroup && !ignorePolygroup)
            {
                VML("   - Skipping different group at " << vi);
                continue;
            }

            VML("   - Considering " << vi << " " << v.key << " " << gi << " " << v.voiceCounter);
            if (v.gated || v.gatedDueToSustain)
            {
                switch (pm)
                {
                case StealingPriorityMode::OLDEST:
                {
                    if (v.voiceCounter < gi)
                    {
                        oldestGated = vi;
                        gi = v.voiceCounter;
                    }
                }
                break;

                case StealingPriorityMode::HIGHEST:
                {
                    if (v.key > gi)
                    {
                        oldestGated = vi;
                        gi = v.key;
                    }
                }
                break;

                case StealingPriorityMode::LOWEST:
                {
                    if (v.key < gi)
                    {
                        oldestGated = vi;
                        gi = v.key;
                    }
                }
                break;
                }
            }
            else
            {
                switch (pm)
                {
                case StealingPriorityMode::OLDEST:
                {
                    if (v.voiceCounter < ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.voiceCounter;
                    }
                }
                break;

                case StealingPriorityMode::HIGHEST:
                {
                    if (v.key > ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.key;
                    }
                }
                break;

                case StealingPriorityMode::LOWEST:
                {
                    if (v.key < ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.key;
                    }
                }
                break;
                }
            }
        }
        if (oldestNonGated >= 0)
        {
            return oldestNonGated;
        }
        if (oldestGated >= 0)
        {
            VML("  - Found " << oldestGated);
            return oldestGated;
        }
        return -1;
    }

    void doMonoRetrigger(int16_t port, uint64_t polyGroup)
    {
        VML("=== MONO mode voice retrigger or move for " << polyGroup);
        auto &ks = keyStateByPort[port];
        auto ft = playModeFeatures.at(polyGroup);
        int dch{-1}, dk{-1};
        float dvel{0.f};

        auto findBestKey = [&](bool ignoreSustain)
        {
            if (ft & static_cast<uint64_t>(MonoPlayModeFeatures::ON_RELEASE_TO_LATEST))
            {
                int64_t mtx = 0;
                for (int ch = 0; ch < 16; ++ch)
                {
                    for (int k = 0; k < 128; ++k)
                    {
                        auto ksp = ks[ch][k].find(polyGroup);
                        if (ksp != ks[ch][k].end())
                        {
                            const auto [tx, vel, hbs] = ksp->second;
                            if (hbs != ignoreSustain)
                                continue;
                            VML("- Found note " << ch << " " << k << " " << tx << " " << vel << " "
                                                << hbs << " with ignore " << ignoreSustain);
                            if (mtx < tx)
                            {
                                mtx = tx;
                                dch = ch;
                                dk = k;
                                dvel = vel;
                            }
                        }
                    }
                }
            }
            else if (ft & static_cast<uint64_t>(MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST))
            {
                int64_t mk = 0;
                for (int ch = 0; ch < 16; ++ch)
                {
                    for (int k = 0; k < 128; ++k)
                    {
                        auto ksp = ks[ch][k].find(polyGroup);
                        if (ksp != ks[ch][k].end())
                        {
                            const auto [tx, vel, hbs] = ksp->second;
                            if (hbs != ignoreSustain)
                                continue;
                            if (tx != 0 && k > mk)
                            {
                                mk = k;
                                dch = ch;
                                dk = k;
                                dvel = vel;
                            }
                        }
                    }
                }
            }
            else if (ft & static_cast<uint64_t>(MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST))
            {
                int64_t mk = 1024;
                for (int ch = 0; ch < 16; ++ch)
                {
                    for (int k = 0; k < 128; ++k)
                    {
                        auto ksp = ks[ch][k].find(polyGroup);
                        if (ksp != ks[ch][k].end())
                        {
                            const auto [tx, vel, hbs] = ksp->second;
                            if (hbs != ignoreSustain)
                                continue;
                            if (tx != 0 && k < mk)
                            {
                                mk = k;
                                dch = ch;
                                dk = k;
                                dvel = vel;
                            }
                        }
                    }
                }
            }
            VML("- FindBestKey Result is " << dch << "/" << dk);
        };

        findBestKey(false);
        if (dch < 0)
            findBestKey(true);

        if (ft & static_cast<uint64_t>(MonoPlayModeFeatures::MONO_RETRIGGER) && dch >= 0 && dk >= 0)
        {
            // Need to know the velocity and the port
            VML("- retrigger Note " << dch << " " << dk << " " << dvel);

            // FIXME
            auto dnid = -1;

            // So now begin end voice transaction
            auto voicesToBeLaunched = vm.responder.beginVoiceCreationTransaction(
                voiceBeginWorkingBuffer, port, dch, dk, dnid, dvel);
            for (int i = 0; i < voicesToBeLaunched; ++i)
            {
                voiceInitInstructionsBuffer[i] = {};
                voiceInitWorkingBuffer[i] = {};
                if (voiceBeginWorkingBuffer[i].polyphonyGroup != polyGroup)
                {
                    voiceInitInstructionsBuffer[i].instruction =
                        VoiceInitInstructionsEntry<Cfg>::Instruction::SKIP;
                }
            }
            auto voicesLeft = vm.responder.initializeMultipleVoices(
                voicesToBeLaunched, voiceInitInstructionsBuffer, voiceInitWorkingBuffer, port, dch,
                dk, dnid, dvel, 0.f);
            auto idx = 0;
            while (!voiceInitWorkingBuffer[idx].voice)
                idx++;

            for (auto &vi : voiceInfo)
            {
                if (!vi.activeVoiceCookie)
                {
                    vi.voiceCounter = mostRecentVoiceCounter++;
                    vi.transactionId = mostRecentTransactionID;
                    vi.port = port;
                    vi.channel = dch;
                    vi.key = dk;
                    vi.noteId = dnid;
                    vi.snapOriginalToCurrent();

                    vi.gated = true;
                    vi.gatedDueToSustain = false;
                    vi.activeVoiceCookie = voiceInitWorkingBuffer[idx].voice;
                    vi.polyGroup = voiceBeginWorkingBuffer[idx].polyphonyGroup;

                    keyStateByPort[vi.port][vi.channel][vi.key][vi.polyGroup] = {vi.transactionId,
                                                                                 dvel};

                    VML("- New Voice assigned with "
                        << mostRecentVoiceCounter << " at pckn=" << port << "/" << dch << "/" << dk
                        << "/" << dnid << " pg=" << vi.polyGroup);

                    ++usedVoices.at(vi.polyGroup);
                    ++totalUsedVoices;

                    --voicesLeft;
                    if (voicesLeft == 0)
                    {
                        break;
                    }
                    idx++;
                    while (!voiceInitWorkingBuffer[idx].voice)
                        idx++;
                }
            }

            vm.responder.endVoiceCreationTransaction(port, dch, dk, dnid, dvel);
        }

        else if (ft & static_cast<uint64_t>(MonoPlayModeFeatures::MONO_LEGATO) && dch >= 0 &&
                 dk >= 0)
        {
            VML("- Move notes in group " << polyGroup << " to " << dch << "/" << dk);
            for (auto &v : voiceInfo)
            {
                if (v.activeVoiceCookie && v.polyGroup == polyGroup)
                {
                    if (v.gated || v.gatedDueToSustain)
                    {
                        VML("- Move gated voice");
                        vm.responder.moveVoice(v.activeVoiceCookie, port, dch, dk, dvel);
                    }
                    else
                    {
                        VML("- Move and retrigger non gated voice");
                        vm.responder.moveAndRetriggerVoice(v.activeVoiceCookie, port, dch, dk,
                                                           dvel);
                    }
                    v.port = port;
                    v.channel = dch;
                    v.key = dk;
                }
            }
        }
    }

    bool anyKeyHeldFor(int16_t port, uint64_t polyGroup, int exceptChannel, int exceptKey,
                       bool includeHeldBySustain = false)
    {
        auto &ks = keyStateByPort[port];
        for (int ch = 0; ch < 16; ++ch)
        {
            for (int k = 0; k < 128; ++k)
            {
                auto ksp = ks[ch][k].find(polyGroup);
                if (ksp != ks[ch][k].end() && (includeHeldBySustain || !ksp->second.heldBySustain))
                {
                    if (!(ch == exceptChannel && k == exceptKey))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void debugDumpKeyState(int port) const
    {
        if constexpr (vmLog)
        {
            VML(">>>> Dump Key State " << port);
            auto &ks = keyStateByPort.at(port);
            for (int ch = 0; ch < 16; ++ch)
            {
                for (int k = 0; k < 128; ++k)
                {
                    if (!ks[ch][k].empty())
                    {
                        VML(">>>> - State at " << ch << "/" << k);
                        auto &vmap = ks[ch][k];
                        for (const auto &[pg, it] : vmap)
                        {
                            VML(">>>>   - PG=" << pg);
                            VML(">>>>     " << it.transaction << "/" << it.inceptionVelocity << "/"
                                            << it.heldBySustain);
                        }
                    }
                }
            }
            VML("<<<< Dump Key State");
        }
    }
};

template <typename Cfg, typename Responder, typename MonoResponder>
VoiceManager<Cfg, Responder, MonoResponder>::VoiceManager(Responder &r, MonoResponder &m)
    : responderUnderlyer(r), monoResponderUnderlyer(m), responder(*this), monoResponder(*this),
      details(*this)
{
    static_assert(constraints::ConstraintsChecker<Cfg, Responder, MonoResponder>::satisfies());
    static_assert(
        constraints::ConstraintsChecker<Cfg, responderProxy_t, monoResponderProxy_t>::satisfies());
    registerVoiceEndCallback();
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::registerVoiceEndCallback()
{
    responder.setVoiceEndCallback([this](typename Cfg::voice_t *t) { details.endVoice(t); });
}

template <typename Cfg, typename Responder, typename MonoResponder>
bool VoiceManager<Cfg, Responder, MonoResponder>::processNoteOnEvent(int16_t port, int16_t channel,
                                                                     int16_t key, int32_t noteid,
                                                                     float velocity, float retune)
{
    if (repeatedKeyMode == RepeatedKeyMode::PIANO)
    {
        bool didAnyRetrigger{false};
        ++details.mostRecentTransactionID;
        for (auto &vi : details.voiceInfo)
        {
            if (vi.matches(port, channel, key, -1)) // dont match noteid
            {
                /*
                 * This condition allows voice stacks to occur. See the
                 * 'Stacked Voices' piano mode note id test for instance
                 */
                if (vi.gated && !vi.gatedDueToSustain)
                {
                    continue;
                }

                if (vi.voiceId > 0)
                    responder.discardHostVoice(vi.voiceId);

                responder.retriggerVoiceWithNewNoteID(vi.activeVoiceCookie, noteid, velocity);
                vi.gated = true;
                // We are not gated because of sustain; we are gated because we are gated.
                // If we release we will turn gatedDueToSustain back on
                vi.gatedDueToSustain = false;
                vi.voiceCounter = ++details.mostRecentVoiceCounter;
                vi.transactionId = details.mostRecentTransactionID;
                vi.noteIdStack[vi.noteIdStackPos] = noteid;
                vi.noteIdStackPos =
                    (vi.noteIdStackPos + 1) & (Details::VoiceInfo::noteIdStackSize - 1);

                // if both legs have note ids then only do one voice
                auto hadNoteId = vi.noteId != -1;
                auto hasNoteId = noteid != -1;

                vi.noteId = noteid;
                vi.voiceId = noteid;

                if (hadNoteId && hasNoteId)
                {
                    return true;
                }
                didAnyRetrigger = true;
            }
        }

        if (didAnyRetrigger)
        {
            return true;
        }
    }

    VML("- About to call beginVoiceCreationTransaction");
    auto voicesToBeLaunched = responder.beginVoiceCreationTransaction(
        details.voiceBeginWorkingBuffer, port, channel, key, noteid, velocity);
    VML("- Post begin transaction: voicesToBeLaunched=" << voicesToBeLaunched << " voices");

    if (voicesToBeLaunched == 0)
    {
        responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);
        return true;
    }

    std::unordered_map<uint64_t, int32_t> createdByPolyGroup;
    std::unordered_set<uint64_t> monoGroups;
    for (int i = 0; i < voicesToBeLaunched; ++i)
    {
        assert(details.playMode.find(details.voiceBeginWorkingBuffer[i].polyphonyGroup) !=
               details.playMode.end());

        ++createdByPolyGroup[details.voiceBeginWorkingBuffer[i].polyphonyGroup];
        if (details.playMode[details.voiceBeginWorkingBuffer[i].polyphonyGroup] ==
            PlayMode::MONO_NOTES)
        {
            monoGroups.insert(details.voiceBeginWorkingBuffer[i].polyphonyGroup);
        }
    }

    VML("======== LAUNCHING " << voicesToBeLaunched << " @ " << port << "/" << channel << "/" << key
                              << "/" << noteid << " ============");

    for (int i = 0; i < voicesToBeLaunched; ++i)
    {
        details.voiceInitInstructionsBuffer[i] = {};
        const auto &vbb = details.voiceBeginWorkingBuffer[i];
        auto polyGroup = vbb.polyphonyGroup;
        assert(details.playMode.find(polyGroup) != details.playMode.end());

        auto pm = details.playMode.at(polyGroup);
        if (pm == PlayMode::MONO_NOTES)
            continue;

        assert(details.polyLimits.find(polyGroup) != details.polyLimits.end());

        VML("Poly Stealing:");
        VML("- Voice " << i << " group=" << polyGroup << " mode=" << static_cast<int>(pm));
        VML("- Checking polygroup " << polyGroup);
        int32_t voiceLimit{details.polyLimits.at(polyGroup)};
        int32_t voicesUsed{details.usedVoices.at(polyGroup)};
        int32_t groupFreeVoices = std::max(0, voiceLimit - voicesUsed);

        int32_t globalFreeVoices = Cfg::maxVoiceCount - details.totalUsedVoices;
        int32_t voicesFree = std::min(groupFreeVoices, globalFreeVoices);
        VML("- VoicesFree=" << voicesFree << " toBeCreated=" << createdByPolyGroup.at(polyGroup)
                            << " voiceLimit=" << voiceLimit << " voicesUsed=" << voicesUsed
                            << " groupFreeVoices=" << groupFreeVoices
                            << " globalFreeVoices=" << globalFreeVoices);

        auto voicesToSteal = std::max(createdByPolyGroup.at(polyGroup) - voicesFree, 0);
        auto stealFromPolyGroup{polyGroup};

        VML("- Voices to steal is " << voicesToSteal);
        auto lastVoicesToSteal = voicesToSteal + 1;
        while (voicesToSteal > 0 && voicesToSteal != lastVoicesToSteal)
        {
            lastVoicesToSteal = voicesToSteal;
            auto stealVoiceIndex = details.findNextStealableVoiceInfo(
                polyGroup, details.stealingPriorityMode.at(polyGroup),
                groupFreeVoices > 0 && globalFreeVoices == 0);
            VML("- " << voicesToSteal << " from " << stealFromPolyGroup << " stealing voice "
                     << stealVoiceIndex);
            if (stealVoiceIndex >= 0)
            {
                auto &stealVoice = details.voiceInfo[stealVoiceIndex];
                responder.terminateVoice(stealVoice.activeVoiceCookie);
                VML("  - SkipThis found");
                --voicesToSteal;

                /*
                 * This code makes sure if voices were launched from the same
                 * event they are reaped together
                 */
                for (const auto &v : details.voiceInfo)
                {
                    if (v.activeVoiceCookie &&
                        v.activeVoiceCookie != stealVoice.activeVoiceCookie &&
                        v.transactionId == stealVoice.transactionId)
                    {
                        responder.terminateVoice(v.activeVoiceCookie);
                        --voicesToSteal;
                    }
                }
            }
        }
    }

    // Mono Stealing
    if (!monoGroups.empty())
        VML("Mono Stealing:");
    for (const auto &mpg : monoGroups)
    {
        VML("- Would steal all voices in " << mpg);
        auto isLegato = details.playModeFeatures.at(mpg) &
                        static_cast<uint64_t>(MonoPlayModeFeatures::MONO_LEGATO);
        VML("- IsLegato : " << isLegato);
        if (isLegato)
        {
            bool foundOne{false};
            for (auto &v : details.voiceInfo)
            {
                if (v.activeVoiceCookie && v.polyGroup == mpg)
                {
                    VML("  - Moving existing voice " << &v << " " << v.activeVoiceCookie << " to "
                                                     << key << " (" << v.gated << ")");
                    if (v.gated)
                    {
                        responder.moveVoice(v.activeVoiceCookie, port, channel, key, velocity);
                        v.noteIdStack[v.noteIdStackPos] = noteid;
                        v.noteIdStackPos =
                            (v.noteIdStackPos + 1) & (Details::VoiceInfo::noteIdStackSize - 1);
                    }
                    else
                    {
                        responder.moveAndRetriggerVoice(v.activeVoiceCookie, port, channel, key,
                                                        velocity);
                    }
                    responder.discardHostVoice(noteid);
                    v.port = port;
                    v.channel = channel;
                    v.key = key;
                    v.gated = true;

                    foundOne = true;
                }
            }
            if (foundOne)
            {
                for (int i = 0; i < voicesToBeLaunched; ++i)
                {
                    if (details.voiceBeginWorkingBuffer[i].polyphonyGroup == mpg)
                    {
                        VML("  - Setting instruction " << i << " to skip " << mpg);
                        details.voiceInitInstructionsBuffer[i].instruction =
                            VoiceInitInstructionsEntry<Cfg>::Instruction::SKIP;
                    }
                }
            }
        }
        else
        {
            for (const auto &v : details.voiceInfo)
            {
                if (v.activeVoiceCookie && v.polyGroup == mpg)
                {
                    VML("- Stealing voice " << v.key);
                    responder.terminateVoice(v.activeVoiceCookie);
                }
            }
        }
    }

    if (details.lastPBByChannel[channel] != 0)
    {
        monoResponder.setMIDIPitchBend(channel, details.lastPBByChannel[channel] + 8192);
    }

    int cid{0};
    for (auto &mcc : details.midiCCCache[channel])
    {
        if (mcc != 0)
        {
            monoResponder.setMIDI1CC(channel, cid, mcc);
        }
        cid++;
    }

    if constexpr (vmLog)
    {
        VML("- Instruction Buffer (size " << voicesToBeLaunched << ")");
        for (int i = 0; i < voicesToBeLaunched; ++i)
        {
            VML("  - i=" << i << " inst="
                         << static_cast<int>(details.voiceInitInstructionsBuffer[i].instruction)
                         << " pg=" << details.voiceBeginWorkingBuffer[i].polyphonyGroup);
        }
    }
    auto voicesLaunched = responder.initializeMultipleVoices(
        voicesToBeLaunched, details.voiceInitInstructionsBuffer, details.voiceInitWorkingBuffer,
        port, channel, key, noteid, velocity, retune);

    VML("- Voices created " << voicesLaunched);
    details.debugDumpKeyState(port);

    if (voicesLaunched != voicesToBeLaunched)
    {
        // This is probably OK
    }

    ++details.mostRecentTransactionID;
    if (voicesLaunched == 0)
    {
        for (int i = 0; i < voicesToBeLaunched; ++i)
        {
            // bail but still record the key press
            details.keyStateByPort[port][channel][key]
                                  [details.voiceBeginWorkingBuffer[i].polyphonyGroup] = {
                details.mostRecentTransactionID, velocity};
        }

        responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

        return false;
    }

    auto voicesLeft = voicesLaunched;
    VML("- VoicesToBeLaunced " << voicesToBeLaunched << " voices launched " << voicesLaunched);

    auto placeVoiceFromIndex = [&](int index)
    {
        for (auto &vi : details.voiceInfo)
        {
            if (!vi.activeVoiceCookie)
            {
                vi.voiceCounter = details.mostRecentVoiceCounter++;
                vi.transactionId = details.mostRecentTransactionID;
                vi.port = port;
                vi.channel = channel;
                vi.key = key;
                vi.noteId = noteid;
                vi.snapOriginalToCurrent();

                vi.gated = true;
                vi.gatedDueToSustain = false;
                vi.activeVoiceCookie = details.voiceInitWorkingBuffer[index].voice;
                vi.polyGroup = details.voiceBeginWorkingBuffer[index].polyphonyGroup;
                vi.noteIdStackPos = 1;
                vi.noteIdStack[0] = noteid;
                vi.voiceId = noteid;

                VML("- New Voice assigned from "
                    << index << " with " << details.mostRecentVoiceCounter << " at pckn=" << port
                    << "/" << channel << "/" << key << "/" << noteid << " pg=" << vi.polyGroup
                    << " avc=" << vi.activeVoiceCookie);

                assert(details.usedVoices.find(vi.polyGroup) != details.usedVoices.end());
                ++details.usedVoices.at(vi.polyGroup);
                ++details.totalUsedVoices;
                return true;
            }
        }
        return false;
    };

    for (int i = 0; i < voicesToBeLaunched; ++i)
    {
        details.keyStateByPort[port][channel][key]
                              [details.voiceBeginWorkingBuffer[i].polyphonyGroup] = {
            details.mostRecentTransactionID, velocity};

        if (details.voiceInitInstructionsBuffer[i].instruction !=
                VoiceInitInstructionsEntry<Cfg>::Instruction::SKIP &&
            details.voiceInitWorkingBuffer[i].voice)
        {
            placeVoiceFromIndex(i);
            --voicesLeft;
        }
    }

    // assert(voicesLeft == 0);

    responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

    return false;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::processNoteOffEvent(int16_t port, int16_t channel,
                                                                      int16_t key, int32_t noteid,
                                                                      float velocity)
{
    std::unordered_set<uint64_t> retriggerGroups;

    VML("==== PROCESS NOTE OFF " << port << "/" << channel << "/" << key << "/" << noteid << " @ "
                                 << velocity);
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key, noteid))
        {
            VML("- Found matching release note at " << vi.polyGroup << " " << vi.key << " "
                                                    << vi.gated);
            if (details.playMode[vi.polyGroup] == PlayMode::MONO_NOTES)
            {
                if (details.playModeFeatures[vi.polyGroup] &
                    static_cast<uint64_t>(MonoPlayModeFeatures::MONO_LEGATO))
                {
                    bool anyOtherOption = details.anyKeyHeldFor(port, vi.polyGroup, channel, key);
                    VML("- AnoyOther check for legato at "
                        << vi.polyGroup << " " << static_cast<int>(channel) << " "
                        << static_cast<int>(key) << " is " << anyOtherOption);
                    if (anyOtherOption)
                    {
                        retriggerGroups.insert(vi.polyGroup);
                        VML("- A key is down in same group. Initiating mono legato move");
                        continue;
                    }
                    else
                    {
                        auto &ks = details.keyStateByPort[port];
                        for (int ch = 0; ch < 16; ++ch)
                        {
                            for (int k = 0; k < 128; ++k)
                            {
                                for (const auto &[g, s] : ks[ch][k])
                                {
                                    VML("=== Held Key " << g << " " << s.transaction
                                                        << " at k=" << (int)k << " ch=" << (int)ch)
                                }
                            }
                        }
                    }
                }
                auto susCh = dialect == MIDI1Dialect::MIDI1_MPE ? 0 : channel;
                if (details.sustainOn[susCh])
                {
                    VML("- Release with sustain on. Checking to see if there are gated voices "
                        "away");

                    details.debugDumpKeyState(port);
                    if (details.anyKeyHeldFor(port, vi.polyGroup, channel, key))
                    {
                        VML("- There's a gated key away so untrigger this");
                        retriggerGroups.insert(vi.polyGroup);
                        responder.terminateVoice(vi.activeVoiceCookie);
                        VML("- Gated to False ***");
                        vi.gated = false;
                    }
                    else
                    {
                        vi.gatedDueToSustain = true;
                    }
                }
                else
                {
                    if (vi.gated)
                    {
                        bool anyOtherOption =
                            details.anyKeyHeldFor(port, vi.polyGroup, channel, key);
                        if (anyOtherOption)
                        {
                            VML("- Hard Terminate voice with other away " << vi.polyGroup << " "
                                                                          << vi.activeVoiceCookie);
                            responder.terminateVoice(vi.activeVoiceCookie);
                            retriggerGroups.insert(vi.polyGroup);
                        }
                        else
                        {
                            VML("- Release voice completely (no other)" << vi.polyGroup << " "
                                                                        << vi.activeVoiceCookie);
                            responder.releaseVoice(vi.activeVoiceCookie, velocity);
                        }
                        VML("- Gated to False ***");
                        vi.gated = false;
                    }
                }
            }
            else
            {
                // Poly branch ere
                auto susCh = dialect == MIDI1Dialect::MIDI1_MPE ? 0 : channel;
                if (details.sustainOn[susCh])
                {
                    vi.gatedDueToSustain = true;
                }
                else
                {
                    if (vi.gated)
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, velocity);
                        VML("- Gated to False ***");
                        vi.gated = false;
                    }
                }
            }
        }
    }

    auto susCh = dialect == MIDI1Dialect::MIDI1_MPE ? 0 : channel;
    if (details.sustainOn[susCh])
    {
        VML("- Updating just-by-sustain at " << port << " " << channel << " " << key);
        for (auto &inf : details.keyStateByPort[port][channel][key])
        {
            inf.second.heldBySustain = true;
        }
    }
    else
    {
        VML("-  Clearing keyStateByPort at " << port << " " << channel << " " << key);
        details.keyStateByPort[port][channel][key] = {};
    }

    details.debugDumpKeyState(port);

    for (const auto &rtg : retriggerGroups)
    {
        VML("- Retriggering mono group " << rtg);
        details.doMonoRetrigger(port, rtg);
        if (noteid >= 0)
        {
            for (auto &vi : details.voiceInfo)
            {
                if (vi.polyGroup == rtg && vi.activeVoiceCookie)
                {
                    vi.removeNoteIdFromStack(noteid);
                }
            }
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::updateSustainPedal(int16_t port, int16_t channel,
                                                                     int8_t level)
{
    auto sop = details.sustainOn[channel];
    details.sustainOn[channel] = level > 64;
    if (sop != details.sustainOn[channel])
    {
        if (!details.sustainOn[channel])
        {
            VML("Sustain Release");
            auto channelMatch = dialect == MIDI1Dialect::MIDI1_MPE ? -1 : channel;
            std::unordered_set<uint64_t> retriggerGroups;
            // release all voices with sustain gates
            for (auto &vi : details.voiceInfo)
            {
                if (!vi.activeVoiceCookie)
                    continue;

                VML("- Checking " << vi.gated << " " << vi.gatedDueToSustain << " " << vi.key);
                if (vi.gatedDueToSustain && vi.matches(port, channelMatch, -1, -1))
                {
                    if (details.playMode[vi.polyGroup] == PlayMode::MONO_NOTES)
                    {
                        retriggerGroups.insert(vi.polyGroup);
                        responder.releaseVoice(vi.activeVoiceCookie, 0);
                    }
                    else
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, 0);
                    }

                    details.keyStateByPort[vi.port][vi.channel][vi.key] = {};

                    VML("- Gated to False ***");
                    vi.gated = false;
                    vi.gatedDueToSustain = false;
                }
            }
            for (const auto &rtg : retriggerGroups)
            {
                auto &ks = details.keyStateByPort[port];
                for (int ch = 0; ch < 16; ++ch)
                {
                    for (int k = 0; k < 128; ++k)
                    {
                        auto ksp = ks[ch][k].find(rtg);
                        if (ksp != ks[ch][k].end() && ksp->second.heldBySustain)
                        {
                            ks[ch][k].erase(ksp);
                        }
                    }
                }

                details.doMonoRetrigger(port, rtg);
            }
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeMIDIPitchBend(int16_t port, int16_t channel,
                                                                     int16_t pb14bit)
{
    if (dialect == MIDI1Dialect::MIDI1)
    {
        details.doMonoPitchBend(port, channel, pb14bit);
    }
    else if (dialect == MIDI1Dialect::MIDI1_MPE)
    {
        if (channel == mpeGlobalChannel)
        {
            details.doMonoPitchBend(port, -1, pb14bit);
        }
        else
        {
            details.doMPEPitchBend(port, channel, pb14bit);
        }
    }
    else
    {
        // Code this dialect! What is it even?
        assert(false);
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
size_t VoiceManager<Cfg, Responder, MonoResponder>::getVoiceCount() const
{
    size_t res{0};
    for (const auto &vi : details.voiceInfo)
    {
        res += (vi.activeVoiceCookie != nullptr);
    }
    return res;
}

template <typename Cfg, typename Responder, typename MonoResponder>
size_t VoiceManager<Cfg, Responder, MonoResponder>::getGatedVoiceCount() const
{
    size_t res{0};
    for (const auto &vi : details.voiceInfo)
    {
        res += (vi.activeVoiceCookie != nullptr && vi.gated) ? 1 : 0;
    }
    return res;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeNoteExpression(int16_t port, int16_t channel,
                                                                      int16_t key, int32_t noteid,
                                                                      int32_t expression,
                                                                      double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key,
                       noteid)) // all keys and notes on a channel for midi PB
        {
            responder.setNoteExpression(vi.activeVoiceCookie, expression, value);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routePolyphonicParameterModulation(
    int16_t port, int16_t channel, int16_t key, int32_t voiceid, uint32_t parameter, double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matchesVoiceId(voiceid))
        {
            responder.setVoicePolyphonicParameterModulation(vi.activeVoiceCookie, parameter, value);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeMonophonicParameterModulation(
    int16_t port, int16_t channel, int16_t key, uint32_t parameter, double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.activeVoiceCookie)
        {
            responder.setVoiceMonophonicParameterModulation(vi.activeVoiceCookie, parameter, value);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routePolyphonicAftertouch(int16_t port,
                                                                            int16_t channel,
                                                                            int16_t key, int8_t pat)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key, -1)) // all keys and notes on a channel for midi PB
        {
            responder.setPolyphonicAftertouch(vi.activeVoiceCookie, pat);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeChannelPressure(int16_t port,
                                                                       int16_t channel, int8_t pat)
{
    if (dialect == MIDI1Dialect::MIDI1)
    {
        details.doMonoChannelPressure(port, channel, pat);
    }
    else if (dialect == MIDI1Dialect::MIDI1_MPE)
    {
        if (channel == mpeGlobalChannel)
        {
            details.doMonoChannelPressure(port, channel, pat);
        }
        else
        {
            details.doMPEChannelPressure(port, channel, pat);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeMIDI1CC(int16_t port, int16_t channel,
                                                               int8_t cc, int8_t val)
{
    if (dialect == MIDI1Dialect::MIDI1_MPE && channel != mpeGlobalChannel && cc == mpeTimbreCC)
    {
        for (auto &vi : details.voiceInfo)
        {
            if (vi.activeVoiceCookie && vi.port == port && vi.channel == channel && vi.gated)
            {
                responder.setVoiceMIDIMPETimbre(vi.activeVoiceCookie, val);
            }
        }
    }
    else
    {
        details.midiCCCache[channel][cc] = val;
        monoResponder.setMIDI1CC(channel, cc, val);
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::allSoundsOff()
{
    for (const auto &v : details.voiceInfo)
    {
        if (v.activeVoiceCookie)
        {
            responder.terminateVoice(v.activeVoiceCookie);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::allNotesOff()
{
    for (auto &v : details.voiceInfo)
    {
        if (v.activeVoiceCookie)
        {
            responder.releaseVoice(v.activeVoiceCookie, 0);
            VML("- Gated to False ***");
            v.gated = false;
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setPolyphonyGroupVoiceLimit(uint64_t groupId,
                                                                              int32_t limit)
{
    details.guaranteeGroup(groupId);
    details.polyLimits[groupId] = limit;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setPlaymode(uint64_t groupId, PlayMode pm,
                                                              uint64_t features)
{
    details.guaranteeGroup(groupId);
    details.playMode[groupId] = pm;
    details.playModeFeatures[groupId] = features;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setStealingPriorityMode(uint64_t groupId,
                                                                          StealingPriorityMode pm)
{
    details.guaranteeGroup(groupId);
    details.stealingPriorityMode[groupId] = pm;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::guaranteeGroup(uint64_t groupId)
{
    details.guaranteeGroup(groupId);
}
} // namespace sst::voicemanager
#endif // INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_IMPL_H
