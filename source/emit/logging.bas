
SUB EmitLoggingStatement(elements As String, loglevel As String, hasPrefix As Long)
    SELECT CASE loglevel
        CASE "TRACE": func$ = "sub__logtrace": scaseLayout$ = "LogTrace"
        CASE "INFO": func$ = "sub__loginfo": scaseLayout$ = "LogInfo"
        CASE "WARN": func$ = "sub__logwarn": scaseLayout$ = "logWarn"
        CASE "ERROR": func$ = "sub__logerror": scaseLayout$ = "logError"
    END SELECT

    IF hasPrefix THEN scaseLayout$ = "_" + scaseLayout$

    e$ = fixoperationorder(elements)
    IF Error_Happened THEN EXIT SUB

    l$ = SCase$(scaseLayout$) + sp + tlayout$
    layoutdone = 1: pushelement layout$, l$

    e$ = evaluatetotyp(e$, ISSTRING)
    IF Error_Happened THEN EXIT SUB

    IF inclevel = 0 THEN
        WriteBufLine MainTxtBuf, func$ + "(" + AddQuotes$(escapeString$(sourcefile$)) + ", " + AddQuotes$(escapeString$(subfunc$)) + ", " + _TRIM$(STR$(linenumber)) + ", " + e$ + ");"
    ELSE
        WriteBufLine MainTxtBuf, func$ + "(" + AddQuotes$(escapeString$(incname$(inclevel))) + ", " + AddQuotes$(escapeString$(subfunc$)) + ", " + _TRIM$(STR$(inclinenumber(inclevel))) + ", " + e$ + ");"
    END IF

END SUB

