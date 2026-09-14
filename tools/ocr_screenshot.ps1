# ocr_screenshot.ps1 -- capture the screen and read game UI text via glm-ocr.
#
# Usage (from the repo root):
#   powershell -File tools/ocr_screenshot.ps1                 # OCR the current screen
#   powershell -File tools/ocr_screenshot.ps1 -Game           # launch game, capture, kill, OCR
#   powershell -File tools/ocr_screenshot.ps1 -Image <png>    # OCR an existing screenshot
#                                                            # (e.g. one saved from chat)
#
# Needs: ollama model glm-ocr:q8_0 pulled. Prints recognized text to stdout.
param([switch]$Game, [string]$Image = "")
$ErrorActionPreference = "Stop"
$shot = $Image
if ($shot -eq "")
{
    if ($Game)
    {
        $root = Split-Path -Parent $PSScriptRoot
        Start-Process "$root/prj/bin/Release/conflict-converge.exe" -WorkingDirectory $root
        Start-Sleep -Seconds 6
    }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $b = [Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object Drawing.Bitmap($b.Width, $b.Height)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
    $g.Dispose()
    $shot = Join-Path $env:TEMP "ocr_shot.png"
    $bmp.Save($shot, [Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    if ($Game)
    {
        Stop-Process -Name conflict-converge -ErrorAction SilentlyContinue
    }
}
ollama run glm-ocr:q8_0 "Text Recognition: $shot"
