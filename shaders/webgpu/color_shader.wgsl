// TODO: Better explore alternatives in alignment, maybe we could merge in different variables to reduce waste due to alignment

// TODO: Replace with WESL logic
const dynamicShadowedPointLightIntegerOffset = [<int><POINT_LIGHT_PADDING><68>];
const pcfSamplePatternMax = [<int><PCF_SAMPLE_MAX><32>];

// Math functionality
struct orthonormalBasis {
    up: vec3<f32>,
    left: vec3<f32>
}

// Uniform variables in the color pass that update semi frequently
struct ColorUniforms {
    // Camera Data
    camViewMat : mat4x4<f32>,
    camPos: vec3<f32>,

    // Light Data
    dirLightAmount: u32,
    pointLightAmount: u32,
    spotLightAmount: u32,
    padding: u32,
    padding2: u32
}

// Uniform variables in color pass completely controlled by renderer 
// that almost never update
struct ColorFixedUniforms {
    // PCF Data
    pcfSamplePattern: array<vec2<f32>, pcfSamplePatternMax>,
    pcfSampleRate: u32,
    pcfRange: f32, // Replace later when run out of padding room
    // Light information
    dirLightCascadeCount: u32,

    padding: u32,
}

// Represents the data that differentiates each instance of the same mesh
struct ObjData {
    transform : mat4x4<f32>,
    normMat : mat4x4<f32>,
    color : vec4<f32>
}

// Represents a single directional light with shadows and a potential to change pos/dir over time.
struct DynamicShadowedDirLight {
    color : vec3<f32>,
    padding : f32, // Fill
    direction : vec3<f32>,
    padding2 : f32, // Fill with useful stuff later
}

struct DynamicShadowedSpotLight {
    color : vec3<f32>,
    penumbraCosCutoff : f32,
    position : vec3<f32>,
    outerCosCutoff : f32,
    direction : vec3<f32>,
    range : f32,
    nearPlaneWorldDim : f32,
    planeDimSlope : f32,
    falloff : f32,
    padding : f32
}

struct DynamicShadowedPointLight {
    color : vec3<f32>,
    radius : f32,
    position : vec3<f32>,
    falloff : f32
}

struct DynamicShadowedPointLightPadded {
    data : DynamicShadowedPointLight,
    padding : array<u32, dynamicShadowedPointLightIntegerOffset>
}

@binding(0) @group(0) var<uniform> combinedCamSpace : mat4x4<f32>;
@binding(1) @group(0) var<storage, read> objStore : array<ObjData>; 

@binding(0) @group(1) var<uniform> colorUniforms : ColorUniforms;
@binding(1) @group(1) var<uniform> colorFixedUniforms : ColorFixedUniforms;
@binding(2) @group(1) var<storage, read> shadowedDirLightStore : array<DynamicShadowedDirLight>;
@binding(3) @group(1) var<storage, read> shadowCascadesWorldToTextureCoordRatio : array<f32>;
@binding(4) @group(1) var<storage, read> shadowedSpotLightStore : array<DynamicShadowedSpotLight>;
@binding(5) @group(1) var<storage, read> shadowedPointLightStore : array<DynamicShadowedPointLightPadded>;
@binding(6) @group(1) var<storage, read> lightsSpacesStore : array<mat4x4<f32>>;
@binding(7) @group(1) var shadowedDirLightMap: texture_depth_2d_array;
@binding(8) @group(1) var shadowedSpotLightMap: texture_depth_2d_array;
@binding(9) @group(1) var shadowedPointLightMap: texture_depth_cube_array;
@binding(10) @group(1) var<storage, read> dirLightCascadeViewSpaceCutoffs : array<f32>;
@binding(11) @group(1) var shadowMapSampler : sampler_comparison;

struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(1) uvX : f32,
    @location(2) normal : vec3<f32>,
    @location(3) uvY : f32,
    @builtin(instance_index) instance: u32, // Represents which instance within objStore to pull data from
}

