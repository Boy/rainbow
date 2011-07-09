; Installer for non-redistributable components (nrc). Searches locally for an existing viewer
; installation, checks version and size and copies the files into the the Rainbow Viewer directory.
; All credits for this small installer to the Emerald guys!
; Needs to be compiled with AutoIt v3, http://www.autoitscript.com
#NoTrayIcon
#RequireAdmin
$LLKDU = ""
$SLVOICE = ""
$FOUND = ""
$SLDIR = RegRead("HKLM\SOFTWARE\Linden Research, Inc.\SecondLife", "")
CHECKFILES()
$SLDIR = RegRead("HKLM\SOFTWARE\Linden Research, Inc.\SecondLifeReleaseCandidate", "")
CHECKFILES()
$SLDIR = RegRead("HKLM\SOFTWARE\Linden Research, Inc.\Snowglobe", "")
CHECKFILES()
$SLDIR = RegRead("HKLM\SOFTWARE\CoolViewer\CoolViewer", "")
CHECKFILES()
$SLDIR = RegRead("HKLM\SOFTWARE\RainbowViewer\RainbowViewer", "")
CHECKFILES()
If $LLKDU <> "" Then
	$FOUND &= "KDU "
EndIf
If $SLVOICE <> "" Then
	$FOUND &= "SLVoice "
EndIf
If $FOUND <> "" Then
	$RESULT = MsgBox(292, "Non-redistributable SecondLife Files", "The following non-redistributable SL components were found"&@CRLF&"on your system from an existing viewer installation and can"&@CRLF&"be automatically installed into Rainbow Viewer:"&@CRLF&@CRLF&$FOUND&@CRLF&@CRLF&"That is recommended for optimum graphics performance and full"&@CRLF&"functionality including voice features."&@CRLF&@CRLF&"Do you want to continue and copy these files?")
	If $RESULT = 6 Then
		If $LLKDU <> "" Then
			FileCopy($LLKDU, @ScriptDir, 1)
		EndIf
		If $SLVOICE <> "" Then
			FileCopy($SLVOICE & "\SLVoice.exe", @ScriptDir, 1)
			FileCopy($SLVOICE & "\vivoxsdk.dll", @ScriptDir, 1)
		EndIf
	EndIf
Else
	MsgBox(64, "Non-redistributable SecondLife Files", "No non-redistributable SL components could be found on your system. For optimum performance please install them manually as described in README_IMPORTANT!!!.txt")
EndIf

Func CHECKFILES()
	Local $VER
	Local $LEN
	If $LLKDU = "" Then
		$LEN = FileGetSize($SLDIR & "\llkdu.dll")
		If ($LEN = 753664 Or $LEN = 1208320 Or $LEN = 1175552) Then
			$LLKDU = $SLDIR & "\llkdu.dll"
		EndIf
	EndIf
	If $SLVOICE = "" Then
		$VER = FileGetVersion($SLDIR & "\vivoxsdk.dll")
		$LEN1 = FileGetSize($SLDIR & "\SLVoice.exe")
		If ($VER = "2.1.3010.6151" Or $VER = "2.1.3010.6270") And $LEN1 = 946176 Then
			$SLVOICE = $SLDIR
		EndIf
	EndIf
EndFunc