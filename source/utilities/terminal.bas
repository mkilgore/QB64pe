' This function finds a usable terminal installed on Linux and provides the
' required configuration for it. This allows the terminal to display $CONSOLE
' programs and logging if requested.
' 
' The user can replace the result with their own configuration, this is just a
' best effort to find a working default.
FUNCTION findWorkingTerminal$()
    DIM Exes$(7), Formats$(7)
    Exes$(1) = "gnome-terminal": Formats$(1) = "-- $$ $@"
    Exes$(2) = "konsole": Formats$(2) = "-e $$ $@"
    Exes$(3) = "lxterminal": Formats$(3) = "-e $$ $@"
    Exes$(4) = "mate-terminal": Formats$(4) = "-x $$ $@"
    Exes$(5) = "xfce4-terminal": Formats$(5) = "-x $$ $@"
    Exes$(6) = "urxvt": Formats$(6) = "-e $$ $@"
    Exes$(7) = "xterm": Formats$(7) = "-e $$ $@"

    FOR i = 1 TO UBOUND(Exes$)
        ret& = SHELL("command -v " + CHR$(34) + Exes$(i) + CHR$(34) + " >/dev/null 2>&1")

        IF ret& = 0 THEN
            findWorkingTerminal$ = Exes$(i) + " " + Formats$(i)
            EXIT FUNCTION
        END IF
    NEXT

    findWorkingTerminal$ = ""
END FUNCTION