struct ColorPassVertexOut {
    @builtin(position) position : vec4<f32>,
    @location(0) fragToCamPos : vec3<f32>,
    @location(1) normal : vec3<f32>,
    @location(2) worldPos : vec4<f32>,
    @location(3) color : vec3<f32>,
    @location(4) uv : vec2<f32>
}



/**
 * >>> Brdf functionality <<<
 * Encapsulates brdf logic
 * 
 * Currently encapsulates basic phong shading
 * TODO: Implement blinn-phong instead of just phong
 */
// Represents the material data at a point
struct MatData {
    color : vec3<f32>,
    uv : vec2<f32>
};
// Represents the data accumulated by the brdf over a series of lights
struct ColorData {
    color : vec3<f32>
};
// Gathers together mat data at the vertex stage
fn getMatData(output : ColorPassVertexOut) -> MatData {
    return MatData(output.color.xyz, output.uv);
}
// Accumulates the results of a single light into color data 
// through a designated brdf
fn addBrdf(
    lightColor : vec3<f32>, 
    fragToCamDir : vec3<f32>, 
    fragToLightDir : vec3<f32>, 
    fragNormal : vec3<f32>, 
    fragMat : MatData, 
    output : ptr<function, ColorData>) {

    // Adds on diffuse and specular lighting to overall light
    let diffuseIntensity : f32 = max(dot(fragNormal, fragToLightDir), 0.0);
    let specularIntensity : f32 = pow(max(dot(-fragToCamDir, reflect(-fragToLightDir, fragNormal)), 0.0), 32);
    (*output).color += (specularIntensity + diffuseIntensity) * lightColor;
}

// Compiles all of the previous brdfs into a single color
fn finalizeBrdf(fragMat : MatData, finalColor : ColorData) ->  vec3<f32> {
    let ambientIntensity : f32 = 0.25;
    return (finalColor.color + vec3f(ambientIntensity)) * fragMat.color;
}

/**
 * Math helpers
 */
