$SCREENHIDE
$CONSOLE
_Dest _Console
ON ERROR GOTO errorhand

$IF WIN THEN
Print _DesktopHeight > 0
$ELSE
Print _DesktopHeight = 0
$END IF

$IF WIN THEN
Print _DesktopWidth > 0
$ELSE
Print _DesktopWidth = 0
$END IF

_Icon
Print "Got past icon!"

_MouseHide
Print "Got past MouseHide!"

_MouseShow
Print "Got past MouseHide!"

Print _ScreenExists

_ScreenHide
Print "Got past ScreenHide"

Print _ScreenIcon <> 0

Print _ScreenX > 0
Print _ScreenY > 0

_Title "foobar"
Print "Title: "; _Title$

Print _WindowHandle <> 0
Print _WindowHasFocus

_ScreenShow
Print "Got past ScreenShow!"
System

System

errorhand:
PRINT "Error:"; ERR; ", Line:"; _ERRORLINE
RESUME NEXT
