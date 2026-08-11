Unicode true
RequestExecutionLevel user
ManifestDPIAware true
CRCCheck force
SetCompressor /SOLID lzma

!ifndef PAYLOAD_DIR
  !error "PAYLOAD_DIR must name the staged Windows release payload"
!endif
!ifndef SOURCE_DIR
  !error "SOURCE_DIR must name the source checkout"
!endif
!ifndef OUTPUT_FILE
  !error "OUTPUT_FILE must name the installer executable"
!endif
!ifndef RELEASE_LABEL
  !error "RELEASE_LABEL must name the validated tagged release"
!endif

!define PRODUCT_NAME "Jagged Alliance 2 1.13 SDL3"
!define PRODUCT_PUBLISHER "JA2 1.13 SDL3 contributors"
!define PRODUCT_WEB_SITE "https://github.com/tais/1vibe13"
!define PRODUCT_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\JA2-1.13-SDL3"
!define PRODUCT_INSTALL_KEY "Software\JA2-1.13-SDL3"
!define START_MENU_FOLDER "Jagged Alliance 2 1.13 SDL3"

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\JA2-1.13-SDL3"
InstallDirRegKey HKCU "${PRODUCT_INSTALL_KEY}" "InstallDir"
BrandingText "JA2 1.13 SDL3"
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "1.13.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "JA2 1.13 SDL3 engine installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${RELEASE_LABEL}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "JA2 1.13 SDL3 contributors"

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_ICON "${SOURCE_DIR}\Ja2\Res\jagged3.ico"
!define MUI_UNICON "${SOURCE_DIR}\Ja2\Res\jagged3.ico"
!define MUI_WELCOMEPAGE_TEXT \
  "This installs the engine only; original JA2 1.13 game data is required.\r\n\r\nChoose a directory containing Ja2.ini and Data/, or add your legally obtained game data after installation."
!define MUI_DIRECTORYPAGE_TEXT_TOP \
  "Select your JA2 1.13 directory. The uninstaller preserves Data/, mods, saves, configuration, and all other files it did not install."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
FunctionEnd

Section "JA2 1.13 SDL3 engine" MainSection
  SectionIn RO
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /oname=JA2_ENGLISH.exe "${PAYLOAD_DIR}\JA2_ENGLISH.exe"
  File /oname=JA2UB_ENGLISH.exe "${PAYLOAD_DIR}\JA2UB_ENGLISH.exe"
  File /oname=JA2MAPEDITOR_ENGLISH.exe "${PAYLOAD_DIR}\JA2MAPEDITOR_ENGLISH.exe"
  File /oname=README-engine-only.txt "${SOURCE_DIR}\packaging\windows\ENGINE-ONLY.txt"

  SetOutPath "$INSTDIR\ja2server"
  File /oname=ja2server.exe "${PAYLOAD_DIR}\ja2server\ja2server.exe"
  # The live ja2_mp.ini is user configuration. Ship a refreshable sample,
  # but never overwrite or claim ownership of an existing operator config.
  File /oname=ja2_mp.ini.sample "${PAYLOAD_DIR}\ja2server\ja2_mp.ini"
  File /oname=README.md "${PAYLOAD_DIR}\ja2server\README.md"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall-JA2-SDL3.exe"
  WriteRegStr HKCU "${PRODUCT_INSTALL_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayVersion" "${RELEASE_LABEL}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\JA2_ENGLISH.exe"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall-JA2-SDL3.exe"'
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\${START_MENU_FOLDER}"
  CreateShortcut "$SMPROGRAMS\${START_MENU_FOLDER}\Jagged Alliance 2.lnk" \
    "$INSTDIR\JA2_ENGLISH.exe" "" "$INSTDIR\JA2_ENGLISH.exe"
  CreateShortcut "$SMPROGRAMS\${START_MENU_FOLDER}\Unfinished Business.lnk" \
    "$INSTDIR\JA2UB_ENGLISH.exe" "" "$INSTDIR\JA2UB_ENGLISH.exe"
  CreateShortcut "$SMPROGRAMS\${START_MENU_FOLDER}\Map Editor.lnk" \
    "$INSTDIR\JA2MAPEDITOR_ENGLISH.exe" "" "$INSTDIR\JA2MAPEDITOR_ENGLISH.exe"
  CreateShortcut "$SMPROGRAMS\${START_MENU_FOLDER}\Uninstall.lnk" \
    "$INSTDIR\Uninstall-JA2-SDL3.exe"
SectionEnd

Section "Uninstall"
  SetShellVarContext current

  Delete "$SMPROGRAMS\${START_MENU_FOLDER}\Jagged Alliance 2.lnk"
  Delete "$SMPROGRAMS\${START_MENU_FOLDER}\Unfinished Business.lnk"
  Delete "$SMPROGRAMS\${START_MENU_FOLDER}\Map Editor.lnk"
  Delete "$SMPROGRAMS\${START_MENU_FOLDER}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${START_MENU_FOLDER}"

  Delete "$INSTDIR\JA2_ENGLISH.exe"
  Delete "$INSTDIR\JA2UB_ENGLISH.exe"
  Delete "$INSTDIR\JA2MAPEDITOR_ENGLISH.exe"
  Delete "$INSTDIR\README-engine-only.txt"
  Delete "$INSTDIR\ja2server\ja2server.exe"
  Delete "$INSTDIR\ja2server\ja2_mp.ini.sample"
  Delete "$INSTDIR\ja2server\README.md"
  RMDir "$INSTDIR\ja2server"

  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_INSTALL_KEY}"
  Delete "$INSTDIR\Uninstall-JA2-SDL3.exe"

  # These non-recursive removals succeed only when the directory is empty.
  # Never recursively remove $INSTDIR: users put Data/, mods, and saves here.
  RMDir "$INSTDIR"
SectionEnd
