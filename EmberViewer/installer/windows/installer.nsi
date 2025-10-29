; EmberViewer NSIS Installer Script
; Creates a Windows installer for EmberViewer

!define APPNAME "EmberViewer"
!define COMPANYNAME "Magnus Overli"
!define DESCRIPTION "Ember+ Protocol Viewer"
!define VERSIONMAJOR 0
!define VERSIONMINOR 0
!define VERSIONBUILD 5

; Compression settings - lzma provides good compression with faster build time than default
SetCompressor /SOLID lzma
SetCompressorDictSize 8

; Installer details
Name "${APPNAME}"
OutFile "EmberViewer-Setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
RequestExecutionLevel admin

; Include Modern UI
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; Insert macros
!insertmacro GetSize
!insertmacro GetParameters
!insertmacro GetOptions

; Windows Messages (only define if not already defined)
!ifndef WM_CLOSE
!define WM_CLOSE 0x0010
!endif

; Icon settings
!define MUI_ICON "..\..\resources\icon.ico"
!define MUI_UNICON "..\..\resources\icon.ico"
Icon "..\..\resources\icon.ico"
UninstallIcon "..\..\resources\icon.ico"

; MUI Settings
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_LICENSE "..\..\..\LICENSE.TXT"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Installer Sections
Section "Install"
    ; Set output path
    SetOutPath "$INSTDIR"
    
    ; Check for /UPDATE parameter (used by auto-updater)
    ; When /UPDATE is passed, we skip the running app check and wait for it to close
    ${GetParameters} $R0
    ClearErrors
    ${GetOptions} $R0 "/UPDATE" $R1
    IfErrors not_update_mode
    
    ; UPDATE mode: Wait for app to close (max 10 seconds)
    StrCpy $R2 0
    wait_loop:
        FindWindow $R3 "" "EmberViewer"
        IntCmp $R3 0 app_closed
        IntOp $R2 $R2 + 1
        IntCmp $R2 100 wait_timeout  ; 100 * 100ms = 10 seconds
        Sleep 100
        Goto wait_loop
    
    wait_timeout:
        MessageBox MB_OKCANCEL "EmberViewer is still running. Click OK to force close it, or Cancel to abort the update." IDOK force_close
        Abort
    
    force_close:
        ; Try to close gracefully with WM_CLOSE
        FindWindow $R3 "" "EmberViewer"
        SendMessage $R3 ${WM_CLOSE} 0 0
        Sleep 1000
        Goto wait_loop
    
    app_closed:
        Goto check_done
    
    not_update_mode:
        ; Normal installation: Check if app is running
        ; Use FindWindow for more reliable detection
        FindWindow $R3 "" "EmberViewer"
        IntCmp $R3 0 check_done
        
        ; Also check with tasklist as backup
        nsExec::ExecToStack 'tasklist /FI "IMAGENAME eq EmberViewer.exe" /NH'
        Pop $0  ; Return code
        Pop $1  ; Output text
        
        StrCpy $2 $1 15  ; Get first 15 characters
        StrCmp $2 "EmberViewer.exe" 0 check_done
            ; App is running in normal installation
            MessageBox MB_OK "EmberViewer is currently running. Please close it and try again."
            Abort
    
    check_done:
    
    ; Copy files (all files from the build directory)
    ClearErrors
    File /r "..\..\..\deploy\*.*"
    
    ; Check if file copy was successful
    IfErrors file_copy_failed file_copy_success
    
    file_copy_failed:
        MessageBox MB_ICONSTOP "Failed to copy application files. The installation cannot continue."
        Abort
    
    file_copy_success:
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\EmberViewer.exe" "" "$INSTDIR\EmberViewer.exe" 0
    CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\EmberViewer.exe" "" "$INSTDIR\EmberViewer.exe" 0
    
    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Write registry for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME} - ${DESCRIPTION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\EmberViewer.exe"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1
    
    ; Estimate size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" "$0"
    
    ; Clean up the installer itself if it's in temp directory
    ; This handles auto-update installers that were copied to temp
    StrCpy $R0 "$EXEPATH"  ; Get installer path
    Push $R0
    Push "$TEMP"
    Call IsSubPath
    Pop $R1
    
    ; If installer is in temp directory, schedule it for deletion
    IntCmp $R1 1 0 skip_cleanup skip_cleanup
        ; Create a batch file to delete the installer after it exits
        FileOpen $R2 "$TEMP\cleanup_installer.bat" w
        FileWrite $R2 '@echo off$\r$\n'
        FileWrite $R2 'timeout /t 2 /nobreak > nul$\r$\n'
        FileWrite $R2 'del /f /q "$R0"$\r$\n'
        FileWrite $R2 'del /f /q "%~f0"$\r$\n'  ; Delete the batch file itself
        FileClose $R2
        
        ; Execute the cleanup batch file
        Exec '"$TEMP\cleanup_installer.bat"'
    
    skip_cleanup:
SectionEnd

; Helper function to check if a path is a subpath of another
Function IsSubPath
    Exch $R0  ; Parent path
    Exch
    Exch $R1  ; Child path
    Push $R2
    Push $R3
    
    StrLen $R2 $R0
    StrCpy $R3 $R1 $R2
    
    StrCmp $R3 $R0 0 +3
        StrCpy $R0 1
        Goto end
    StrCpy $R0 0
    
    end:
    Pop $R3
    Pop $R2
    Pop $R1
    Exch $R0
FunctionEnd

; Uninstaller Section
Section "Uninstall"
    ; Remove files and directories
    RMDir /r "$INSTDIR"
    
    ; Remove shortcuts
    Delete "$DESKTOP\${APPNAME}.lnk"
    RMDir /r "$SMPROGRAMS\${APPNAME}"
    
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
    DeleteRegKey HKLM "Software\${APPNAME}"
SectionEnd
