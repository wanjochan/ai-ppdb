#powershell .\build_all.ps1
$ErrorActionPreference = "Stop"
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location "$scriptPath"

$tests = @(
    "test42",
    "sync_locked",
    "sync_lockfree",
    "skiplist_locked",
    "skiplist_lockfree",
    "memtable_locked",
    "memtable_lockfree",
    "sharded_locked",
    "sharded_lockfree"
)

foreach ($test in $tests) {
    Write-Host "`n=== Running test: $test ==="
    Write-Host "Executing build.bat $test..."
    cmd /c build.bat $test
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Test $test failed with exit code: $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    Write-Host "Test $test completed successfully"
    #Write-Host "Press Enter to continue to next test..."
    #Read-Host
} 