# VynexOS UI test harness (Windows PowerShell 5.1).
#
# Dot-source this file, then drive a *copy* of vynex_init.exe:
#
#   . .\tools\ui_harness\VynexUiHarness.ps1
#   $app = Start-VynexApp -Exe <path\to\vynex_init.exe> -WorkDir <scratch dir>
#   $geo = Get-VynexGeometry $app
#   Invoke-VynexClick $app $geo 50 1060        # logical desktop coordinates (1920x1080 space)
#   Save-VynexScreenshot $app 'out.png'
#   Stop-VynexApp $app
#
# Design rules
#   * Coordinates passed to Invoke-* are LOGICAL desktop coordinates; the harness maps them through the
#     same aspect-preserving viewport the app uses, so it exercises the real pointer path end to end.
#   * No input is ever sent unless the app window is the foreground window and the target lies inside its
#     client area. Otherwise the call throws instead of clicking something else on the desktop.
#   * Ground truth for window positions comes from the app's own log (logical bounds), not from pixels.

Set-StrictMode -Version 2.0

if (-not ('VynexNative' -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class VynexNative {
    [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct RC { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RC r);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RC r);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
}
"@
}
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[void][VynexNative]::SetProcessDPIAware()   # work in physical pixels, like a DPI-aware observer

$script:LogicalW = 1920.0
$script:LogicalH = 1080.0

function Start-VynexApp {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string]$WorkDir,
        [int]$StartupSeconds = 4,
        [string[]]$Arguments = @()   # command-line arguments for the app, e.g. '--vfs-profile=empty'
    )
    if (-not (Test-Path $Exe)) { throw "Executable not found: $Exe" }
    New-Item -ItemType Directory -Force $WorkDir | Out-Null
    $run = Join-Path $WorkDir 'run'
    New-Item -ItemType Directory -Force $run | Out-Null
    Copy-Item $Exe $run -Force
    $log = Join-Path $run 'vynexos_runtime.log'
    if (Test-Path $log) { Clear-Content $log -ErrorAction SilentlyContinue }

    $exeCopy = Join-Path $run (Split-Path $Exe -Leaf)
    $proc = if ($Arguments.Count -gt 0) { Start-Process $exeCopy -ArgumentList $Arguments -WorkingDirectory $run -PassThru } else { Start-Process $exeCopy -WorkingDirectory $run -PassThru }
Start-Sleep -Seconds $StartupSeconds
    $proc.Refresh()
    if ($proc.HasExited) { throw "App exited during startup (exit code $($proc.ExitCode))" }
    if ($proc.MainWindowHandle -eq [IntPtr]::Zero) { Stop-Process -Id $proc.Id -Force; throw 'App has no window' }
    $app = [pscustomobject]@{ Process = $proc; Handle = $proc.MainWindowHandle; Log = $log; Run = $run; Failures = New-Object System.Collections.ArrayList; Checks = 0 }
    Set-VynexForeground $app
    return $app
}

function Stop-VynexApp {
    param([Parameter(Mandatory)]$App)
    if ($App.Process -and -not $App.Process.HasExited) { Stop-Process -Id $App.Process.Id -Force }
}

function Set-VynexForeground {
    param([Parameter(Mandatory)]$App)
    for ($i = 0; $i -lt 5; $i++) {
        [void][VynexNative]::SetForegroundWindow($App.Handle)
        Start-Sleep -Milliseconds 300
        if ([VynexNative]::GetForegroundWindow() -eq $App.Handle) { return }
    }
    throw 'Could not bring the VynexOS window to the foreground; refusing to send input.'
}

# Client rectangle and the aspect-preserving viewport the app draws the desktop into.
function Get-VynexGeometry {
    param([Parameter(Mandatory)]$App)
    $pt = New-Object VynexNative+PT
    [void][VynexNative]::ClientToScreen($App.Handle, [ref]$pt)
    $rc = New-Object VynexNative+RC
    [void][VynexNative]::GetClientRect($App.Handle, [ref]$rc)
    $w = [double]$rc.R; $h = [double]$rc.B
    $scale = [Math]::Min($w / $script:LogicalW, $h / $script:LogicalH)
    $vw = $script:LogicalW * $scale; $vh = $script:LogicalH * $scale
    return [pscustomobject]@{
        ClientX = $pt.X; ClientY = $pt.Y; ClientW = $rc.R; ClientH = $rc.B
        ViewX = $pt.X + ($w - $vw) / 2.0; ViewY = $pt.Y + ($h - $vh) / 2.0
        ViewW = $vw; ViewH = $vh; Scale = $scale
    }
}

function ConvertTo-VynexScreenPoint {
    param([Parameter(Mandatory)]$Geo, [double]$X, [double]$Y)
    return [pscustomobject]@{ X = [int][Math]::Round($Geo.ViewX + $X * $Geo.Scale); Y = [int][Math]::Round($Geo.ViewY + $Y * $Geo.Scale) }
}

