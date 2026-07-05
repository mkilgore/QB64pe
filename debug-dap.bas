$UNSTABLE:TYPEFIELDS
$CONSOLE

_Font 16

'$include: 'json.bi'
'$include: 'dap.bi'

Dim exe as String

exe = Command$(1)
print "Exe: "; exe

IF exe = "" THEN
    Print "Please supply exe to debug as first argument"
    System 1
END IF

Dim Shared Dap(0) As DapConnection

IF NOT DapStartConnection&(Dap(0), exe) THEN
    Print "Unable to start dap server with exe"
    End
END IF

DapQueueConfigurationDone Dap(0)

Dim Response(0) As DapResponse

DO WHILE INKEY$ = ""
    _Limit 120

    IF DapPumpQueue&(Dap(0), Response(0)) THEN
        IF Response(0).Typ = DapTypeResponse THEN
            HandleResponse Response(0)
        ELSEIF Response(0).Typ = DapTypeEvent THEN
            HandleEvent Response(0)
        ELSE
            _Echo "Unknown Dap response, : " + JsonRender$(Response(0).j)
        END IF
    END IF
LOOP

SUB HandleEvent(Response As DapResponse)
    SELECT CASE Response.Event
        CASE "stopped"
            PRINT "Stopped: "; Dap(0).Stopped.description

            DapQueueThreads Dap(0)
            DapQueueStackTrace Dap(0), Dap(0).Stopped.threadId
    END SELECT
END SUB

SUB HandleResponse(Response As DapResponse)
    SELECT CASE Response.Command
        CASE "stackTrace"
            FOR i = 0 TO UBOUND(Response.StackFrame.threadStackFrame)
                PRINT USING " [##] "; i;
                PRINT ProcessQB64Symbol$(Response.StackFrame.threadStackFrame(i).nam); " in "; Response.StackFrame.threadStackFrame(i).source.nam; " at"; Response.StackFrame.threadStackFrame(i).lin
            NEXT
    END SELECT
END SUB

FUNCTION ProcessQB64Symbol$(s as String)
    IF LEFT$(s, 4) = "SUB_" THEN
        ProcessQB64Symbol$ = MID$(s, 5)
    ELSEIF LEFT$(s, 5) = "FUNC_" THEN
        ProcessQB64Symbol$ = MID$(s, 6)
    ELSEIF LEFT$(s, 7) = "QBMAIN(" THEN
        ProcessQB64Symbol$ = "Main Module"
    ELSE
        ProcessQB64Symbol$ = s
    END IF
END FUNCTION

'$include: 'json.bm'
'$include: 'dap.bm'


