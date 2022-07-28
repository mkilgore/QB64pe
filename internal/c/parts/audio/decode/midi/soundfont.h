#ifndef AUDIO_DECODE_MIDI_SOUNDFONT_H
#define AUDIO_DECODE_MIDI_SOUNDFONT_H

extern "C" {
    // These symbols are provided via objcopy and are the raw data of the
    // soundfont to use
    //
    // We provide a macro to expand to the correct symbol name so that the code
    // using these symbols doesn't have to be conditional

#ifdef QB64_WINDOWS
    // On Windows the symbol retains its _ prefix.
    extern char _binary_soundfont_sf2_start[];
    extern size_t _binary_soundfont_sf2_size;

#   define SOUNDFONT_BIN _binary_soundfont_sf2_start
#   define SOUNDFONT_SIZE _binary_soundfont_sf2_size
#else
    extern char _binary_soundfont_sf2_start[];
    extern size_t _binary_soundfont_sf2_size;

#   define SOUNDFONT_BIN _binary_soundfont_sf2_start
#   define SOUNDFONT_SIZE _binary_soundfont_sf2_size
#endif
}

#endif
