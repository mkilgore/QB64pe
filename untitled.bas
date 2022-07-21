
l = _sndopen("foobar.mid")

_sndplay l

do
    _delay .1
    locate 3, 3
    print "Position: "; _sndGetPos(l); "             ";
loop

