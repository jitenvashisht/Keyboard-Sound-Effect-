param(
    [Parameter(Mandatory=$true)][string]$Key,
    [Parameter(Mandatory=$true)][string]$File
)

# friendly name -> VK code
$lookup = @{
    "Space" = 32
    "Enter" = 13
    "Backspace" = 8
    "Tab" = 9
}

# A-Z
for ($c = 65; $c -le 90; $c++) { $lookup[[char]$c] = $c }
# 0-9
for ($c = 48; $c -le 57; $c++) { $lookup[[char]$c] = $c }
# F1-F12
for ($i = 1; $i -le 12; $i++) { $lookup["F$i"] = 0x70 + ($i - 1) }

$keyTrim = $Key.Trim()
if ($lookup.ContainsKey($keyTrim)) { $vk = $lookup[$keyTrim] }
elseif ($keyTrim -match '^[0-9]+$') { $vk = [int]$keyTrim }
else { Write-Error "Unknown key name: $Key"; exit 1 }

# load existing keymap
$ht = @{}
if (Test-Path keymap.json) {
    $json = Get-Content keymap.json -Raw
    if ($json.Trim().Length -gt 0) {
        $obj = ConvertFrom-Json $json
        foreach ($p in $obj.PSObject.Properties) { $ht[$p.Name] = $p.Value }
    }
}

$ht[[string]$vk] = $File

# write JSON with stable formatting
$out = "{`n"
$keys = $ht.Keys | Sort-Object {[int]$_}
for ($i = 0; $i -lt $keys.Count; $i++) {
    $k = $keys[$i]
    $v = $ht[$k] -replace '\\','\\\\' -replace '"','\"'
    $comma = (if ($i -lt $keys.Count - 1) { ',' } else { '' })
    $out += "  \"$k\": \"$v\"$comma`n"
}
$out += "}`n"
Set-Content -Path keymap.json -Value $out -Encoding UTF8
Write-Host "Mapped $Key -> $File (VK=$vk)"
