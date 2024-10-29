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
            std::cout << "include/sst/voicemanager/voicemanager_impl.h:" << __LINE__ << " "        \
                      << __VA_ARGS__ << std::endl;                                                 \
        }                                                                                          \
    }

template <typename Cfg, typename Responder, typename MonoResponder>
struct VoiceManager<Cfg, Responder, MonoResponder>::Details
{
    VoiceManager<Cfg, Responder, MonoResponder> &vm;
    Details(VoiceManager<Cfg, Responder, MonoResponder> &in) : vm(in)
    {
        std::fill(lastPBByChannel.begin(), lastPBByChannel.end(), 0);

        keyStateByPort[0] = {};
        guaranteeGroup(0);
    }

    int64_t mostRecentVoiceCounter{1};
    int64_t mostRecentTransactionID{1};

    struct VoiceInfo
    {
        int16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1};

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
            res = res && (nid == -1 || noteId == -1 || nid == noteId);
            return res;
        }
    };
    std::array<VoiceInfo, Cfg::maxVoiceCount> voiceInfo;
    std::unordered_map<uint64_t, int32_t> polyLimits;
    std::unordered_map<uint64_t, int32_t> usedVoices;
    std::unordered_map<uint64_t, StealingPriorityMode> stealingPriorityMode;
    std::unordered_map<uint64_t, PlayMode> playMode;
    std::unordered_map<uint64_t, uint64_t> playModeFeatures;
    int32_t totalUsedVoices{0};

    struct IndividualKeyState
    {
        int64_t transaction{0};
        float inceptionVelocity{0.f};
        bool heldBySustain{false};
    };
    using keyState_t = std::array<std::array<std::map<uint64_t, IndividualKeyState>, 128>, 16>;
    std::map<int32_t, keyState_t> keyStateByPort{};

    void guaranteeGroup(int groupId)
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
            playModeFeatures[groupId] = (uint64_t)MonoPlayModeFeatures::NONE;
    }

    typename VoiceBeginBufferEntry<Cfg>::buffer_t voiceBeginWorkingBuffer;
    typename VoiceInitBufferEntry<Cfg>::buffer_t voiceInitWorkingBuffer;
    typename VoiceInitInstructionsEntry<Cfg>::buffer_t voiceInitInstructionsBuffer;
    std::array<std::array<uint16_t, 128>, 16> midiCCCache{};
    bool sustainOn{false};
    std::array<int16_t, 16> lastPBByChannel{};

    void doMonoPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        if (channel >= 0 && channel < (int16_t)lastPBByChannel.size())
            lastPBByChannel[channel] = pb14bit - 8192;

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

        for (int32_t vi = 0; vi < (int32_t)voiceInfo.size(); ++vi)
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
        VML("=== MONO mode voice retrigger for " << polyGroup);
        auto &ks = keyStateByPort[port];
        auto ft = playModeFeatures.at(polyGroup);
        int dch{-1}, dk{-1};
        float dvel{0.f};

        auto findBestKey = [&](bool ignoreSustain)
        {
            VML("- find best key " << ignoreSustain);
            if (ft & (uint64_t)MonoPlayModeFeatures::ON_RELEASE_TO_LATEST)
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
            else if (ft & (uint64_t)MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST)
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
            else if (ft & (uint64_t)MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST)
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

        if (dch >= 0 && dk >= 0)
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

                    voicesLeft--;
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
            VML(">>>> Dump Key State");
            auto &ks = keyStateByPort.at(port);
            for (int ch = 0; ch < 16; ++ch)
            {
                for (int k = 0; k < 128; ++k)
                {
                    if (!ks[ch][k].empty())
                    {
                        VML("- State at " << ch << "/" << k);
                        auto &vmap = ks[ch][k];
                        for (const auto &[pg, it] : vmap)
                        {
                            VML("   - PG=" << pg);
                            VML("     " << it.transaction << "/" << it.inceptionVelocity << "/"
                                        << it.heldBySustain);
                        }
                    }
                }
            }
        }
    }
};

