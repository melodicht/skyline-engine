struct PointDepthFixed {
    lightPos : vec3<f32>,
    farPlane : f32
}

struct ObjData {
    transform : mat4x4<f32>,
    normMat : mat4x4<f32>,
    color : vec4<f32>
}

struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(2) normal : vec3<f32>,
    @builtin(instance_index) instance: u32, // Represents which instance within objStore to pull data from
}

// Default pipeline for color pass 
struct PointDepthPassVertexOut {
    @builtin(position) position: vec4<f32>,
    @location(1) worldPos: vec4<f32>
}
@binding(0) @group(0) var<uniform> cameraSpace : mat4x4<f32>;

@binding(1) @group(0) var<storage, read> objStore : array<ObjData>; 

@binding(2) @group(0) var<uniform> fixedPointData : PointDepthFixed;

// Depth pass pipeline
@vertex
fn vtxMain(in : VertexIn) -> PointDepthPassVertexOut {
  var worldPos : vec4<f32> = objStore[in.instance].transform * vec4<f32>(in.position,1);

  var out : PointDepthPassVertexOut;
  out.position = cameraSpace * worldPos;
  out.worldPos = worldPos;
  return out;
}

struct FragOut {
    @location(0) dummy : vec4<f32>,
    @builtin(frag_depth) depth : f32
}

// Actually passes in
@fragment
fn fsMain(in : PointDepthPassVertexOut) -> FragOut {
    var out : FragOut;
    var relativeLightToFrag : vec3<f32> = (in.worldPos.xyz/in.worldPos.w) - fixedPointData.lightPos;
    var pixDepth : f32 = length(relativeLightToFrag) / fixedPointData.farPlane;
    out.depth = pixDepth + fwidth(pixDepth);
    return out;
}


