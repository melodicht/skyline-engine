// TODO: Better explore alternatives in alignment, maybe we could merge in different variables to reduce waste due to alignment

// Math functionality
struct orthonormalBasis {
    up: vec3<f32>,
    left: vec3<f32>
}

// Uses normal vector and find two arbitrary normal vectors perpidicular to eachother and the base normal
// Taken from https://graphics.pixar.com/library/OrthonormalB/paper.pdf
fn getOrthonormalBasis(normalForward: vec3<f32>) -> orthonormalBasis {
    var sign: f32 = select(-1.0, 1.0, normalForward.z >= 0);
    var temp1: f32 = -1.0 / (sign + normalForward.z);
    var temp2: f32 = normalForward.x * normalForward.y * temp1;
    
    var output: orthonormalBasis;
    output.up = vec3<f32>(
        1.0 + sign * normalForward.x * normalForward.x * temp1, 
        sign * temp2, 
        -sign * normalForward.x);
    output.left = vec3<f32>(
        temp2, 
        sign + normalForward.y * normalForward.y * temp1, 
        -normalForward.y);
    return output;
}



// Represents fixed length color pass data
struct ColorPassFixedData {
    // Camera Data
    combinedMat : mat4x4<f32>,
    viewMat : mat4x4<f32>,
    projMat : mat4x4<f32>,
    pos: vec3<f32>,
    // Light Data
    dirLightAmount: u32,
    pointLightAmount: u32,
    spotLightAmount: u32,
    dirLightCascadeCount: u32,
    // Pass Data
    dirLightMapDimension: u32,
    pointLightMapDimension: u32,
    padding1: u32,
    padding2: u32,
    pcsRange: u32
}

// Represents the data that differentiates each instance of the same mesh
struct ObjData {
    transform : mat4x4<f32>,
    normMat : mat4x4<f32>,
    color : vec4<f32>
}

// TODO: Create specified ambient lighting

// Represents a single directional light with shadows and a potential to change pos/dir over time.
struct DynamicShadowedDirLight {
    diffuse : vec3<f32>,
    padding : f32, // Fill with useful stuff later
    specular : vec3<f32>,
    padding2 : f32, // Fill with useful stuff later
    direction : vec3<f32>,
    intensity : f32
}

struct DynamicShadowedPointLight {
    diffuse : vec3<f32>,
    radius : f32,
    specular : vec3<f32>,
    falloff : f32,
    position : vec3<f32>,
    padding : f32
}

struct DynamicShadowedSpotLight {
    diffuse : vec3<f32>,
    penumbraCutoff : f32,
    specular : vec3<f32>,
    outerCutoff : f32,
    position : vec3<f32>,
    padding : f32,
    direction : vec3<f32>,
    padding2 : f32
}

@binding(0) @group(0) var<uniform> fixedData : ColorPassFixedData;

@binding(1) @group(0) var<storage, read> objStore : array<ObjData>; 

@binding(2) @group(0) var<storage, read> dynamicShadowedDirLightStore : array<DynamicShadowedDirLight>;

@binding(3) @group(0) var<storage, read> dynamicShadowedPointLightStore : array<DynamicShadowedPointLight>;

@binding(4) @group(0) var<storage, read> dynamicShadowedSpotLightStore : array<DynamicShadowedSpotLight>;

@binding(5) @group(0) var<storage, read> dynamicLightsSpacesStore : array<mat4x4<f32>>;

@binding(6) @group(0) var dynamicShadowedDirLightMap: texture_depth_2d_array;

@binding(7) @group(0) var dynamicShadowedPointLightMap: texture_depth_cube_array;

@binding(8) @group(0) var<storage, read> dynamicShadowedDirLightCascadeRatios : array<f32>;

@binding(9) @group(0) var shadowMapSampler : sampler_comparison;


struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(1) uvX : f32,
    @location(2) normal : vec3<f32>,
    @location(3) uvY : f32,
    @builtin(instance_index) instance: u32, // Represents which instance within objStore to pull data from
}

