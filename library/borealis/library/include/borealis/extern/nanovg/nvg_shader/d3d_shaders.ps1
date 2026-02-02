
$files = "D3D11PixelShader", "D3D11PixelShaderAA", "D3D11VertexShader"
$types = "ps_5_0", "ps_5_0", "vs_5_0"

for($i=0; $i -lt $files.Length; $i++)
{
    $file = $files[$i]
    $type = $types[$i]
    echo "${file}.hlsl generate to ${file}.h"
    fxc /Fh "${file}.h" /T $type /E "${file}_Main" "${file}.hlsl"
}