template <typename Cfg, typename Responder, typename MonoResponder>
VoiceManager<Cfg, Responder, MonoResponder>::VoiceManager(Responder &r, MonoResponder &m)
    : responder(r), monoResponder(m), details(*this)
{
    static_assert(constraints::ConstraintsChecker<Cfg, Responder, MonoResponder>::satisfies());
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
                responder.retriggerVoiceWithNewNoteID(vi.activeVoiceCookie, noteid, velocity);
                vi.gated = true;
                vi.voiceCounter = ++details.mostRecentVoiceCounter;
                vi.transactionId = details.mostRecentTransactionID;
                didAnyRetrigger = true;
            }
        }

        if (didAnyRetrigger)
        {
            return true;
        }
    }

    auto voicesToBeLaunched = responder.beginVoiceCreationTransaction(
        details.voiceBeginWorkingBuffer, port, channel, key, noteid, velocity);

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
        const auto &vbb = details.voiceBeginWorkingBuffer[i];
        auto polyGroup = vbb.polyphonyGroup;
        assert(details.polyLimits.find(polyGroup) != details.polyLimits.end());
        assert(details.playMode.find(polyGroup) != details.playMode.end());

        auto pm = details.playMode.at(polyGroup);
        if (pm == PlayMode::MONO_NOTES)
            continue;

        VML("Poly Stealing:");
        VML("- Voice " << i << " group=" << polyGroup << " mode=" << (int)pm);
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
        VML("- Would steal all voices in " << mpg << " (TODO: This is *WRONG* for Legato)");
        for (const auto &v : details.voiceInfo)
        {
            if (v.activeVoiceCookie && v.polyGroup == mpg)
            {
                VML("- Stealing voice " << v.key);
                responder.terminateVoice(v.activeVoiceCookie);
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

    auto voicesLaunched = responder.initializeMultipleVoices(
        voicesToBeLaunched, details.voiceInitInstructionsBuffer, details.voiceInitWorkingBuffer,
        port, channel, key, noteid, velocity, retune);

    VML("- Voices created " << voicesLaunched);

    if (voicesLaunched != voicesToBeLaunched)
    {
        // This is probably OK
    }

    if (voicesLaunched == 0)
    {
        responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

        return false;
    }

    auto voicesLeft = voicesLaunched;
    ++details.mostRecentTransactionID;

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

            vi.gated = true;
            vi.gatedDueToSustain = false;
            vi.activeVoiceCookie = details.voiceInitWorkingBuffer[voicesLeft - 1].voice;
            vi.polyGroup = details.voiceBeginWorkingBuffer[voicesLeft - 1].polyphonyGroup;

            details.keyStateByPort[vi.port][vi.channel][vi.key][vi.polyGroup] = {vi.transactionId,
                                                                                 velocity};

            VML("- New Voice assigned with " << details.mostRecentVoiceCounter
                                             << " at pckn=" << port << "/" << channel << "/" << key
                                             << "/" << noteid << " pg=" << vi.polyGroup);

            ++details.usedVoices.at(vi.polyGroup);
            ++details.totalUsedVoices;

            voicesLeft--;
            if (voicesLeft == 0)
            {
                responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

                details.debugDumpKeyState(port);
                return true;
            }
        }
    }

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
            VML("- Found matching release note at " << vi.polyGroup << " " << vi.key);
            if (details.playMode[vi.polyGroup] == PlayMode::MONO_NOTES)
            {
                if (details.sustainOn)
                {
                    VML("- Release with sustain on. Checking to see if there are gated voices "
                        "away");

                    details.debugDumpKeyState(port);
                    bool anyOtherOption = details.anyKeyHeldFor(port, vi.polyGroup, channel, key);
                    if (anyOtherOption)
                    {
                        VML("- There's a gated key away so untrigger this");
                        retriggerGroups.insert(vi.polyGroup);
                        responder.terminateVoice(vi.activeVoiceCookie);
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
                        VML("- Terminating voice at " << vi.polyGroup << " "
                                                      << vi.activeVoiceCookie);
                        bool anyOtherOption =
                            details.anyKeyHeldFor(port, vi.polyGroup, channel, key);
                        if (anyOtherOption)
                        {
                            responder.terminateVoice(vi.activeVoiceCookie);
                            retriggerGroups.insert(vi.polyGroup);
                        }
                        else
                        {
                            responder.releaseVoice(vi.activeVoiceCookie, velocity);
                        }
                        vi.gated = false;
                    }
                }
            }
            else
            {
                if (details.sustainOn)
                {
                    vi.gatedDueToSustain = true;
                }
                else
                {
                    if (vi.gated)
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, velocity);
                        vi.gated = false;
                    }
                }
            }
        }
    }

    if (details.sustainOn)
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
        details.doMonoRetrigger(port, rtg);
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::updateSustainPedal(int16_t port, int16_t channel,
                                                                     int8_t level)
{
    auto sop = details.sustainOn;
    details.sustainOn = level > 64;
    if (sop != details.sustainOn)
    {
        if (!details.sustainOn)
        {
            VML("Sustain Release");
            std::unordered_set<uint64_t> retriggerGroups;
            // release all voices with sustain gates
            for (auto &vi : details.voiceInfo)
            {
                if (!vi.activeVoiceCookie)
                    continue;

                VML("- Checking " << vi.gated << " " << vi.gatedDueToSustain << " " << vi.key);
                if (vi.gatedDueToSustain && vi.matches(port, channel, -1, -1))
                {
                    if (details.playMode[vi.polyGroup] == PlayMode::MONO_NOTES)
                    {
                        retriggerGroups.insert(vi.polyGroup);
                        responder.terminateVoice(vi.activeVoiceCookie);
                    }
                    else
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, 0);
                    }

                    details.keyStateByPort[vi.port][vi.channel][vi.key] = {};

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
    int16_t port, int16_t channel, int16_t key, int32_t noteid, uint32_t parameter, double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key,
                       noteid)) // all keys and notes on a channel for midi PB
        {
            responder.setVoicePolyphonicParameterModulation(vi.activeVoiceCookie, parameter, value);
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
#endif // VOICEMANAGER_IMPL_H