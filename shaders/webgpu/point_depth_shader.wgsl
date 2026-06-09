// The instance < point light index | point light face | instance >
override instanceBitMask: u32 = 0x000FFFFF;
override instanceStoreShift: u32 = 20;

override faceBitMask: u32 = 0x07;
override faceStoreShift: u32 = 3;

struct DynamicShadowedPointLight {
    diffuse : vec3<f32>,
    radius : f32,
    specular : vec3<f32>,
    falloff : f32,
    position : vec3<f32>,
    padding : f32
}


struct ObjData {
    transform: mat4x4<f32>,
    normMat: mat4x4<f32>,
    color: vec4<f32>
}

struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(2) normal: vec3<f32>,
    @builtin(instance_index) instance_buffer: u32, // Represents which instance within objStore to pull data from
}

// Default pipeline for color pass 
struct PointDepthPassVertexOut {
    @builtin(position) position: vec4<f32>,
    @location(1) worldPos: vec4<f32>,
    @location(2) @interpolate(flat) pointLightIndex: u32
}

@binding(0) @group(0) var<storage, read> pointLightSpaces: array<array<mat4x4<f32>, 6>>;

@binding(1) @group(0) var<storage, read> pointLightUniforms: array<DynamicShadowedPointLight>;

@binding(2) @group(0) var<storage, read> objStore: array<ObjData>; 

// Depth pass pipeline
@vertex
fn vtxMain(in : VertexIn) -> PointDepthPassVertexOut {
  var out : PointDepthPassVertexOut;
  // Processes instance buffer
  var faceIndex : u32 = in.instance_buffer >> instanceStoreShift;
  var instance : u32 = in.instance_buffer & instanceBitMask;
  var pointLightIndex : u32 = faceIndex  >> faceStoreShift;
  faceIndex = faceIndex & faceBitMask;

  var worldPos : vec4<f32> = objStore[instance].transform * vec4<f32>(in.position,1);

  out.position = pointLightSpaces[pointLightIndex][faceIndex] * worldPos;
  out.worldPos = worldPos;
  out.pointLightIndex = pointLightIndex;
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
    var relativeLightToFrag : vec3<f32> = (in.worldPos.xyz/in.worldPos.w) - pointLightUniforms[in.pointLightIndex].position;
    var pixDepth : f32 = length(relativeLightToFrag) / pointLightUniforms[in.pointLightIndex].radius;
    out.depth = pixDepth + fwidth(pixDepth) * 3.0; // For now multiplying slope by 3 is standard across lights, TODO: make less arbitrary
    return out;
}


