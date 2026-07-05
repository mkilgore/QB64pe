
Type DapSource
    nam As String
    path As String
End Type

Type DapStackFrame
    id As Long
    nam As String
    source As DapSource
    lin As Long
    column As Long
    endLine As Long
    endColumn As Long
End Type

Type DapThread
    nam As String
    id As Long
End Type

Type DapStackFrameResponse
    threadStackFrame(0 TO 0) _Dynamic As DapStackFrame
    totalFrames As Long
END TYPE

Type DapStoppedEvent
    reason As String
    description As String
    threadId As Long
END TYPE

CONST DapTypeResponse = 1
CONST DapTypeEvent = 2
CONST DapTypeUnknown = 3

Type DapResponse
    Typ As Long
    Event As String
    Command As String
    HandledInternal As Long

    ' The entire response Json object
    j As Json

    StackFrame As DapStackFrameResponse
End Type

Type DapFuncBreakpoint
    nam As String
    src As String

    lin As Long
    column As Long
    endLine As Long
    endColumn As Long

    id As Long
    verified As _Byte
    offset As _Offset
End Type

CONST DapProcessStateNone = 0
CONST DapProcessStateCont = 1
CONST DapProcessStateStopped = 2
CONST DapProcessStateTermined = 3
CONST DapProcessStateExited = 4

Type DapConnection
    dbg As Long
    inStream As Long
    outStream As Long
    errStream As Long

    exe As String
    inputBuffer As String

    processState As Long

    Stopped As DapStoppedEvent ' Stopped State
    ExitCode As Long ' Exited State

    threads(0 TO 0) _Dynamic As DapThread

    ' funcBreakpoints(0 TO 0) _Dynamic As DapFuncBreakpoint
End Type


DECLARE FUNCTION DapStartConnection&(dap As DapConnection, exe As String)

DECLARE SUB DapQueueThreads(dap As DapConnection)
DECLARE SUB DapQueueStackTrace(dap As DapConnection, tid As Long)
DECLARE SUB DapQueueConfigurationDone(dap As DapConnection)

' DECLARE SUB DapAddFunctionBreakpoint(dap As DapConnection, src As String, func As String)

DECLARE FUNCTION DapPumpQueue&(dap As DapConnection, res As DapResponse)

