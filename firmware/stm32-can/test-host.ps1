param([string]$Clang = 'clang.exe')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & $Clang -std=c99 -O2 -Wall -Wextra -Werror -pedantic ../app_protocol.c ../app_heartbeat.c ../native_test.c -o build/test_protocol.exe
    if ($LASTEXITCODE -ne 0) { throw 'Protocol host compilation failed' }
    & ./build/test_protocol.exe
    if ($LASTEXITCODE -ne 0) { throw 'Protocol host tests failed' }
    & $Clang -std=c99 -O2 -Wall -Wextra -Werror -Itests/stubs can_app.c ../app_protocol.c ../app_heartbeat.c tests/test_can_app.c -o build/test_can_app.exe
    if ($LASTEXITCODE -ne 0) { throw 'Adapter host compilation failed' }
    & ./build/test_can_app.exe
    if ($LASTEXITCODE -ne 0) { throw 'Adapter host tests failed' }
} finally { Pop-Location }
