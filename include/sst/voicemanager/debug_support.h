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

#ifndef INCLUDE_SST_VOICEMANAGER_DEBUG_SUPPORT_H
#define INCLUDE_SST_VOICEMANAGER_DEBUG_SUPPORT_H

#include <iostream>
namespace sst::voicemanager::details
{
struct DebugSupport
{
    template <typename... Args> void logFromVM(Args... args) { logInternal(" ->", args...); }

    template <typename... Args> void logVMAPI(Args... args) { logInternal("|=> ", args...); }

    template <typename... Args> void log(Args... args) { logInternal(args...); }

    template <typename T, typename... Rest> void logInternal(T t, Rest... rest)
    {
        std::cout << t << " ";
        logInternal(rest...);
    }

    void logInternal() { std::cout << "" << std::endl; }
};
}; // namespace sst::voicemanager::details

#endif // DEBUG_SUPPORT_H
