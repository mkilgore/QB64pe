$CONSOLE
$SCREENHIDE
_DEST _CONSOLE
CHDIR _STARTDIR$

$Unstable: Midi
$MidiSoundFont: "tests/compile_tests/midi_soundfont/test-soundfont.sf2"

handle = _SndOpen("./midi.mid")

print handle;
SYSTEM
