


SUB AddProfileEntry(nam As String, level As Long)
    Dim i As Long

    profileCount = profileCount + 1
    profileEntries(profileCount).entryName = nam
    profileEntries(profileCount).startTime = TIMER(.001)
    profileEntries(profileCount).level = level

    i = profileCount
    While i > 1
        i = i - 1
        If profileEntries(i).level < level Then _Continue
        If profileEntries(i).endTime <> 0 Then _Continue

        ' We found the previous entry of this level or higher, record the start
        ' time of our newest entry as the end time for this level
        profileEntries(i).endTime = profileEntries(profileCount).startTime
    Wend
END SUB


SUB WriteProfileEntries()
    Dim endTime As Single, level as Long, i As Long
    endTime = TIMER(.001)
    profileEntries(profileCount).endTime = endTime

    level = profileEntries(profileCount).level

    i = profileCount
    While i > 1
        i = i - 1
        If profileEntries(i).endTime <> 0 Then _Continue

        ' We found the previous entry of this level, finish it off and go to
        ' the next level up
        profileEntries(i).endTime = endTime
    Wend

    For i = 1 To profileCount
        Print Space$(profileEntries(i).level * 2); profileEntries(i).entryName; ":"; Using " ###.###s"; profileEntries(i).endTime - profileEntries(i).startTime
    Next
END SUB
