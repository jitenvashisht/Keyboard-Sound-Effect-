function New-Wav($file, $freq, $duration, $amp = 0.6) {
    $sampleRate = 44100
    $bits = 16
    $channels = 1
    $bytesPerSample = $bits / 8
    $samples = [int]($sampleRate * $duration)
    $dataSize = $samples * $channels * $bytesPerSample
    $byteRate = $sampleRate * $channels * $bytesPerSample
    $maxAmp = [math]::Pow(2, $bits - 1) - 1

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
    $bw.Write([int](36 + $dataSize))
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
    $bw.Write([int]16)
    $bw.Write([Int16]1)
    $bw.Write([Int16]$channels)
    $bw.Write([int]$sampleRate)
    $bw.Write([int]$byteRate)
    $bw.Write([Int16]($channels * $bytesPerSample))
    $bw.Write([Int16]$bits)
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $bw.Write([int]$dataSize)

    for ($i = 0; $i -lt $samples; $i++) {
        $t = $i / $sampleRate
        # soft envelope: quick exponential decay + linear fade
        $decay = [math]::Exp(-30 * $t) # fast decay for click
        $env = $decay * (1.0 - ($t / $duration))
        $sampleVal = $maxAmp * $amp * $env * [math]::Sin(2 * [math]::PI * $freq * $t)
        $sample = [int]([math]::Round($sampleVal))
        $bw.Write([Int16]$sample)
    }

    $bw.Flush()
    [System.IO.File]::WriteAllBytes($file, $ms.ToArray())
    $bw.Close()
    $ms.Close()
}

Write-Host "Generating sample WAV files..."
New-Wav "click.wav" 2000 0.03
New-Wav "space.wav" 1000 0.06
New-Wav "enter.wav" 600 0.08
New-Wav "backspace.wav" 400 0.06
Write-Host "Done. Files: click.wav, space.wav, enter.wav, backspace.wav"
