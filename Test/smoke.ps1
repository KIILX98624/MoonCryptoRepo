$ErrorActionPreference = "Stop"

function Run-FailFixture([string]$path, [string]$msg) {
    $proc = Start-Process -FilePath $binary -ArgumentList "`"$path`"" -NoNewWindow -Wait -PassThru -RedirectStandardError "$env:TEMP\moon_err.log" -RedirectStandardOutput "$env:TEMP\moon_out.log"
    if ($proc.ExitCode -eq 0) { throw $msg }
    $null = $proc.ExitCode
}

$root = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $root "Test\mooncyt-smoke.exe"

& g++ -std=c++20 -Wall -Wextra -Wpedantic -Isrc src/main.cpp src/project.cpp src/lexer.cpp src/ast.cpp -lbcrypt -o $binary
if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar o MoonCrypto" }

$control = (& $binary "$PSScriptRoot\fixtures\control.mncyt" | Out-String).Trim()
$expectedControl = @("if-ok", "while-0", "while-1", "while-2", "for-0", "for-1") -join [Environment]::NewLine
if ($control -ne $expectedControl) { throw "Controle de fluxo falhou:`n$control" }

$crypto = (& $binary "$PSScriptRoot\fixtures\crypto.mncyt" | Out-String).Trim()
if ($crypto -ne "sha-84cfbae28e7ce7e83d00e8ff8b4278c2385dbeecf7149b89287d94f8dfd74418") { throw "SHA-256 falhou: $crypto" }

$bytes = (& $binary "$PSScriptRoot\fixtures\bytes.mncyt" | Out-String).Trim()
if ($bytes -notmatch "byte-0=1" -or $bytes -notmatch "byte-2=255") { throw "Bytes falhou: $bytes" }

$json = (& $binary "$PSScriptRoot\fixtures\json.mncyt" | Out-String).Trim()
if ($json -notmatch "json-active=true" -or
    $json -notmatch "json-name=MoonCrypto" -or
    $json -notmatch "json-version=2" -or
    $json -notmatch 'json-array=\[7,9\]' -or
    $json -notmatch 'json-object=\{"lista":\[7,9\],"nome":"MoonCrypto","versao":2\}') {
    throw "JSON module falhou: $json"
}

$bool = (& $binary "$PSScriptRoot\fixtures\bool.mncyt" | Out-String).Trim()
if ($bool -ne "bool-ok") { throw "Bool falhou: $bool" }

$result = (& $binary "$PSScriptRoot\fixtures\result.mncyt" | Out-String).Trim()
if ($result -notmatch "value=42" -or $result -notmatch "error=falhou") { throw "Result falhou: $result" }

$bitwise = (& $binary "$PSScriptRoot\fixtures\bitwise.mncyt" | Out-String).Trim()
if ($bitwise -ne "bits=10") { throw "Bitwise falhou: $bitwise" }

$random = (& $binary "$PSScriptRoot\fixtures\random.mncyt" | Out-String).Trim()
if ($random -notmatch '^[0-9a-f]{16}$') { throw "random_bytes falhou: $random" }

$hmac = (& $binary "$PSScriptRoot\fixtures\hmac.mncyt" | Out-String).Trim()
if ($hmac -ne "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") { throw "HMAC-SHA-256 falhou: $hmac" }

$types = (& $binary "$PSScriptRoot\fixtures\types.mncyt" | Out-String).Trim()
if ($types -ne "limit=10") { throw "const/struct/class falhou: $types" }

$ast = (& $binary ast "$PSScriptRoot\fixtures\control.mncyt" | Out-String).Trim()
if ($ast -notmatch "fn main\[\] -> Int32") { throw "AST de função falhou: $ast" }

$elseIf = (& $binary "$PSScriptRoot\fixtures\else_if.mncyt" | Out-String).Trim()
if ($elseIf -ne "two") { throw "else if falhou: $elseIf" }

Run-FailFixture "$PSScriptRoot\fixtures\const_fail.mncyt" "Reatribuição de const foi aceita"
Run-FailFixture "$PSScriptRoot\fixtures\overflow_fail.mncyt" "Overflow de inteiro foi aceito"
Run-FailFixture "$PSScriptRoot\fixtures\type_fail.mncyt" "Valor incompatível foi aceito"
Run-FailFixture "$PSScriptRoot\fixtures\integer_range_fail.mncyt" "Valor fora da faixa foi aceito"
Run-FailFixture "$PSScriptRoot\fixtures\invalid_function.mncyt" "Função malformada foi aceita"
Run-FailFixture "$PSScriptRoot\fixtures\duplicate_function.mncyt" "Funções duplicadas foram aceitas"

$procExit = Start-Process -FilePath $binary -ArgumentList "`"$PSScriptRoot\fixtures\exit_code.mncyt`"" -NoNewWindow -Wait -PassThru -RedirectStandardError "$env:TEMP\moon_err.log" -RedirectStandardOutput "$env:TEMP\moon_out.log"
if ($procExit.ExitCode -ne 7) { throw "Código de saída de main falhou: $($procExit.ExitCode)" }

Run-FailFixture "$PSScriptRoot\fixtures\old_call_syntax.mncyt" "Sintaxe antiga foi aceita"

Write-Host "MoonCrypto smoke tests: OK"
