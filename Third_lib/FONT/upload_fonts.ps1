param(
    [string]$Port = "",
    [string]$FontFile = "",
    [int]$BaudRate = 115200
)

$ErrorActionPreference = "Stop"
$blockSize = 240
$fontFiles = @(
    @{ Id = 0; Name = "UNIGBK.BIN"; Size = 174344 },
    @{ Id = 1; Name = "GBK12.FON";  Size = 574560 },
    @{ Id = 2; Name = "GBK16.FON";  Size = 766080 },
    @{ Id = 3; Name = "GBK24.FON";  Size = 1723680 }
)

$script:Crc32Polynomial = [uint64]3988292384
$script:Crc32Mask = [uint64]4294967295
$script:Crc32Table = New-Object uint32[] 256
for ($tableIndex = 0; $tableIndex -lt 256; $tableIndex++) {
    [uint64]$tableValue = [uint64]$tableIndex
    for ($tableBit = 0; $tableBit -lt 8; $tableBit++) {
        if (($tableValue -band [uint64]1) -ne 0) {
            $tableValue = (($tableValue -shr 1) -bxor $script:Crc32Polynomial) -band $script:Crc32Mask
        } else {
            $tableValue = ($tableValue -shr 1) -band $script:Crc32Mask
        }
    }
    $script:Crc32Table[$tableIndex] = [uint32]$tableValue
}

function Get-Crc32([byte[]]$Data) {
    [uint64]$crc = $script:Crc32Mask
    foreach ($value in $Data) {
        $tableIndex = [int](($crc -bxor [uint64]$value) -band [uint64]255)
        $crc = (($crc -shr 8) -bxor [uint64]$script:Crc32Table[$tableIndex]) -band $script:Crc32Mask
    }
    return [uint32](($crc -bxor $script:Crc32Mask) -band $script:Crc32Mask)
}

function Read-Response([System.IO.Ports.SerialPort]$Serial) {
    return [char]$Serial.ReadByte()
}

function Get-DeviceError([char]$Response) {
    switch ($Response) {
        'E' { return "Sector erase failed" }
        'W' { return "Flash page program failed" }
        'H' { return "Invalid file header or file size" }
        'S' { return "Whole-file CRC32 did not match" }
        'T' { return "Device receive timeout" }
        default { return "Unknown device response: $Response" }
    }
}

function Wait-Erase([System.IO.Ports.SerialPort]$Serial) {
    while ($true) {
        $response = Read-Response $Serial
        if ($response -eq '.') {
            Write-Host -NoNewline "."
            continue
        }
        if ($response -eq 'R') {
            Write-Host " erase complete"
            return
        }
        throw (Get-DeviceError $response)
    }
}

function Select-Files {
    Write-Host "1. UNIGBK.BIN"
    Write-Host "2. GBK12.FON"
    Write-Host "3. GBK16.FON"
    Write-Host "4. GBK24.FON"
    Write-Host "5. All files"
    $choice = Read-Host "Select font file"
    if ($choice -eq "5") {
        return $fontFiles
    }
    $index = [int]$choice - 1
    if ($index -lt 0 -or $index -ge $fontFiles.Count) {
        throw "Invalid font selection"
    }
    return @($fontFiles[$index])
}

function Resolve-FontFiles([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return Select-Files
    }
    $name = [System.IO.Path]::GetFileName($Path)
    $font = $fontFiles | Where-Object { $_.Name -ieq $name }
    if ($null -eq $font) {
        throw "The file name must be one of UNIGBK.BIN, GBK12.FON, GBK16.FON or GBK24.FON"
    }
    return @(@{ Id = $font.Id; Name = $font.Name; Size = $font.Size; Path = $Path })
}

function Send-FontFile(
    [System.IO.Ports.SerialPort]$Serial,
    [hashtable]$Font,
    [string]$BaseDir
) {
    $path = if ($Font.ContainsKey("Path")) { $Font.Path } else { Join-Path $BaseDir $Font.Name }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Font file not found: $path"
    }
    [byte[]]$data = [System.IO.File]::ReadAllBytes($path)
    if ($data.Length -ne $Font.Size) {
        throw "$($Font.Name) has invalid size $($data.Length), expected $($Font.Size)"
    }

    [uint32]$fileCrc = Get-Crc32 $data
    Write-Host ("Writing {0}, size {1}, CRC32 0x{2:X8}" -f $Font.Name, $data.Length, $fileCrc)
    [byte[]]$magic = [System.Text.Encoding]::ASCII.GetBytes("FONT")
    $Serial.Write($magic, 0, $magic.Length)
    if ((Read-Response $Serial) -ne 'K') {
        throw "The device did not accept the font download request"
    }

    [byte[]]$header = New-Object byte[] 9
    $header[0] = [byte]$Font.Id
    [Array]::Copy([BitConverter]::GetBytes([uint32]$data.Length), 0, $header, 1, 4)
    [Array]::Copy([BitConverter]::GetBytes($fileCrc), 0, $header, 5, 4)
    $Serial.Write($header, 0, $header.Length)
    Wait-Erase $Serial

    $offset = 0
    while ($offset -lt $data.Length) {
        $length = [Math]::Min($blockSize, $data.Length - $offset)
        [byte[]]$block = New-Object byte[] $length
        [Array]::Copy($data, $offset, $block, 0, $length)
        [uint32]$blockCrc = Get-Crc32 $block
        [byte[]]$crcBytes = [BitConverter]::GetBytes($blockCrc)
        $retry = 0
        while ($true) {
            $Serial.Write($block, 0, $block.Length)
            $Serial.Write($crcBytes, 0, $crcBytes.Length)
            $response = Read-Response $Serial
            if ($response -eq 'K' -or $response -eq 'D') {
                break
            }
            if ($response -eq 'C' -and $retry -lt 3) {
                $retry++
                continue
            }
            throw (Get-DeviceError $response)
        }
        $offset += $length
        Write-Progress -Activity "Writing $($Font.Name)" -Status "$offset / $($data.Length)" -PercentComplete ([int](100 * $offset / $data.Length))
    }
    Write-Progress -Activity "Writing $($Font.Name)" -Completed
    Write-Host "$($Font.Name) write and CRC32 verification succeeded"
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = Read-Host "Serial port, for example COM3"
}
if ($Port -match '^\d+$') {
    $Port = "COM$Port"
}
$files = Resolve-FontFiles $FontFile
$baseDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate,
    [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 600000
$serial.WriteTimeout = 10000
$serial.Handshake = [System.IO.Ports.Handshake]::None

try {
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()
    foreach ($font in $files) {
        Send-FontFile $serial $font $baseDir
        Start-Sleep -Milliseconds 100
    }
    Write-Host "All selected font files have been written."
}
finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
