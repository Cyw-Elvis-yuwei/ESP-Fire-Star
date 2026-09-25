param([string]$ArmCompiler = 'C:\Keil_v5\ARM\ARMCC\bin')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    # Compile every listed source; no stale object discovery or device access.
    $includes = @('.', '..', 'vendor/Libraries/CMSIS/Include',
        'vendor/Libraries/CMSIS/Device/ST/STM32F1xx/Include',
        'vendor/Libraries/STM32F1xx_HAL_Driver/Inc')
    $sources = @('main.c','can_app.c','../app_protocol.c','../app_heartbeat.c',
        'vendor/User/system_stm32f1xx.c')
    foreach ($module in @('','can','cortex','rcc','rcc_ex','gpio','gpio_ex','flash','flash_ex','pwr','dma')) {
        $suffix = if ($module) { '_' + $module } else { '' }
        $sources += 'vendor/Libraries/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal' + $suffix + '.c'
    }
    $objects = @()
    foreach ($source in $sources) {
        $object = 'build/' + [IO.Path]::GetFileNameWithoutExtension($source) + '.o'
        $arguments = @('--cpu','Cortex-M3','--c99','-O1','--debug','--apcs=interwork',
            '--split_sections','-DSTM32F103xE','-DUSE_HAL_DRIVER')
        foreach ($include in $includes) { $arguments += @('-I',$include) }
        $arguments += @('-c',$source,'-o',$object)
        Write-Output ('Compile ' + $source)
        & (Join-Path $ArmCompiler 'armcc.exe') @arguments
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source ($LASTEXITCODE)" }
        $objects += $object
    }
    & (Join-Path $ArmCompiler 'armasm.exe') --cpu Cortex-M3 --debug --apcs=interwork 'vendor/Libraries/CMSIS/Device/ST/STM32F1xx/Source/Templates/arm/startup_stm32f103xe.s' -o build/startup.o
    if ($LASTEXITCODE -ne 0) { throw 'Assembly failed' }
    & (Join-Path $ArmCompiler 'armlink.exe') build/startup.o @objects --scatter canbench.sct --entry Reset_Handler --map --list build/canbench.map --info sizes --output build/canbench.axf
    if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
    foreach ($format in @('i32','bin')) {
        $ext = if ($format -eq 'i32') { 'hex' } else { 'bin' }
        & (Join-Path $ArmCompiler 'fromelf.exe') ('--' + $format) --output ('build/canbench.' + $ext) build/canbench.axf
        if ($LASTEXITCODE -ne 0) { throw 'Image conversion failed' }
    }
    $hashes = foreach ($name in @('canbench.axf','canbench.hex','canbench.bin')) {
        $path = Join-Path $PSScriptRoot ('build/' + $name)
        $sha = [Security.Cryptography.SHA256]::Create()
        $stream = [IO.File]::OpenRead($path)
        try {
            [pscustomobject]@{File=$name; Bytes=$stream.Length; SHA256=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}
        } finally { $stream.Dispose(); $sha.Dispose() }
    }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath build/artifact-sha256.json -Encoding UTF8
    Write-Output 'CAN ARM candidate built. No device has been programmed.'
} finally { Pop-Location }
