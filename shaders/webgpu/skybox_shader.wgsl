struct SkyboxIn {
    @location(0) position: vec3<f32>
}

@binding(0) @group(0) var<uniform> projMat : mat4x4<f32>;

@binding(1) @group(0) var<uniform> rotViewMat : mat4x4<f32>

@binding(1) @group(0) skyboxTexture: texture_cube<f32>

@vertex
fn vtxMain(in : SkyboxIn) -> @builtin(position) vec4<f32> {
  return  projMat * rotViewMat * position;
}

@fragment
fn fsMain(in : vec4<f32>) -> @location(0) vec4<f32> {
    return out;
}