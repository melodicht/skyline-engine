// Represents the data that differentiates each instance of the same mesh
struct ObjData {
    transform : mat4x4<f32>,
    normMat : mat4x4<f32>,
    color : vec4<f32>
}

@binding(0) @group(0) var<uniform> cameraSpace : mat4x4<f32>;

@binding(1) @group(0) var<storage> objStore : array<ObjData>; 


struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(2) normal : vec3<f32>,
    @builtin(instance_index) instance: u32, // Represents which instance within objStore to pull data from
}

// Depth pass pipeline
@vertex
fn vtxMain(in : VertexIn) -> @builtin(position) vec4<f32> {
  var worldPos = objStore[in.instance].transform * vec4<f32>(in.position,1);
  return cameraSpace * worldPos;
}