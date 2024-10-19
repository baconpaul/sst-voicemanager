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

#ifndef SST_VOICEMANAGER_TESTS_TEST_PLAYER_H
#define SST_VOICEMANAGER_TESTS_TEST_PLAYER_H

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <functional>
#include <set>
#include <array>
#include <vector>
#include <tuple>
#include <sstream>
#include <map>

#include "sst/voicemanager/voicemanager.h"

#define TPT(...)                                                                                   \
    if constexpr (doLog)                                                                           \
    {                                                                                              \
        std::cout << "tests/test_player.h:" << __LINE__ << " " << __VA_ARGS__ << std::endl;        \
    }
#define TPD(x) #x << "=" << (x) << " "
#define TPF TPT(__func__)
#define TPUN TPT(__func__ << " - unimplemented");

/**
 * The TestPlayer class provides an implementation of a voice manager target wihch has an internal
 * voice array that it 'plays' by just updating state and providing debug apis. This allows us to
 * use it to write regtests of various features with lots of input and output set.
 *
 * This test player has the feature that it makes one voice for a note in range 0..72 and 3 voices
 * per note over 72. A released voice fades over 5 calls to process then terminates
 */
template <size_t voiceCount, bool doLog = false> struct TestPlayer
{
    using pckn_t = std::tuple<int16_t, int16_t, int16_t, int32_t>;
    struct Voice
    {
        enum State
        {
            UNUSED,
            ACTIVE
        } state{UNUSED};
        int32_t runtime{0};
        bool isGated{false};
        int32_t releaseCountdown{0};

        float velocity, releaseVelocity;

        pckn_t pckn{-1, -1, -1, -1};

        int8_t polyATValue{0};

        auto port() const { return std::get<0>(pckn); }
        auto channel() const { return std::get<1>(pckn); }
        auto key() const { return std::get<2>(pckn); }
        auto noteid() const { return std::get<3>(pckn); }

        // Note these make the voices non-fixed size so in a real synth you wouldn't want this
        std::map<int32_t, double> noteExpressionCache;
        std::map<int32_t, double> paramModulationCache;

        int16_t mpeBend{0};
        int8_t mpePressure{0}, mpeTimbre{0};
    };

    struct Config
    {
        using voice_t = Voice;
        static constexpr size_t maxVoiceCount{voiceCount};
    };

    std::array<Voice, voiceCount> voiceStorage;
    std::function<void(Voice *)> voiceEndCallback{nullptr};

    // voice manager responder
    struct Responder
    {
        TestPlayer &testPlayer;
        Responder(TestPlayer &p) : testPlayer(p) {}

        void setVoiceEndCallback(std::function<void(Voice *)> f)
        {
            testPlayer.voiceEndCallback = f;
        }

        int32_t initializeMultipleVoices(
            std::array<typename Config::voice_t *, Config::maxVoiceCount> &voiceInitWorkingBuffer,
            uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity,
            float retune)
        {
            TPF;
            Voice *nv[3]{nullptr, nullptr, nullptr};
            int idx{0};
            int midx{key <= 72 ? 1 : 3};

            for (auto &v : testPlayer.voiceStorage)
            {
                if (v.state != Voice::ACTIVE)
                {
                    nv[idx] = &v;
                    ++idx;
                    if (idx == midx)
                        break;
                }
            }

            if (idx != midx)
                return 0;

            for (int i = 0; i < midx; ++i)
            {
                auto v = nv[i];
                if (!v)
                    continue;
                v->state = Voice::ACTIVE;
                v->runtime = 0;
                v->isGated = true;
                v->pckn = {port, channel, key, noteId};
                v->velocity = velocity;

                voiceInitWorkingBuffer[i] = v;
            }

            TPT("Created " << midx << " voices");
            return midx;
        }

        void terminateVoice(Voice *v)
        {
            TPT("Terminate voice at " << TPD(std::get<2>(v->pckn)));
            testPlayer.voiceEndCallback(v);
            *v = Voice();
        }
        void releaseVoice(Voice *v, float velocity)
        {
            TPF;
            v->isGated = false;
            v->releaseCountdown = 5;
            v->releaseVelocity = velocity;
        }
        void retriggerVoiceWithNewNoteID(Voice *v, int32_t noteid, float velocity)
        {
            TPF;
            v->isGated = true;
            v->releaseCountdown = 0;
            v->velocity = velocity;
        }

        int beginVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key,
                                          int32_t noteid, float velocity)
        {
            TPF;
            if (key <= 72)
                return 1;
            return 3;
        }
        void endVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key,
                                         int32_t noteid, float velocity)
        {
            TPF;
        }

        void setNoteExpression(Voice *v, int32_t e, double val)
        {
            TPF;
            v->noteExpressionCache[e] = val;
        }
        void setVoicePolyphonicParameterModulation(Voice *v, uint32_t e, double val)
        {
            TPF;
            v->paramModulationCache[e] = val;
        }
        void setPolyphonicAftertouch(Voice *v, int8_t val)
        {
            TPF;
            v->polyATValue = val;
        }
        void setVoiceMIDIMPEChannelPitchBend(Voice *v, uint16_t b)
        {
            TPF;
            v->mpeBend = b;
        }
        void setVoiceMIDIMPEChannelPressure(Voice *v, int8_t p)
        {
            TPF;
            v->mpePressure = p;
        }
        void setVoiceMIDIMPETimbre(Voice *v, int8_t t)
        {
            TPF;
            v->mpeTimbre = t;
        }

    } responder;

    struct MonoResponder
    {
        TestPlayer &testPlayer;
        MonoResponder(TestPlayer &p) : testPlayer(p) {}
        void setMIDIPitchBend(int16_t channel, int16_t pb14bit)
        {
            testPlayer.pitchBend[channel] = pb14bit;
        }
        void setMIDI1CC(int16_t channel, int16_t cc, int8_t val)
        {
            testPlayer.midi1CC[channel][cc] = val;
        }
        void setMIDIChannelPressure(int16_t channel, int16_t pres)
        {
            testPlayer.channelPressure[channel] = pres;
        }
    } monoResponder;

    std::array<int16_t, 16> channelPressure{}, pitchBend{};
    std::array<std::array<int8_t, 128>, 16> midi1CC{};

    using voiceManager_t = sst::voicemanager::VoiceManager<Config, Responder, MonoResponder>;
    voiceManager_t voiceManager;

    void processFor(size_t times)
    {
        for (auto i = 0U; i < times; ++i)
            process();
    }
    void process()
    {
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::State::ACTIVE)
            {
                ++v.runtime;
                if (!v.isGated)
                {
                    --v.releaseCountdown;
                    if (v.releaseCountdown == 0)
                    {
                        if (voiceEndCallback)
                        {
                            voiceEndCallback(&v);
                        }
                        v.state = Voice::State::UNUSED;
                    }
                }
            }
        }
    }

    TestPlayer() : responder(*this), monoResponder(*this), voiceManager(responder, monoResponder)
    {
        TPT("Constructed TestPlayer with " << TPD(voiceCount));
    }

    /*
     * This test player has a large collection of debug apis to let us probe the internal
     * state and write regtests. These APIs are below, and are not part of what most voice
     * manager users would use
     */

    std::vector<pckn_t> getGatedVoicePCKNS() const
    {
        auto res = std::vector<pckn_t>();
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && v.isGated)
                res.push_back(v.pckn);
        }
        return res;
    }

    std::vector<pckn_t> getActiveVoicePCKNS() const
    {
        auto res = std::vector<pckn_t>();
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE)
                res.push_back(v.pckn);
        }
        return res;
    }

    std::string pcknToString(const pckn_t &v)
    {
        std::ostringstream oss;
        auto [p, c, k, n] = v;
        oss << "p=" << p << ",c=" << c << ",k=" << k << ",n=" << n;
        return oss.str();
    }
    std::string voiceToString(const Voice &v)
    {
        std::ostringstream oss;
        oss << "Voice[";
        if (v.state == Voice::UNUSED)
        {
            oss << "Unused";
        }
        else
        {
            oss << "rt=" << v.runtime << ",gate=" << v.isGated << ",rc=" << v.releaseCountdown
                << "," << pcknToString(v.pckn) << ",mpeBend=" << v.mpeBend
                << ",mpePres=" << (int)v.mpePressure << ",mpeTim=" << (int)v.mpeTimbre;
        }
        oss << "]";
        return oss.str();
    }

    void dumpAllVoices(bool includeUnused = false)
    {
        TPT("Dump all voices " << (includeUnused ? " including unused" : ""));
        for (const auto &v : voiceStorage)
            if (includeUnused || v.state == Voice::ACTIVE)
                TPT(voiceToString(v));
        TPT("Voice dump complete");
    }

    bool hasKeysActive(const std::set<int16_t> &keySet)
    {
        for (auto &k : keySet)
        {
            auto found = false;
            for (auto &v : voiceStorage)
            {
                found = found || (v.state == Voice::ACTIVE && std::get<2>(v.pckn) == k);
            }
            if (!found)
                return false;
        }
        return true;
    }

    int32_t activeVoicesMatching(std::function<bool(const Voice &)> cond)
    {
        int32_t res = 0;
        for (const auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && cond(v))
            {
                ++res;
            }
        }
        return res;
    }

    bool activeVoiceCheck(std::function<bool(const Voice &)> voiceFilter,
                          std::function<bool(const Voice &)> condition)
    {
        auto res = true;
        auto checkedAny = false;
        for (const auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && voiceFilter(v))
            {
                checkedAny = true;
                res = res & condition(v);
            }
        }
        return res && checkedAny;
    }
};

#undef TPT
#undef TPF
#undef TPD
#undef TPUN

// Some useful test macros
#define REQUIRE_VOICE_COUNTS(count, gatedCount)                                                    \
    REQUIRE(vm.getVoiceCount() == (size_t)(count));                                                \
    REQUIRE(vm.getGatedVoiceCount() == (size_t)(gatedCount));                                      \
    REQUIRE(tp.getActiveVoicePCKNS().size() == (size_t)(count));                                   \
    REQUIRE(tp.getGatedVoicePCKNS().size() == (size_t)(gatedCount));

#define REQUIRE_NO_VOICES                                                                          \
    REQUIRE(vm.getVoiceCount() == 0);                                                              \
    REQUIRE(vm.getGatedVoiceCount() == 0);                                                         \
    REQUIRE(tp.getActiveVoicePCKNS().empty());                                                     \
    REQUIRE(tp.getGatedVoicePCKNS().empty());

#define REQUIRE_INCOMPLETE_TEST REQUIRE(false)

#endif // TEST_RESPONDERS_H
