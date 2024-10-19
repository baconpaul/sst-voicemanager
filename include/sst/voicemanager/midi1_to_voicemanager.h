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

#ifndef INCLUDE_SST_VOICEMANAGER_MIDI1_TO_VOICEMANAGER_H
#define INCLUDE_SST_VOICEMANAGER_MIDI1_TO_VOICEMANAGER_H

#include <cassert>

namespace sst::voicemanager
{
template <typename Manager>
void applyMidi1Message(Manager &voiceManager, int16_t port_index, const uint8_t data[3])
{
    auto msg = data[0] & 0xF0;
    auto chan = data[0] & 0x0F;
    switch (msg)
    {
    case 0x90:
    {
        if (data[2] == 0)
        {
            voiceManager.processNoteOffEvent(port_index, chan, data[1], -1,
                                             voiceManager.midiToFloatVelocity(data[2]));
        }
        else
        {
            // Hosts should prefer CLAP_NOTE events but if they don't
            voiceManager.processNoteOnEvent(port_index, chan, data[1], -1,
                                            voiceManager.midiToFloatVelocity(data[2]), 0.f);
        }

        if (data[1] == 120)
        {
            voiceManager.allSoundsOff();
        }
        if (data[1] == 123)
        {
            voiceManager.allNotesOff();
        }
        break;
    }
    case 0x80:
    {
        // Hosts should prefer CLAP_NOTE events but if they don't
        voiceManager.processNoteOffEvent(port_index, chan, data[1], -1,
                                         voiceManager.midiToFloatVelocity(data[2]));
        break;
    }
    case 0xA0:
    {
        auto key = data[1];
        auto pres = data[2];

        voiceManager.routePolyphonicAftertouch(port_index, chan, key, pres);
        break;
    }
    case 0xB0:
    {
        if (data[1] == 64)
        {
            voiceManager.updateSustainPedal(port_index, chan, data[2]);
        }
        else
        {
            voiceManager.routeMIDI1CC(port_index, chan, data[1], data[2]);
        }
        break;
    }
    case 0xD0:
    {
        voiceManager.routeChannelPressure(port_index, chan, data[1]);
    }
    break;
    case 0xE0:
    {
        // pitch bend
        auto bv = data[1] + data[2] * 128;

        voiceManager.routeMIDIPitchBend(port_index, chan, bv);

        break;
    }
    }
}
} // namespace sst::voicemanager
#endif // CONDUIT_MIDI1_TO_VOICEMANAGER_H