function Assert-VynexInputSafe {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)]$Geo, [int]$ScreenX, [int]$ScreenY)
    if ([VynexNative]::GetForegroundWindow() -ne $App.Handle) { throw 'VynexOS window is not foreground; refusing to send input.' }
    if ($App.Process.HasExited) { throw 'App is not running.' }
    if ($ScreenX -lt $Geo.ClientX -or $ScreenY -lt $Geo.ClientY -or
        $ScreenX -ge ($Geo.ClientX + $Geo.ClientW) -or $ScreenY -ge ($Geo.ClientY + $Geo.ClientH)) {
        throw "Target ($ScreenX,$ScreenY) is outside the app client area; refusing to send input."
    }
}

function Move-VynexMouse {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)]$Geo, [double]$X, [double]$Y, [int]$SettleMs = 150)
    $p = ConvertTo-VynexScreenPoint $Geo $X $Y
    Assert-VynexInputSafe $App $Geo $p.X $p.Y
    [void][VynexNative]::SetCursorPos($p.X, $p.Y)
    Start-Sleep -Milliseconds $SettleMs
}

function Invoke-VynexClick {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)]$Geo, [double]$X, [double]$Y, [int]$SettleMs = 450)
    Move-VynexMouse $App $Geo $X $Y 200
    [VynexNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [VynexNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $SettleMs
}

function Invoke-VynexDrag {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)]$Geo, [double]$X0, [double]$Y0, [double]$X1, [double]$Y1, [int]$Steps = 30)
    Move-VynexMouse $App $Geo $X0 $Y0 200
    [VynexNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    try {
        for ($i = 1; $i -le $Steps; $i++) {
            $p = ConvertTo-VynexScreenPoint $Geo ($X0 + ($X1 - $X0) * $i / $Steps) ($Y0 + ($Y1 - $Y0) * $i / $Steps)
            Assert-VynexInputSafe $App $Geo $p.X $p.Y
            [void][VynexNative]::SetCursorPos($p.X, $p.Y)
            Start-Sleep -Milliseconds 30
        }
        Start-Sleep -Milliseconds 150
    } finally {
        [VynexNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # always release the button
    }
    Start-Sleep -Milliseconds 450
}

function Send-VynexKeys {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)][string]$Keys)
    Set-VynexForeground $App
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
    Start-Sleep -Milliseconds 400
}

# Resize so the CLIENT area becomes the requested size (physical pixels).
function Set-VynexClientSize {
    param([Parameter(Mandatory)]$App, [int]$Width, [int]$Height, [int]$SettleMs = 1500)
    $wr = New-Object VynexNative+RC; [void][VynexNative]::GetWindowRect($App.Handle, [ref]$wr)
    $cr = New-Object VynexNative+RC; [void][VynexNative]::GetClientRect($App.Handle, [ref]$cr)
    $dw = ($wr.R - $wr.L) - $cr.R; $dh = ($wr.B - $wr.T) - $cr.B
    # SWP_NOMOVE | SWP_NOZORDER = 0x6
    [void][VynexNative]::SetWindowPos($App.Handle, [IntPtr]::Zero, 0, 0, $Width + $dw, $Height + $dh, 0x6)
    Start-Sleep -Milliseconds $SettleMs
}

# Captures the app's client area (not the whole screen) so images contain only VynexOS pixels.
function Save-VynexScreenshot {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)][string]$Path)
    $geo = Get-VynexGeometry $App
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $x = [Math]::Max($geo.ClientX, $screen.Left); $y = [Math]::Max($geo.ClientY, $screen.Top)
    $w = [Math]::Min($geo.ClientX + $geo.ClientW, $screen.Right) - $x
    $h = [Math]::Min($geo.ClientY + $geo.ClientH, $screen.Bottom) - $y
    if ($w -le 0 -or $h -le 0) { throw 'Client area is not on screen.' }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($x, $y, 0, 0, $bmp.Size)
    New-Item -ItemType Directory -Force (Split-Path $Path -Parent) | Out-Null
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    # Report whether the whole client area was visible; a clipped capture must not be mistaken for a full one.
    $clipped = ($w -lt $geo.ClientW) -or ($h -lt $geo.ClientH)
    return [pscustomobject]@{ Path = $Path; Width = $w; Height = $h; Clipped = $clipped }
}