// Collects translation from a mat4x4 
fn getTranslate(in : mat4x4<f32>) -> vec3<f32> {
    return vec3<f32>(in[3][0], in[3][1], in[3][2]);
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


/**
 * Vertex function
 */
@vertex
fn vtxMain(in : VertexIn) -> ColorPassVertexOut {
  var out : ColorPassVertexOut;

  let worldPos = objStore[in.instance].transform * vec4<f32>(in.position,1);

  out.worldPos = worldPos;
  out.position = combinedCamSpace * worldPos;
  out.fragToCamPos = colorUniforms.camPos - worldPos.xyz;
  let nMat = objStore[in.instance].normMat;
  out.normal = normalize(mat3x3(nMat[0].xyz, nMat[1].xyz, nMat[2].xyz) * in.normal);
  out.color = objStore[in.instance].color.xyz;
  out.uv = vec2f(in.uvX, in.uvY);
  
  return out;
}

/**
 * Fragment Pass
 */
@fragment
fn fsMain(in : ColorPassVertexOut) -> @location(0) vec4<f32>  {
    // TODO: Set ambient lighting to be specified
    var colorData : ColorData;
    let matData : MatData = getMatData(in);

    let normalWorldPos : vec4<f32> = in.worldPos/in.worldPos.w;

    let viewDir : vec3<f32> = normalize(in.fragToCamPos);
    let fragDepth = (colorUniforms.camViewMat * normalWorldPos).z;

    for (var dirIter : u32 = 0 ; dirIter < colorUniforms.dirLightAmount ; dirIter++) {
        // Checks what cascade it the specific fragment should reference
        var cascadeCheck : u32 = 0;
        while (cascadeCheck < colorFixedUniforms.dirLightCascadeCount) {
            if (dirLightCascadeViewSpaceCutoffs[cascadeCheck] > fragDepth) {
                break;
            }
            cascadeCheck++;
        }

        // Checks if location has been covered by light
        let lightSpaceDirIdx : u32 = dirIter + cascadeCheck * colorUniforms.dirLightAmount;
        var lightSpacePosition : vec4<f32> = lightsSpacesStore[lightSpaceDirIdx] * (normalWorldPos);
        lightSpacePosition = lightSpacePosition / lightSpacePosition.w;
        let texturePosition : vec3<f32> = vec3<f32>((lightSpacePosition.x * 0.5) + 0.5, (lightSpacePosition.y * -0.5) + 0.5, lightSpacePosition.z);
        // Sets up sample location
        let worldToTexCoordRatio : f32 = shadowCascadesWorldToTextureCoordRatio[cascadeCheck];
        let unit: f32 = colorFixedUniforms.pcfRange * worldToTexCoordRatio;
        let base: vec2<f32> = texturePosition.xy - vec2<f32>(unit * 0.5);

        // Finds PCF percentage
        var sum: f32 = 0.0;
        for (var xIter : u32 = 0 ; xIter < 2 ; xIter++) {
            for (var yIter : u32 = 0 ; yIter < 2; yIter++) {
                var newPos: vec2<f32> = base + vec2<f32>(f32(xIter)*unit, f32(yIter)*unit);
                sum += textureSampleCompare(shadowedDirLightMap, shadowMapSampler, newPos, cascadeCheck, texturePosition.z);
            }
        }
        let shadowedIntensity = sum / 4.0;
        let adjustedLightColor : vec3<f32> = shadowedDirLightStore[dirIter].color * shadowedIntensity;

        addBrdf(
            adjustedLightColor, 
            viewDir, 
            -shadowedDirLightStore[dirIter].direction, 
            in.normal, 
            matData, 
            &colorData);
    }

    var spotLightSpaceIdx : u32 = colorFixedUniforms.dirLightCascadeCount * colorUniforms.dirLightAmount;
    for (var spotIter : u32 = 0 ; spotIter < colorUniforms.spotLightAmount ; spotIter++) {
        var spotlight : DynamicShadowedSpotLight = shadowedSpotLightStore[spotIter];

        // Factors theta into lighting
        var thetaIntensity : f32 = 0;
        let fragToSpotLightDir : vec3<f32> = spotlight.position - normalWorldPos.xyz;
        let fragToSpotLightDirNorm : vec3<f32> = normalize(fragToSpotLightDir);
        var theta: f32 = dot(fragToSpotLightDirNorm, normalize(-spotlight.direction));
        if (theta > spotlight.outerCosCutoff) {
            var epsilon : f32 = spotlight.penumbraCosCutoff - spotlight.outerCosCutoff;
            thetaIntensity = clamp((theta - spotlight.outerCosCutoff) / epsilon, 0.0, 1.0);
        }

        // Factors distance into lighting 
        var distanceIntensity : f32 = 0;
        let fragToLightDist : f32 = length(fragToSpotLightDir); 
        if (fragToLightDist < spotlight.range) {
            let sqrtDist : f32 = sqrt(fragToLightDist/spotlight.range);
            let attenuationModifier : f32 = sqrt(1 - sqrtDist)/ (1 + spotlight.falloff * sqrtDist);
            distanceIntensity = attenuationModifier;
        }

        // Factors shadowing into lighting
        var lightSpacePosition : vec4<f32> = lightsSpacesStore[spotLightSpaceIdx] * (in.worldPos);
        lightSpacePosition = lightSpacePosition / lightSpacePosition.w;
        let lightSpaceDirIdx : u32 = spotLightSpaceIdx;
        // TODO: Make the shadow bias less arbitrary.
        let texturePosition: vec3<f32> = vec3<f32>((lightSpacePosition.x * 0.5) + 0.5, (lightSpacePosition.y * -0.5) + 0.5, lightSpacePosition.z - 0.0000125);

        // Sets up sample location
        // Comp works for finding shadow length of frag 
        let fragToPlaneDist : f32 = dot(-spotlight.direction, fragToSpotLightDir);
        let planeSize : f32 = spotlight.planeDimSlope * fragToPlaneDist;
        let worldToTexCoordRatio : f32 = 1 / planeSize;
        let unit: f32 = colorFixedUniforms.pcfRange * worldToTexCoordRatio;
        let base: vec2<f32> = texturePosition.xy - vec2<f32>(unit * 0.5);

        // Finds PCF percentage
        var shadowIntensity: f32 = 0.0;
        for (var xIter : u32 = 0 ; xIter < 2 ; xIter++) {
            for (var yIter : u32 = 0 ; yIter < 2; yIter++) {
                var newPos: vec2<f32> = base + vec2<f32>(f32(xIter)*unit, f32(yIter)*unit);
                shadowIntensity += textureSampleCompare(shadowedSpotLightMap, shadowMapSampler, newPos, spotIter,texturePosition.z);
            }
        }
        shadowIntensity /= 4;

        addBrdf(
            spotlight.color * 
            (distanceIntensity * thetaIntensity* shadowIntensity),
            viewDir,
            fragToSpotLightDirNorm,
            in.normal,
            matData,
            &colorData
        );

        spotLightSpaceIdx += 1;
    }

    for (var pointIter : u32 = 0 ; pointIter < colorUniforms.pointLightAmount ; pointIter++) {
        //Creates copy
        let pointLight = shadowedPointLightStore[pointIter].data;

        // Handles Phong Lighting
        let lightToFragDir: vec3<f32> = (in.worldPos/in.worldPos.w).xyz - pointLight.position;
        var lightToFragDistance : f32 = length(lightToFragDir);

        // Checks for shadowing along approximate 3x3 pixel range 

        // Sets up pcs sampling information
        // Creates grid at the end of normalized light dir to sample off of
        let normalizedLightDir: vec3<f32> = normalize(lightToFragDir);
        var basis: orthonormalBasis = getOrthonormalBasis(normalizedLightDir);
        let unitLength: f32 = colorFixedUniforms.pcfRange;
        basis.up *= unitLength;
        basis.left *= unitLength;
        let baseDir: vec3<f32> = lightToFragDir - (basis.up + basis.left) * 0.5;
        let normalizedLightToFragDistance: f32 = lightToFragDistance/pointLight.radius;

        // Actually samples pcs 
        var pointLightUncovered: f32 = 0;
        for (var xIter : u32 = 0 ; xIter < 2 ; xIter++) {
            for (var yIter : u32 = 0 ; yIter < 2 ; yIter++) {
                let newPos: vec3<f32> = baseDir + basis.up * f32(yIter) + basis.left * f32(xIter);
                pointLightUncovered += textureSampleCompare(
                    shadowedPointLightMap, 
                    shadowMapSampler, 
                    newPos, 
                    pointIter, 
                    normalizedLightToFragDistance);
            }
        }
        pointLightUncovered /= 4;

        let sqrtDist : f32 = sqrt(lightToFragDistance/pointLight.radius);
        let attenuationModifier : f32 = sqrt(1 - sqrtDist)/ (1 + pointLight.falloff * sqrtDist);
        let adjustedLightColor : vec3<f32> = pointLight.color * (attenuationModifier * pointLightUncovered);
        // TODO: Create a softer way to enforce cutoff
        if (lightToFragDistance < pointLight.radius) {
            // Handles phong lighting
            let lightToFragDirNormalized = normalize(lightToFragDir);
            addBrdf(
                adjustedLightColor, 
                viewDir, 
                -lightToFragDirNormalized, 
                in.normal, 
                matData, 
                &colorData);
        }
    }

    return vec4f(finalizeBrdf(matData, colorData),1);
}