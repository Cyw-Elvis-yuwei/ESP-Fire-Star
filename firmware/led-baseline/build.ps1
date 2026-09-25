param([string]$ArmCompiler = 'C:\Keil_v5\ARM\ARMCC\bin')
$ErrorActionPreference = 'Stop'
foreach ($tool in @('armcc.exe','armasm.exe','armlink.exe','fromelf.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $ArmCompiler $tool))) { throw "Missing compiler tool: $tool" }
}
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    $steps = @(
        @{ Tool='armcc.exe'; Args=@('--cpu','Cortex-M3','--c99','-O0','--debug','--apcs=interwork','--split_sections','-c','main.c','-o','build/main.o') },
        @{ Tool='armasm.exe'; Args=@('--cpu','Cortex-M3','--debug','--apcs=interwork','startup_stm32f10x_hd.s','-o','build/startup.o') },
        @{ Tool='armlink.exe'; Args=@('build/startup.o','build/main.o','--scatter','led.sct','--entry','Reset_Handler','--map','--list','build/led.map','--info','sizes','--output','build/led.axf') },
        @{ Tool='fromelf.exe'; Args=@('--i32','--output','build/led.hex','build/led.axf') },
        @{ Tool='fromelf.exe'; Args=@('--bin','--output','build/led.bin','build/led.axf') }
    )
    foreach ($step in $steps) {
        $arguments = $step.Args
        Write-Output ($step.Tool + ' ' + ($arguments -join ' '))
        & (Join-Path $ArmCompiler $step.Tool) @arguments
        if ($LASTEXITCODE -ne 0) { throw ($step.Tool + ' failed with exit ' + $LASTEXITCODE) }
    }
    $hashes = foreach ($artifact in @('build/led.axf','build/led.hex','build/led.bin')) {
        $artifactPath = Join-Path $PSScriptRoot $artifact
        $stream = [System.IO.File]::OpenRead($artifactPath)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hash = [System.BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()
            [pscustomobject]@{ Path=$artifactPath; Algorithm='SHA256'; Hash=$hash }
        } finally { $stream.Dispose(); $sha.Dispose() }
    }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath 'build/artifact-sha256.json' -Encoding utf8
    Write-Output 'LED baseline compiled. This script does not flash a device.'
} finally {
    Pop-Location
}
