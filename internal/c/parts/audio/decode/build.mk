
DEP_AUDIO_DECODE_MIDI ?= $(DEP_AUDIO_DECODE)

decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += tml_impl.cpp
decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += tsf_impl.cpp
decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += midi.cpp

# If DEP_AUDIO_DECODE_MIDI is blank, then we compile a stub that doesn't do
# anything
decode-midi-srcs-y$(DEP_AUDIO_DECODE_MIDI) += stub_midi.cpp

DECODE_MIDI_OBJS := $(patsubst %.cpp,$(PATH_INTERNAL_C)/parts/audio/decode/midi/%.o,$(decode-midi-srcs-y))

CLEAN_LIST += $(DECODE_MIDI_OBJS)

$(PATH_INTERNAL_C)/parts/audio/decode/midi/%.o: $(PATH_INTERNAL_C)/parts/audio/decode/midi/%.cpp
	$(CXX) $(CXXFLAGS) -Wall $< -c -o $@

EXE_LIBS += $(DECODE_MIDI_OBJS)
