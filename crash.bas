_Delay 5
Print "crashing"
baz

SUB bar
    foo
END SUB

SUB foo
    crash
END SUB

SUB crash
    dim m as _mem, o as _offset
    o = 0
    m = _mem(o, 20)

    _memget m, m.offset, l&
END SUB

'$include: 'crash.bm'