// Collects translation from a mat4x4 
fn getTranslate(in : mat4x4<f32>) -> vec3<f32> {
    return vec3<f32>(in[3][0], in[3][1], in[3][2]);
}

// Default pipeline for color pass 
struct ColorPassVertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) fragToCamPos: vec3<f32>,
    @location(1) color: vec3<f32>,
    @location(2) normal: vec3<f32>,
    @location(3) worldPos: vec4<f32>
}

@vertex
fn vtxMain(in : VertexIn) -> ColorPassVertexOut {
  var out : ColorPassVertexOut;

  var worldPos = objStore[in.instance].transform * vec4<f32>(in.position,1);

  out.worldPos = worldPos;
  out.position = fixedData.combinedMat * worldPos;
  out.color = objStore[in.instance].color.xyz;
  out.fragToCamPos = fixedData.pos - worldPos.xyz;
  var nMat = objStore[in.instance].normMat;
  out.normal = normalize(mat3x3(nMat[0].xyz, nMat[1].xyz, nMat[2].xyz) * in.normal);

  return out;
}

fn pcsCompare(
    texture: texture_depth_2d_array, 
    sampler: sampler_comparison, 
    texturePos : vec2<f32>, 
    index: u32, 
    depth: f32, 
    pcsRange : u32,
    mapDimension: u32) -> f32 {
    // Sets up sample location
    var unit: f32 = 1.0/f32(mapDimension);
    var trueRangeHalf: f32 = unit * (f32(pcsRange - 1) /2.0);
    var base: vec2<f32> = texturePos - vec2<f32>(trueRangeHalf,trueRangeHalf);

    // Finds PCS percentage
    var sum: f32 = 0;
    for (var xIter : u32 = 0 ; xIter < pcsRange ; xIter++) {
        for (var yIter : u32 = 0 ; yIter < pcsRange ; yIter++) {
            var newPos: vec2<f32> = base + vec2<f32>(f32(xIter)*unit, f32(yIter)*unit);
            sum += textureSampleCompare(texture, sampler, newPos, index, depth);
        }
    }
    return sum/f32(pcsRange*pcsRange);
}

