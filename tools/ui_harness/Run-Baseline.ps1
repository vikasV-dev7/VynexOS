# Baseline UI scenario: screenshots at several window shapes plus numeric input checks.
#
#   powershell -ExecutionPolicy Bypass -File tools\ui_harness\Run-Baseline.ps1 `
#       -Exe <build>\bin\vynex_init.exe -OutDir <scratch dir> [-Tag baseline]
#
# Runs a COPY of the executable from OutDir (never from the repo or its build tree) and writes
# screenshots and results.txt under OutDir\<Tag>. Window ids follow CompositionRoot's creation order:
# 1 Wallpaper, 2 Taskbar, 3 Launcher, 4 Terminal, 5 File Explorer, 6 Demo App.
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$OutDir,
    [string]$Tag = 'baseline'
)

. (Join-Path $PSScriptRoot 'VynexUiHarness.ps1')

$shots = Join-Path $OutDir $Tag
New-Item -ItemType Directory -Force $shots | Out-Null
$app = Start-VynexApp -Exe $Exe -WorkDir (Join-Path $OutDir "$Tag-run")

try {
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    Write-Host ("Screen {0}x{1}" -f $screen.Width, $screen.Height)

    # --- 1. Startup geometry --------------------------------------------------------------------------------
    Write-Host '[1] startup'
    $geo = Get-VynexGeometry $app
    Write-Host ("  client {0}x{1} at ({2},{3}); desktop scale {4:N4}" -f $geo.ClientW, $geo.ClientH, $geo.ClientX, $geo.ClientY, $geo.Scale)
    $ratio = $geo.ClientW / [double]$geo.ClientH
    Confirm-Vynex $app 'startup window is 16:9' ([Math]::Abs($ratio - 16.0 / 9.0) -lt 0.02) ("ratio {0:N3}" -f $ratio)
    Confirm-Vynex $app 'startup window fits on screen' (($geo.ClientX + $geo.ClientW) -le $screen.Right -and ($geo.ClientY + $geo.ClientH) -le $screen.Bottom) ''
    $s = Save-VynexScreenshot $app (Join-Path $shots '01_startup.png')
    Confirm-Vynex $app 'startup screenshot not clipped' (-not $s.Clipped) ''

    # --- 2. Pointer mapping + drag at several window shapes ------------------------------------------------
    Write-Host '[2] drag across window sizes (File Explorer, id 5)'
    $sizes = @(@(1280, 720), @(1000, 800), @(1400, 500), @(640, 640))
    $n = 0
    foreach ($sz in $sizes) {
        $n++
        Set-VynexClientSize $app $sz[0] $sz[1]
        $geo = Get-VynexGeometry $app
        $name = "02_{0}_{1}x{2}" -f $n, $geo.ClientW, $geo.ClientH
        [void](Save-VynexScreenshot $app (Join-Path $shots ($name + '.png')))
        Start-Sleep -Milliseconds 1300
        $before = Get-VynexWindowBounds $app 5
        if (-not $before) { Confirm-Vynex $app "bounds available at $($geo.ClientW)x$($geo.ClientH)" $false 'no log snapshot'; continue }
        $gx = $before.X + 350; $gy = $before.Y - 20           # middle of the title bar, logical coordinates
        Invoke-VynexDrag $app $geo $gx $gy ($gx + 40) ($gy + 30)
        Start-Sleep -Milliseconds 1300
        $after = Get-VynexWindowBounds $app 5
        $dx = $after.X - $before.X; $dy = $after.Y - $before.Y
        # One screen pixel spans 1/scale logical pixels, so the achievable accuracy depends on the window size.
        $tol = [int][Math]::Ceiling(1.0 / $geo.Scale) + 1
        Confirm-Vynex $app ("drag delta at {0}x{1}" -f $geo.ClientW, $geo.ClientH) ([Math]::Abs($dx - 40) -le $tol -and [Math]::Abs($dy - 30) -le $tol) ("moved ({0},{1}), expected (40,30) +/-{2}" -f $dx, $dy, $tol)
    }

    # --- 3. Launcher ---------------------------------------------------------------------------------------
    Write-Host '[3] launcher'
    Set-VynexClientSize $app 1280 720
    $geo = Get-VynexGeometry $app
    Invoke-VynexClick $app $geo 50 1060
    Start-Sleep -Milliseconds 1300
    $snap = Get-VynexSceneSnapshot $app
    Confirm-Vynex $app 'launcher visible after Vynex click' ($snap -and $snap.Layers.ContainsKey(3)) ''
    [void](Save-VynexScreenshot $app (Join-Path $shots '03_launcher_open.png'))
    Invoke-VynexClick $app $geo 50 1060
    Start-Sleep -Milliseconds 1300
    Confirm-Vynex $app 'launcher hidden after second click' (Test-VynexWindowExcluded $app 3 'Invisible') ''

    # --- 4. Taskbar switching ------------------------------------------------------------------------------
    Write-Host '[4] taskbar'
    $b = Get-VynexWindowBounds $app 5
    Invoke-VynexClick $app $geo ($b.X + 350) ($b.Y - 20)     # focus the Explorer first
    Invoke-VynexClick $app $geo 330 1060                      # its taskbar entry: focused window minimizes
    Start-Sleep -Milliseconds 1300
    Confirm-Vynex $app 'taskbar click minimizes the focused window' (Test-VynexWindowExcluded $app 5 'Minimized') ''
    [void](Save-VynexScreenshot $app (Join-Path $shots '04_taskbar_minimized.png'))
    Invoke-VynexClick $app $geo 330 1060
    Start-Sleep -Milliseconds 1300
    $snap = Get-VynexSceneSnapshot $app
    Confirm-Vynex $app 'taskbar click restores the window' ($snap -and $snap.Layers.ContainsKey(5)) ''

    # --- 5. Keyboard liveness ------------------------------------------------------------------------------
    # NOTE: no application consumes keys yet, so this only proves the app survives keyboard input.
    Write-Host '[5] keyboard (liveness only)'
    Send-VynexKeys $app 'hello'
    Confirm-Vynex $app 'app alive after typing' (-not $app.Process.HasExited) ''

    $errors = Get-VynexLogErrorCount $app
    Confirm-Vynex $app 'no [ERROR]/[FATAL] lines in app log' ($errors -eq 0) "count $errors"
}
finally {
    Stop-VynexApp $app
}

$summary = "{0} checks, {1} failed" -f $app.Checks, $app.Failures.Count
Write-Host ''
Write-Host $summary
if ($app.Failures.Count -gt 0) { Write-Host ('Failed: ' + ($app.Failures -join '; ')) -ForegroundColor Red }
Set-Content -Path (Join-Path $shots 'results.txt') -Value @($summary, ('Failed: ' + ($app.Failures -join '; '))) -Encoding UTF8
exit $app.Failures.Count
