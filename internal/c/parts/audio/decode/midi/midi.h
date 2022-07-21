#ifndef AUDIO_DECODE_MIDI_H
#define AUDIO_DECODE_MIDI_H

#include "snd.h"
#include <stdint.h>

void midi_init(void);

// Returns 1 if the provided file contents are a MIDI file
int midi_file_check(const uint8_t *file_content, size_t length);

// Should only be called if the `midi_file_check()` on this was successful
snd_sequence_struct *snd_decode_midi(const uint8_t *content, size_t length);

#endif
