struct VertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) clipPos: vec4<f32>
}

@binding(0) @group(0) var<uniform> cameraSpaceInverse : mat4x4<f32>;

@binding(1) @group(0) var skyboxTexture: texture_cube<f32>;

@binding(2) @group(0) var skyboxSampler: sampler;

// Much adapted from https://webgpufundamentals.org/webgpu/lessons/webgpu-skybox.html
@vertex
fn vtxMain(@builtin(vertex_index) vert_index: u32) -> VertexOut {
  // All encompassing triangle coords
  let pos = array<vec2<f32>,3>( vec2<f32>(-1, 3),vec2<f32>(-1,-1), vec2<f32>( 3,-1));

  var out : VertexOut;
  out.position = vec4<f32>(pos[vert_index], 1, 1);
  out.clipPos = out.position;
  return out;
}

@fragment
fn fsMain(in : VertexOut) -> @location(0) vec4<f32> {
    let worldPos = cameraSpaceInverse * in.clipPos;
    return textureSample(skyboxTexture, skyboxSampler, (worldPos.xzy / worldPos.w) * vec3<f32>(1, 1, -1));
}