// TODO: Implement blinn-phong instead of just phong
@fragment
fn fsMain(in : ColorPassVertexOut) -> @location(0) vec4<f32>  {
    // TODO: Set ambient lighting to be specified

    // Sets ambient lighting
    var ambientIntensity : f32 = 0.25;
    var ambient : vec3<f32> = in.color * ambientIntensity;

    var viewDir : vec3<f32> = normalize(in.fragToCamPos);
    var fragDepth = (fixedData.viewMat * in.worldPos).z;
    var overallLight : vec3<f32> = vec3<f32>(0, 0, 0);

    for (var dirIter : u32 = 0 ; dirIter < fixedData.dirLightAmount ; dirIter++) {
        // Checks what cascade it the specific fragment should reference
        var cascadeCheck : u32 = 0;
        while (cascadeCheck < fixedData.dirLightCascadeCount) {
            if (dynamicShadowedDirLightCascadeRatios[cascadeCheck] > fragDepth) {
                break;
            }
            cascadeCheck++;
        }

        // Checks if location has been covered by light
        var lightSpaceIdx : u32 = dirIter + cascadeCheck * fixedData.dirLightAmount;
        var lightSpacePosition : vec4<f32> = dynamicLightsSpacesStore[lightSpaceIdx] * (in.worldPos);
        lightSpacePosition = lightSpacePosition / lightSpacePosition.w;
        var texturePosition: vec3<f32> = vec3<f32>((lightSpacePosition.x * 0.5) + 0.5, (lightSpacePosition.y * -0.5) + 0.5, lightSpacePosition.z);
        var lightsUncovered : f32  = pcsCompare(
            dynamicShadowedDirLightMap, 
            shadowMapSampler, 
            texturePosition.xy, 
            cascadeCheck, 
            texturePosition.z,
            fixedData.pcsRange,
            fixedData.dirLightMapDimension);

        // Handles Phong lighting
        var singleLight : vec3<f32> = vec3<f32>(0, 0, 0);

        // Adds on diffuse lighting to light
        var diffuseIntensity : f32 = max(dot(in.normal, -dynamicShadowedDirLightStore[dirIter].direction), 0.0);
        singleLight += diffuseIntensity * dynamicShadowedDirLightStore[dirIter].diffuse;

        // Adds on specular lighting to light
        var specularIntensity : f32 = pow(max(dot(viewDir, reflect(dynamicShadowedDirLightStore[dirIter].direction, in.normal)), 0.0), 32);
        singleLight += specularIntensity * dynamicShadowedDirLightStore[dirIter].specular;

        // Checks to make sure that singleLight actually applies with shadow
        singleLight = singleLight * lightsUncovered;
        overallLight += singleLight;
    }

    for (var pointIter : u32 = 0 ; pointIter < fixedData.pointLightAmount ; pointIter++) {
        //Creates copy
        let pointLight = dynamicShadowedPointLightStore[pointIter];

        // Handles Phong Lighting
        var lightToFragDir: vec3<f32> = (in.worldPos/in.worldPos.w).xyz - pointLight.position;
        var lightToFragDistance : f32 = length(lightToFragDir);

        // Checks for shadowing along approximate 3x3 pixel range 

        // Sets up pcs sampling information
        // Creates grid at the end of normalized light dir to sample off of
        var normalizedLightDir: vec3<f32> = normalize(lightToFragDir);
        var basis: orthonormalBasis = getOrthonormalBasis(normalizedLightDir);
        var unitLength: f32 = (1.0/f32(fixedData.pointLightMapDimension));
        basis.up *= unitLength;
        basis.left *= unitLength;
        var baseDir: vec3<f32> = normalizedLightDir - (basis.up + basis.left) * f32(fixedData.pcsRange - 1)/2.0;
        var normalizedLightToFragDistance: f32 = lightToFragDistance/pointLight.radius;

        // Actually samples pcs 
        var pointLightUncovered: f32 = 0;
        for (var xIter : u32 = 0 ; xIter < fixedData.pcsRange ; xIter++) {
            for (var yIter : u32 = 0 ; yIter < fixedData.pcsRange ; yIter++) {
                var newPos: vec3<f32> = baseDir + basis.up * f32(yIter) + basis.left * f32(xIter);
                pointLightUncovered += textureSampleCompare(
                    dynamicShadowedPointLightMap, 
                    shadowMapSampler, 
                    newPos, 
                    pointIter, 
                    normalizedLightToFragDistance);
            }
        }
        pointLightUncovered /= f32(fixedData.pcsRange * fixedData.pcsRange);

        // TODO: Create a softer way to enforce cutoff
        if (lightToFragDistance < pointLight.radius) {
            // Handles phong lighting
            var singleLight : vec3<f32> = vec3<f32>(0, 0, 0);
            lightToFragDir = normalize(lightToFragDir);

            // Adds on diffuse lighting to light
            var diffuseIntensity : f32 = max(dot(in.normal, -lightToFragDir), 0.0);
            singleLight += diffuseIntensity * pointLight.diffuse;

            // Adds on specular lighting 
            var specularIntensity : f32 = pow(max(dot(viewDir, reflect(lightToFragDir, in.normal)), 0.0), 32);
            singleLight += specularIntensity * pointLight.specular;

            // Takes attenuation into account then adds lighting contribution
            var sqrtDist = sqrt(lightToFragDistance/pointLight.radius);
            singleLight *= sqrt(1 - sqrtDist)/ (1 + pointLight.falloff * sqrtDist);
            overallLight += singleLight * pointLightUncovered;
        }
    }

    for (var spotIter : u32 = 0 ; spotIter < fixedData.spotLightAmount ; spotIter++) {

    }
    return vec4<f32>(((overallLight) * in.color) + ambient,1);
}