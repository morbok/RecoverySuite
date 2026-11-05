# Dedicated Windows CI fixture only. Never chooses a pre-existing physical disk.
param([Parameter(Mandatory=$true)][string]$Cli)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if ($env:CI -ne 'true' -or $env:RUNNER_OS -ne 'Windows') { throw 'Run only on a dedicated Windows CI runner' }
$Cli = (Resolve-Path $Cli).Path
$fixture = Join-Path $env:RUNNER_TEMP ('recoverysuite-vhd-' + [guid]::NewGuid())
New-Item -ItemType Directory $fixture | Out-Null
$vhd = Join-Path $fixture 'source.vhd'
$attached = $false
try {
    $instructions = Join-Path $fixture 'create.txt'
    "create vdisk file=`"$vhd`" maximum=32 type=fixed`r`nexit" | Set-Content -Encoding ascii $instructions
    diskpart /s $instructions
    if ($LASTEXITCODE -ne 0 -or !(Test-Path $vhd)) { throw 'Could not create the scratch VHD' }
    # Place a known marker in the virtual disk, leaving the fixed-VHD footer intact.
    $file = [System.IO.File]::Open($vhd, 'Open', 'ReadWrite', 'None')
    try {
        $marker = [System.Text.Encoding]::ASCII.GetBytes('RecoverySuite synthetic acquisition fixture')
        $file.Write($marker, 0, $marker.Length)
        $file.Position = 16MB
        $jpeg = [Convert]::FromHexString('FFD8FF68656C6C6FFFD9')
        $file.Write($jpeg, 0, $jpeg.Length)
        $file.Flush($true)
    } finally { $file.Dispose() }
    Mount-DiskImage -ImagePath $vhd -Access ReadOnly -NoDriveLetter | Out-Null
    $attached = $true
    $disks = @(Get-DiskImage -ImagePath $vhd | Get-Disk)
    if ($disks.Count -ne 1 -or $disks[0].IsBoot -or $disks[0].IsSystem -or $disks[0].Size -ne 32MB) {
        throw 'Scratch VHD identity/geometry check failed'
    }
    $disk = $disks[0]
    $device = '\\.\PhysicalDrive' + $disk.Number
    $output = Join-Path $fixture 'capture.img'
    Set-Disk -InputObject $disk -IsOffline $false
    & $Cli device capture $device --output $output
    if ($LASTEXITCODE -eq 0 -or (Test-Path $output)) { throw 'Online source was incorrectly accepted' }
    Set-Disk -InputObject $disk -IsOffline $true
    & $Cli device capture $device --output $output
    if ($LASTEXITCODE -ne 0) { throw 'Offline read-only VHD capture failed' }
    & $Cli device capture $device --output $output
    if ($LASTEXITCODE -eq 0) { throw 'Existing destination was incorrectly accepted' }
    & $Cli device scan $device
    if ($LASTEXITCODE -ne 0) { throw 'Direct physical source scan failed' }
    $recovered = Join-Path $fixture 'recovered'
    New-Item -ItemType Directory $recovered | Out-Null
    & $Cli device recover $device --output $recovered --select 'Signature matches/carved_0.jpg'
    if ($LASTEXITCODE -ne 0) { throw 'Selected file recovery from physical source failed' }
    $jpegPath = Join-Path $recovered 'Signature matches/carved_0.jpg'
    if ([Convert]::ToHexString([System.IO.File]::ReadAllBytes($jpegPath)) -ne 'FFD8FF68656C6C6FFFD9') {
        throw 'Direct physical source recovery returned incorrect bytes'
    }
    Write-Output 'Direct physical-source scan and selected-file recovery passed'
    Dismount-DiskImage -ImagePath $vhd
    $attached = $false
    $expected = Join-Path $fixture 'expected.img'
    $inputFile = [System.IO.File]::OpenRead($vhd)
    $outputFile = [System.IO.File]::Create($expected)
    try {
        $buffer = New-Object byte[] 65536
        $remaining = 32MB
        while ($remaining -gt 0) {
            $n = $inputFile.Read($buffer, 0, [Math]::Min($remaining, $buffer.Length))
            if ($n -eq 0) { throw 'Short fixture read' }
            $outputFile.Write($buffer, 0, $n)
            $remaining -= $n
        }
    } finally { $inputFile.Dispose(); $outputFile.Dispose() }
    if ((Get-Item $output).Length -ne 32MB -or (Get-FileHash $output).Hash -ne (Get-FileHash $expected).Hash) {
        throw 'Capture differs from the fixed VHD payload'
    }
    Write-Output 'Read-only VHD acquisition passed: online rejection, exact bytes, no-overwrite'
} finally {
    if ($attached) { Dismount-DiskImage -ImagePath $vhd }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
# The final CLI call intentionally rejects an existing output. Its nonzero
# LASTEXITCODE must not escape after all assertions and cleanup have succeeded.
exit 0
