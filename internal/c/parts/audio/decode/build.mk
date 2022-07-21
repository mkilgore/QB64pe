
decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += tml_impl.cpp
decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += tsf_impl.cpp
decode-midi-srcs-$(DEP_AUDIO_DECODE_MIDI) += midi.cpp

# If DEP_AUDIO_DECODE_MIDI is blank, then we compile a stub that doesn't do
# anything
decode-midi-srcs-y$(DEP_AUDIO_DECODE_MIDI) += stub_midi.cpp

DECODE_MIDI_OBJS-y := $(patsubst %.cpp,$(PATH_INTERNAL_C)/parts/audio/decode/midi/%.o,$(decode-midi-srcs-y))

# If we're using MIDI, then there should be a soundfont availiable to be linked in
DECODE_MIDI_OBJS-$(DEP_AUDIO_DECODE_MIDI) += $(PATH_INTERNAL_TEMP)/soundfont.o

# Turn the soundfont into a linkable object
# The "cd" is used to make the symbol name predictable and not dependent upon
# the "PATH_INTERNAL_TEMP" value
ifeq ($(OS),win)
# Fairly ugly, but we have to get the right relative path to objcopy on Windows
# to make the whole thing work out
$(PATH_INTERNAL_TEMP)/soundfont.o: $(PATH_INTERNAL_TEMP)/soundfont.sf2
	cd $(call FIXPATH,$(PATH_INTERNAL_TEMP)) && ..\..\$(OBJCOPY) -Ibinary $(OBJCOPY_FLAGS) soundfont.sf2 soundfont.o
else
$(PATH_INTERNAL_TEMP)/soundfont.o: $(PATH_INTERNAL_TEMP)/soundfont.sf2
	cd $(call FIXPATH,$(PATH_INTERNAL_TEMP)) && $(OBJCOPY) -Ibinary $(OBJCOPY_FLAGS) soundfont.sf2 soundfont.o
endif

$(PATH_INTERNAL_C)/parts/audio/decode/midi/%.o: $(PATH_INTERNAL_C)/parts/audio/decode/midi/%.cpp
	$(CXX) $(CXXFLAGS) -Wall $< -c -o $@

CLEAN_LIST += $(DECODE_MIDI_OBJS-y)
EXE_LIBS += $(DECODE_MIDI_OBJS-y)
