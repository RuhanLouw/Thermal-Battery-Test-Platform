$ErrorActionPreference = 'Stop'

$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$tests = @('test_ds18b20', 'test_register_contract')
foreach ($test in $tests) {
    $source = Join-Path $PSScriptRoot ($test + '.c')
    $binary = Join-Path $buildDir ($test + '.exe')
    & gcc -std=c11 -Wall -Wextra -Werror $source -o $binary
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $test" }
    & $binary
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
    Write-Host "PASS $test"
}

$uartSource = Get-Content -Raw (Join-Path $PSScriptRoot '..\mcc_generated_files\uart\src\usart1.c')
if ($uartSource -match 'valChecker') {
    throw 'Unsafe USART1 receive debug buffer is present'
}
Write-Host 'PASS usart1_receive_bounds'