# The app dumps its window table and scene graph about once a second. This returns the most recent COMPLETE
# dump so results are never read from a half-written or stale block.
#   Registered[id] = logical bounds from the window table (present even when hidden/minimized)
#   Layers[id]     = logical bounds of windows that are actually in the scene (i.e. visible on screen)
#   Excluded[id]   = reason the window was left out of the scene (Invisible, Minimized, ...)
function Get-VynexSceneSnapshot {
    param([Parameter(Mandatory)]$App)
    $lines = @(Get-Content $App.Log -Tail 300 -ErrorAction SilentlyContinue)
    $markers = @(for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match 'Generated SceneGraph:') { $i } })
    for ($k = $markers.Count - 1; $k -ge 0; $k--) {
        $from = $markers[$k]
        $to = if ($k + 1 -lt $markers.Count) { $markers[$k + 1] - 1 } else { $lines.Count - 1 }
        $block = $lines[$from..$to]
        if (-not ($block -match 'RenderLayer count:')) { continue }   # incomplete: still being written
        $snap = [pscustomobject]@{ Registered = @{}; Layers = @{}; Excluded = @{} }
        $tableFrom = [Math]::Max(0, $from - 40)
        foreach ($l in $lines[$tableFrom..$from]) {
            if ($l -match 'Window ID: (\d+), Title: .*?Bounds: \[(-?\d+),(-?\d+),(\d+)x(\d+)\]') {
                $snap.Registered[[int]$Matches[1]] = [pscustomobject]@{ X = [int]$Matches[2]; Y = [int]$Matches[3]; W = [int]$Matches[4]; H = [int]$Matches[5] }
            }
        }
        foreach ($l in $block) {
            if ($l -match 'RenderLayer Content: Window ID: (\d+), Bounds: \[(-?\d+),(-?\d+),(\d+)x(\d+)\]') {
                $snap.Layers[[int]$Matches[1]] = [pscustomobject]@{ X = [int]$Matches[2]; Y = [int]$Matches[3]; W = [int]$Matches[4]; H = [int]$Matches[5] }
            } elseif ($l -match 'Excluded Window ID: (\d+) .*Reason: (\w+)') {
                $snap.Excluded[[int]$Matches[1]] = $Matches[2]
            }
        }
        return $snap
    }
    return $null
}

# Latest logical bounds the app logged for a window id.
function Get-VynexWindowBounds {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)][int]$WindowId)
    $snap = Get-VynexSceneSnapshot $App
    if ($snap -and $snap.Registered.ContainsKey($WindowId)) { return $snap.Registered[$WindowId] }
    return $null
}

# True when the latest complete dump says the window was left out of the scene for the given reason.
function Test-VynexWindowExcluded {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)][int]$WindowId, [string]$Reason)
    $snap = Get-VynexSceneSnapshot $App
    return [bool]($snap -and $snap.Excluded.ContainsKey($WindowId) -and $snap.Excluded[$WindowId] -eq $Reason)
}

# Colour of the on-screen pixel under a logical desktop coordinate (0..255 per channel).
function Get-VynexPixel {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)]$Geo, [double]$X, [double]$Y)
    $p = ConvertTo-VynexScreenPoint $Geo $X $Y
    Assert-VynexInputSafe $App $Geo $p.X $p.Y
    $bmp = New-Object System.Drawing.Bitmap 1, 1
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($p.X, $p.Y, 0, 0, $bmp.Size)
    $c = $bmp.GetPixel(0, 0)
    $g.Dispose(); $bmp.Dispose()
    return [pscustomobject]@{ R = [int]$c.R; G = [int]$c.G; B = [int]$c.B }
}

# A colour token from the design tokens in include/vynexos/desktop/theme.hpp (e.g. 'surface_raised'), read from the
# source so checks follow the theme instead of copying its values.
function Get-VynexThemeColor {
    param([Parameter(Mandatory)][string]$Token)
    $theme = Join-Path $PSScriptRoot '..\..\include\vynexos\desktop\theme.hpp'
    $line = Select-String -Path $theme -Pattern ("\b{0}\{{rgb\(0x([0-9A-Fa-f]{{6}})" -f [regex]::Escape($Token)) | Select-Object -First 1
    if (-not $line) { throw "Theme token '$Token' not found in $theme" }
    $hex = [Convert]::ToInt32($line.Matches[0].Groups[1].Value, 16)
    return [pscustomobject]@{ R = ($hex -shr 16) -band 0xFF; G = ($hex -shr 8) -band 0xFF; B = $hex -band 0xFF }
}

# True when two colours match within a per-channel tolerance (screen capture of a scaled frame can shift a few levels).
function Test-VynexColorNear {
    param([Parameter(Mandatory)]$A, [Parameter(Mandatory)]$B, [int]$Tolerance = 6)
    return ([Math]::Abs($A.R - $B.R) -le $Tolerance) -and ([Math]::Abs($A.G - $B.G) -le $Tolerance) -and ([Math]::Abs($A.B - $B.B) -le $Tolerance)
}

function Get-VynexLogErrorCount {
    param([Parameter(Mandatory)]$App)
    return @(Select-String -Path $App.Log -Pattern '\[ERROR\]|\[FATAL\]' -ErrorAction SilentlyContinue).Count
}

# Lightweight check recorder so scenarios report pass/fail instead of silently continuing.
function Confirm-Vynex {
    param([Parameter(Mandatory)]$App, [Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][bool]$Condition, [string]$Detail = '')
    $App.Checks++
    if ($Condition) { Write-Host ("  PASS  {0}  {1}" -f $Name, $Detail) }
    else { [void]$App.Failures.Add($Name); Write-Host ("  FAIL  {0}  {1}" -f $Name, $Detail) -ForegroundColor Red }
}
