# 生成 app.ico:摇杆图案工业风图标(多尺寸 PNG-in-ICO)
Add-Type -AssemblyName System.Drawing
$dir = "e:\traestudy\Voicejoystick\mawinHcom-ui"
$sizes = @(256,128,64,48,32,24,16)

function New-IconPng([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $u = [double]($s / 256.0)
    $cx = [double]($s / 2.0); $cy = [double]($s / 2.0)

    # 底盘:深灰圆 + 亮灰描边
    $bg = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0x26,0x26,0x26))
    $r = [double](100*$u)
    $g.FillEllipse($bg, [single]($cx-$r), [single]($cy-$r), [single](2*$r), [single](2*$r))
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0x5a,0x5a,0x5a), [single](4*$u))
    $g.DrawEllipse($pen, [single]($cx-$r), [single]($cy-$r), [single](2*$r), [single](2*$r))

    # 8 方向刻度
    $tick = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0xb0,0xb0,0xb0), [single](4*$u))
    for ($i=0; $i -lt 8; $i++) {
        $a = $i * [Math]::PI / 4
        $len = 0; if ($i % 2 -eq 0) { $len = 20 } else { $len = 11 }
        $r1 = [double](104*$u); $r2 = [double]((104+$len)*$u)
        $g.DrawLine($tick,
            [single]($cx + $r1*[Math]::Cos($a)), [single]($cy + $r1*[Math]::Sin($a)),
            [single]($cx + $r2*[Math]::Cos($a)), [single]($cy + $r2*[Math]::Sin($a)))
    }

    # 十字准线(灰)
    $cross = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0x77,0x77,0x77), [single](3*$u))
    $g1 = [double](30*$u); $g2 = [double](78*$u)
    $g.DrawLine($cross, [single]($cx-$g2), [single]$cy, [single]($cx-$g1), [single]$cy)
    $g.DrawLine($cross, [single]($cx+$g1), [single]$cy, [single]($cx+$g2), [single]$cy)
    $g.DrawLine($cross, [single]$cx, [single]($cy-$g2), [single]$cx, [single]($cy-$g1))
    $g.DrawLine($cross, [single]$cx, [single]($cy+$g1), [single]$cx, [single]($cy+$g2))

    # 指示线:绿色,中心向上(摇杆前推)
    $green = [System.Drawing.Color]::FromArgb(0x3d,0x8b,0x40)
    $ind = New-Object System.Drawing.Pen($green, [single](6*$u))
    $g.DrawLine($ind, [single]$cx, [single]$cy, [single]$cx, [single]($cy-70*$u))

    # 旋钮:深灰圆 + 亮边
    $knob = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0x3a,0x3a,0x3a))
    $kr = [double](46*$u)
    $g.FillEllipse($knob, [single]($cx-$kr), [single]($cy-$kr), [single](2*$kr), [single](2*$kr))
    $kpen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(0x90,0x90,0x90), [single](3*$u))
    $g.DrawEllipse($kpen, [single]($cx-$kr), [single]($cy-$kr), [single](2*$kr), [single](2*$kr))

    # 中心绿点
    $dot = New-Object System.Drawing.SolidBrush($green)
    $dr = [double](10*$u)
    $g.FillEllipse($dot, [single]($cx-$dr), [single]($cy-$dr), [single](2*$dr), [single](2*$dr))

    $g.Dispose()
    return $bmp
}

$pngs = @{}
foreach ($s in $sizes) {
    $bmp = New-IconPng $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs[$s] = $ms.ToArray()
    $ms.Dispose(); $bmp.Dispose()
}

# 拼 ICO:ICONDIR + N*ICONDIRENTRY + PNG 数据
$fs = [System.IO.File]::Create("$dir\app.ico")
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
foreach ($s in $sizes) {
    $b = $pngs[$s]
    $dim = $s; if ($s -ge 256) { $dim = 0 }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$b.Length); $bw.Write([uint32]$offset)
    $offset += $b.Length
}
foreach ($s in $sizes) { $bw.Write($pngs[$s]) }
$bw.Flush(); $bw.Close()
Write-Host "app.ico OK: $((Get-Item "$dir\app.ico").Length) bytes"
