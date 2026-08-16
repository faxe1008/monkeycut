# Build a versioned MSI installer from a staged app directory.
# Run on Windows, after the app is built and staged (windeployqt + ffmpeg DLLs).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File package\wix\build-msi.ps1 `
#     -StageDir .\release\monkeycut -Version 0.1.0.42 -Out ..\dist\monkeycut-0.1.0.42-win64.msi
#
# GUIDs are derived from content/version (deterministic):
#   - ProductCode   : one per version (new products can upgrade old ones)
#   - UpgradeCode   : one stable channel for MonkeyCut
#   - file components: stable per file path (upgrades replace in place)

param(
    [Parameter(Mandatory)][string]$StageDir,
    [Parameter(Mandatory)][string]$Version,
    [string]$Out = (Join-Path $PSScriptRoot "..\..\dist\monkeycut-$Version-win64.msi"),
    [string]$Wix = "wix"
)
$ErrorActionPreference = "Stop"

$stage = (Resolve-Path $StageDir).Path.TrimEnd('\')
if (-not (Test-Path (Join-Path $stage "monkeycut.exe"))) {
    throw "monkeycut.exe not found in stage dir: $stage"
}

function New-DeterministicGuid([string]$seed) {
    $sha = [System.Security.Cryptography.SHA1]::Create().ComputeHash(
        [System.Text.Encoding]::UTF8.GetBytes($seed))
    $b = New-Object byte[] 16
    [Array]::Copy($sha, $b, 16)
    $b[6] = ($b[6] -band 0x0F) -bor 0x40
    $b[8] = ($b[8] -band 0x3F) -bor 0x80
    return [Guid]::new($b).ToString("B")
}

$productGuid = New-DeterministicGuid "monkeycut-product-$Version"
$upgradeGuid = New-DeterministicGuid "monkeycut-upgrade-channel-v1"
$startMenuGuid = New-DeterministicGuid "monkeycut-shortcut-startmenu"
$desktopGuid = New-DeterministicGuid "monkeycut-shortcut-desktop"

$files = Get-ChildItem $stage -Recurse -File | Sort-Object FullName

$xml = New-Object System.Text.StringBuilder
[void]$xml.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
[void]$xml.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$xml.AppendLine('  <Package Name="MonkeyCut" Manufacturer="faxe1008" Version="' + $Version + '" UpgradeCode="' + $upgradeGuid + '" ProductCode="' + $productGuid + '">')
[void]$xml.AppendLine('    <MajorUpgrade DowngradeErrorMessage="A newer version of MonkeyCut is already installed." />')
[void]$xml.AppendLine('    <StandardDirectory Id="ProgramFiles6432Folder">')
[void]$xml.AppendLine('      <Directory Id="APPDIR" Name="MonkeyCut">')

$componentIds = @()
foreach ($f in $files) {
    $rel = $f.FullName.Substring($stage.Length + 1).Replace('\', '/')
    $id = "F_" + ($rel -replace '[^0-9A-Za-z]', '_')
    $guid = New-DeterministicGuid "monkeycut-file-$rel"
    [void]$xml.AppendLine('        <Component Id="' + $id + '" Guid="' + $guid + '">')
    [void]$xml.AppendLine('          <File Source="' + $f.FullName + '" />')
    [void]$xml.AppendLine('        </Component>')
    $componentIds += $id
}

[void]$xml.AppendLine('      </Directory>')
[void]$xml.AppendLine('    </StandardDirectory>')

[void]$xml.AppendLine('    <StandardDirectory Id="ProgramMenuFolder">')
[void]$xml.AppendLine('      <Directory Id="APPSTARTMENU" Name="MonkeyCut">')
[void]$xml.AppendLine('        <Component Id="ShortcutStartMenuDir" Guid="' + $startMenuGuid + '">')
[void]$xml.AppendLine('          <CreateFolder />')
[void]$xml.AppendLine('          <Shortcut Id="StartMenuShortcut" Name="MonkeyCut" Target="[APPDIR]monkeycut.exe" WorkingDirectory="APPDIR" />')
[void]$xml.AppendLine('          <RemoveFolder Id="RemoveAppStartMenu" Directory="APPSTARTMENU" On="uninstall" />')
[void]$xml.AppendLine('          <RegistryValue Root="HKCU" Key="Software\MonkeyCut" Name="StartMenuShortcut" Type="integer" Value="1" KeyPath="yes" />')
[void]$xml.AppendLine('        </Component>')
[void]$xml.AppendLine('      </Directory>')
[void]$xml.AppendLine('    </StandardDirectory>')

[void]$xml.AppendLine('    <StandardDirectory Id="DesktopFolder">')
[void]$xml.AppendLine('      <Component Id="ShortcutDesktop" Guid="' + $desktopGuid + '">')
[void]$xml.AppendLine('        <Shortcut Id="DesktopShortcut" Name="MonkeyCut" Target="[APPDIR]monkeycut.exe" WorkingDirectory="APPDIR" />')
[void]$xml.AppendLine('        <RegistryValue Root="HKCU" Key="Software\MonkeyCut" Name="DesktopShortcut" Type="integer" Value="1" KeyPath="yes" />')
[void]$xml.AppendLine('      </Component>')
[void]$xml.AppendLine('    </StandardDirectory>')

[void]$xml.AppendLine('    <Feature Id="Main" Title="MonkeyCut" Level="1">')
foreach ($id in $componentIds) {
    [void]$xml.AppendLine('      <ComponentRef Id="' + $id + '" />')
}
[void]$xml.AppendLine('      <ComponentRef Id="ShortcutStartMenuDir" />')
[void]$xml.AppendLine('      <ComponentRef Id="ShortcutDesktop" />')
[void]$xml.AppendLine('    </Feature>')
[void]$xml.AppendLine('  </Package>')
[void]$xml.AppendLine('</Wix>')

$workDir = Join-Path $PSScriptRoot "..\..\.wix-build"
if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
New-Item -ItemType Directory $workDir | Out-Null

$wxsPath = Join-Path $workDir "monkeycut.wxs"
[System.IO.File]::WriteAllText($wxsPath, $xml.ToString(), [System.Text.Encoding]::UTF8)

$Out = (Resolve-Path (Split-Path $Out -Parent)).Path + "\$(Split-Path $Out -Leaf)"
& $Wix build $wxsPath -o $Out
if ($LASTEXITCODE -ne 0) { throw "wix build failed" }

Write-Host "written: $Out ($((Get-Item $Out).Length / 1MB) MB)"
