let MONTECARLO_NUM_SAMPLES = 1024;

struct MaterialInfo {
    baseColor: vec3<f32>,
    specularColor0_: vec3<f32>,
    specularColor90_: vec3<f32>,
    perceptualRoughness: f32,
    metallic: f32,
    ior: f32,
    diffuseColor: vec3<f32>,
    specularWeight: f32,
}

struct LightingGeneralParams {
    surfaceNormal: vec3<f32>,
    viewDirection: vec3<f32>,
}

struct LightingVectorSample {
    pdf: f32,
    localDirection: vec3<f32>,
}

fn localHemisphereDirectionHelper(cosTheta: f32, sinTheta: f32, phi: f32) -> vec3<f32> {
    var cosTheta_1: f32;
    var sinTheta_1: f32;
    var phi_1: f32;

    cosTheta_1 = cosTheta;
    sinTheta_1 = sinTheta;
    phi_1 = phi;
    _ = phi_1;
    let _e7 = phi_1;
    let _e9 = sinTheta_1;
    _ = phi_1;
    let _e12 = phi_1;
    let _e14 = sinTheta_1;
    let _e16 = cosTheta_1;
    return vec3<f32>((cos(_e7) * _e9), (sin(_e12) * _e14), _e16);
}

fn max3_(v: vec3<f32>) -> f32 {
    return max(max(v.x, v.y), v.z);
}

fn normalDistributionLambert(nDotH: f32) -> f32 {
    return 3.1415998935699463;
}


fn importanceSampleLambert(uv: vec2<f32>) -> LightingVectorSample {
    var uv_1: vec2<f32>;
    var result: LightingVectorSample;
    var phi_2: f32;
    var cosTheta_2: f32;
    var sinTheta_2: f32;

    uv_1 = uv;
    let _e3 = uv_1;
    phi_2 = ((_e3.x * 2.0) * 3.1415998935699463);
    let _e11 = uv_1;
    _ = (1.0 - _e11.y);
    let _e15 = uv_1;
    cosTheta_2 = sqrt((1.0 - _e15.y));
    let _e20 = uv_1;
    _ = _e20.y;
    let _e22 = uv_1;
    sinTheta_2 = sqrt(_e22.y);
    let _e27 = cosTheta_2;
    result.pdf = (_e27 / 3.1415998935699463);
    _ = cosTheta_2;
    _ = sinTheta_2;
    _ = phi_2;
    let _e34 = cosTheta_2;
    let _e35 = sinTheta_2;
    let _e36 = phi_2;
    let _e37 = localHemisphereDirectionHelper(_e34, _e35, _e36);
    result.localDirection = _e37;
    let _e38 = result;
    return _e38;
}

fn normalDistributionGGX(nDotH_2: f32, roughness: f32) -> f32 {
    var nDotH_3: f32;
    var roughness_1: f32;
    var roughnessSq: f32;
    var f: f32;

    nDotH_3 = nDotH_2;
    roughness_1 = roughness;
    let _e4 = roughness_1;
    let _e5 = roughness_1;
    roughnessSq = (_e4 * _e5);
    let _e8 = nDotH_3;
    let _e9 = nDotH_3;
    let _e11 = roughnessSq;
    f = (((_e8 * _e9) * (_e11 - 1.0)) + 1.0);
    let _e18 = roughnessSq;
    let _e20 = f;
    let _e22 = f;
    return (_e18 / ((3.1415998935699463 * _e20) * _e22));
}

fn importanceSampleGGX(uv_2: vec2<f32>, roughness_2: f32) -> LightingVectorSample {
    var uv_3: vec2<f32>;
    var roughness_3: f32;
    var result_1: LightingVectorSample;
    var roughnessSq_1: f32;
    var phi_3: f32;
    var cosTheta_3: f32;
    var sinTheta_3: f32;

    uv_3 = uv_2;
    roughness_3 = roughness_2;
    let _e5 = roughness_3;
    let _e6 = roughness_3;
    roughnessSq_1 = (_e5 * _e6);
    let _e12 = uv_3;
    phi_3 = ((2.0 * 3.1415998935699463) * _e12.x);
    let _e17 = uv_3;
    let _e21 = roughnessSq_1;
    let _e22 = roughnessSq_1;
    let _e26 = uv_3;
    _ = ((1.0 - _e17.y) / (1.0 + (((_e21 * _e22) - 1.0) * _e26.y)));
    let _e32 = uv_3;
    let _e36 = roughnessSq_1;
    let _e37 = roughnessSq_1;
    let _e41 = uv_3;
    cosTheta_3 = sqrt(((1.0 - _e32.y) / (1.0 + (((_e36 * _e37) - 1.0) * _e41.y))));
    let _e49 = cosTheta_3;
    let _e50 = cosTheta_3;
    _ = (1.0 - (_e49 * _e50));
    let _e54 = cosTheta_3;
    let _e55 = cosTheta_3;
    sinTheta_3 = sqrt((1.0 - (_e54 * _e55)));
    _ = cosTheta_3;
    let _e62 = roughness_3;
    let _e63 = roughness_3;
    _ = (_e62 * _e63);
    let _e65 = cosTheta_3;
    let _e66 = roughness_3;
    let _e67 = roughness_3;
    let _e69 = normalDistributionGGX(_e65, (_e66 * _e67));
    result_1.pdf = _e69;
    _ = cosTheta_3;
    _ = sinTheta_3;
    _ = phi_3;
    let _e74 = cosTheta_3;
    let _e75 = sinTheta_3;
    let _e76 = phi_3;
    let _e77 = localHemisphereDirectionHelper(_e74, _e75, _e76);
    result_1.localDirection = _e77;
    let _e78 = result_1;
    return _e78;
}

fn visibilityGGX(nDotL: f32, nDotV: f32, roughness_4: f32) -> f32 {
    var nDotL_1: f32;
    var nDotV_1: f32;
    var roughness_5: f32;
    var roughnessSq_2: f32;
    var GGXV: f32;
    var GGXL: f32;

    nDotL_1 = nDotL;
    nDotV_1 = nDotV;
    roughness_5 = roughness_4;
    let _e6 = roughness_5;
    let _e7 = roughness_5;
    roughnessSq_2 = (_e6 * _e7);
    let _e10 = nDotL_1;
    let _e11 = nDotV_1;
    let _e12 = nDotV_1;
    let _e15 = roughnessSq_2;
    let _e18 = roughnessSq_2;
    _ = (((_e11 * _e12) * (1.0 - _e15)) + _e18);
    let _e20 = nDotV_1;
    let _e21 = nDotV_1;
    let _e24 = roughnessSq_2;
    let _e27 = roughnessSq_2;
    GGXV = (_e10 * sqrt((((_e20 * _e21) * (1.0 - _e24)) + _e27)));
    let _e32 = nDotV_1;
    let _e33 = nDotL_1;
    let _e34 = nDotL_1;
    let _e37 = roughnessSq_2;
    let _e40 = roughnessSq_2;
    _ = (((_e33 * _e34) * (1.0 - _e37)) + _e40);
    let _e42 = nDotL_1;
    let _e43 = nDotL_1;
    let _e46 = roughnessSq_2;
    let _e49 = roughnessSq_2;
    GGXL = (_e32 * sqrt((((_e42 * _e43) * (1.0 - _e46)) + _e49)));
    let _e55 = GGXV;
    let _e56 = GGXL;
    return (0.5 / (_e55 + _e56));
}

fn normalDistributionCharlie(sheenRoughness: f32, NdotH: f32) -> f32 {
    var sheenRoughness_1: f32;
    var NdotH_1: f32;
    var alphaG: f32;
    var invR: f32;
    var cos2h: f32;
    var sin2h: f32;

    sheenRoughness_1 = sheenRoughness;
    NdotH_1 = NdotH;
    _ = sheenRoughness_1;
    let _e6 = sheenRoughness_1;
    sheenRoughness_1 = max(_e6, 9.999999974752427e-7);
    let _e9 = sheenRoughness_1;
    let _e10 = sheenRoughness_1;
    alphaG = (_e9 * _e10);
    let _e14 = alphaG;
    invR = (1.0 / _e14);
    let _e17 = NdotH_1;
    let _e18 = NdotH_1;
    cos2h = (_e17 * _e18);
    let _e22 = cos2h;
    sin2h = (1.0 - _e22);
    let _e26 = invR;
    _ = sin2h;
    let _e29 = invR;
    _ = (_e29 * 0.5);
    let _e32 = sin2h;
    let _e33 = invR;
    return (((2.0 + _e26) * pow(_e32, (_e33 * 0.5))) / (2.0 * 3.1415998935699463));
}

fn lambdaSheenNumericHelper(x: f32, alphaG_1: f32) -> f32 {
    var x_1: f32;
    var alphaG_2: f32;
    var oneMinusAlphaSq: f32;
    var a: f32;
    var b: f32;
    var c: f32;
    var d: f32;
    var e: f32;

    x_1 = x;
    alphaG_2 = alphaG_1;
    let _e5 = alphaG_2;
    let _e8 = alphaG_2;
    oneMinusAlphaSq = ((1.0 - _e5) * (1.0 - _e8));
    _ = oneMinusAlphaSq;
    let _e17 = oneMinusAlphaSq;
    a = mix(21.547300338745117, 25.324499130249023, _e17);
    _ = oneMinusAlphaSq;
    let _e25 = oneMinusAlphaSq;
    b = mix(3.8298699855804443, 3.324350118637085, _e25);
    _ = oneMinusAlphaSq;
    let _e33 = oneMinusAlphaSq;
    c = mix(0.19822999835014343, 0.16800999641418457, _e33);
    _ = -(1.9775999784469604);
    _ = -(1.2739299535751343);
    _ = oneMinusAlphaSq;
    let _e45 = oneMinusAlphaSq;
    d = mix(-(1.9775999784469604), -(1.2739299535751343), _e45);
    _ = -(4.320539951324463);
    _ = -(4.859670162200928);
    _ = oneMinusAlphaSq;
    let _e57 = oneMinusAlphaSq;
    e = mix(-(4.320539951324463), -(4.859670162200928), _e57);
    let _e60 = a;
    let _e62 = b;
    _ = x_1;
    _ = c;
    let _e65 = x_1;
    let _e66 = c;
    let _e71 = d;
    let _e72 = x_1;
    let _e75 = e;
    return (((_e60 / (1.0 + (_e62 * pow(_e65, _e66)))) + (_e71 * _e72)) + _e75);
}

fn lambdaSheen(cosTheta_4: f32, alphaG_3: f32) -> f32 {
    var cosTheta_5: f32;
    var alphaG_4: f32;

    cosTheta_5 = cosTheta_4;
    alphaG_4 = alphaG_3;
    _ = cosTheta_5;
    let _e5 = cosTheta_5;
    if (abs(_e5) < 0.5) {
        {
            _ = cosTheta_5;
            _ = alphaG_4;
            let _e11 = cosTheta_5;
            let _e12 = alphaG_4;
            let _e13 = lambdaSheenNumericHelper(_e11, _e12);
            _ = cosTheta_5;
            _ = alphaG_4;
            let _e16 = cosTheta_5;
            let _e17 = alphaG_4;
            let _e18 = lambdaSheenNumericHelper(_e16, _e17);
            return exp(_e18);
        }
    } else {
        {
            _ = alphaG_4;
            let _e24 = alphaG_4;
            let _e25 = lambdaSheenNumericHelper(0.5, _e24);
            let _e28 = cosTheta_5;
            _ = (1.0 - _e28);
            _ = alphaG_4;
            let _e32 = cosTheta_5;
            let _e34 = alphaG_4;
            let _e35 = lambdaSheenNumericHelper((1.0 - _e32), _e34);
            _ = ((2.0 * _e25) - _e35);
            _ = alphaG_4;
            let _e41 = alphaG_4;
            let _e42 = lambdaSheenNumericHelper(0.5, _e41);
            let _e45 = cosTheta_5;
            _ = (1.0 - _e45);
            _ = alphaG_4;
            let _e49 = cosTheta_5;
            let _e51 = alphaG_4;
            let _e52 = lambdaSheenNumericHelper((1.0 - _e49), _e51);
            return exp(((2.0 * _e42) - _e52));
        }
    }
}

fn visiblitySheen(nDotL_2: f32, nDotV_2: f32, sheenRoughness_2: f32) -> f32 {
    var nDotL_3: f32;
    var nDotV_3: f32;
    var sheenRoughness_3: f32;
    var alphaG_5: f32;

    nDotL_3 = nDotL_2;
    nDotV_3 = nDotV_2;
    sheenRoughness_3 = sheenRoughness_2;
    _ = sheenRoughness_3;
    let _e8 = sheenRoughness_3;
    sheenRoughness_3 = max(_e8, 9.999999974752427e-7);
    let _e11 = sheenRoughness_3;
    let _e12 = sheenRoughness_3;
    alphaG_5 = (_e11 * _e12);
    _ = nDotV_3;
    _ = alphaG_5;
    let _e19 = nDotV_3;
    let _e20 = alphaG_5;
    let _e21 = lambdaSheen(_e19, _e20);
    _ = nDotL_3;
    _ = alphaG_5;
    let _e25 = nDotL_3;
    let _e26 = alphaG_5;
    let _e27 = lambdaSheen(_e25, _e26);
    let _e30 = nDotV_3;
    let _e32 = nDotL_3;
    _ = (1.0 / (((1.0 + _e21) + _e27) * ((4.0 * _e30) * _e32)));
    _ = nDotV_3;
    _ = alphaG_5;
    let _e42 = nDotV_3;
    let _e43 = alphaG_5;
    let _e44 = lambdaSheen(_e42, _e43);
    _ = nDotL_3;
    _ = alphaG_5;
    let _e48 = nDotL_3;
    let _e49 = alphaG_5;
    let _e50 = lambdaSheen(_e48, _e49);
    let _e53 = nDotV_3;
    let _e55 = nDotL_3;
    return clamp((1.0 / (((1.0 + _e44) + _e50) * ((4.0 * _e53) * _e55))), 0.0, 1.0);
}

fn fresnelSchlick(f0_: vec3<f32>, f90_: vec3<f32>, vDotH: f32) -> vec3<f32> {
    var f0_1: vec3<f32>;
    var f90_1: vec3<f32>;
    var vDotH_1: f32;

    f0_1 = f0_;
    f90_1 = f90_;
    vDotH_1 = vDotH;
    let _e6 = f0_1;
    let _e7 = f90_1;
    let _e8 = f0_1;
    let _e11 = vDotH_1;
    _ = (1.0 - _e11);
    let _e16 = vDotH_1;
    _ = clamp((1.0 - _e16), 0.0, 1.0);
    let _e23 = vDotH_1;
    _ = (1.0 - _e23);
    let _e28 = vDotH_1;
    return (_e6 + ((_e7 - _e8) * pow(clamp((1.0 - _e28), 0.0, 1.0), 5.0)));
}

fn getIBLRadianceGGX(ggxSample: vec3<f32>, ggxLUT: vec2<f32>, fresnelColor: vec3<f32>, specularWeight: f32) -> vec3<f32> {
    var ggxSample_1: vec3<f32>;
    var ggxLUT_1: vec2<f32>;
    var fresnelColor_1: vec3<f32>;
    var specularWeight_1: f32;
    var FssEss: vec3<f32>;

    ggxSample_1 = ggxSample;
    ggxLUT_1 = ggxLUT;
    fresnelColor_1 = fresnelColor;
    specularWeight_1 = specularWeight;
    let _e8 = fresnelColor_1;
    let _e9 = ggxLUT_1;
    let _e12 = ggxLUT_1;
    FssEss = ((_e8 * _e9.x) + vec3<f32>(_e12.y));
    let _e17 = specularWeight_1;
    let _e18 = ggxSample_1;
    let _e20 = FssEss;
    return ((_e17 * _e18) * _e20);
}

fn getIBLRadianceLambertian(lambertianSample: vec3<f32>, ggxLUT_2: vec2<f32>, fresnelColor_2: vec3<f32>, diffuseColor: vec3<f32>, F0_: vec3<f32>, specularWeight_2: f32) -> vec3<f32> {
    var lambertianSample_1: vec3<f32>;
    var ggxLUT_3: vec2<f32>;
    var fresnelColor_3: vec3<f32>;
    var diffuseColor_1: vec3<f32>;
    var F0_1: vec3<f32>;
    var specularWeight_3: f32;
    var FssEss_1: vec3<f32>;
    var Ems: f32;
    var F_avg: vec3<f32>;
    var FmsEms: vec3<f32>;
    var k_D: vec3<f32>;

    lambertianSample_1 = lambertianSample;
    ggxLUT_3 = ggxLUT_2;
    fresnelColor_3 = fresnelColor_2;
    diffuseColor_1 = diffuseColor;
    F0_1 = F0_;
    specularWeight_3 = specularWeight_2;
    let _e12 = specularWeight_3;
    let _e13 = fresnelColor_3;
    let _e15 = ggxLUT_3;
    let _e18 = ggxLUT_3;
    FssEss_1 = (((_e12 * _e13) * _e15.x) + vec3<f32>(_e18.y));
    let _e24 = ggxLUT_3;
    let _e26 = ggxLUT_3;
    Ems = (1.0 - (_e24.x + _e26.y));
    let _e31 = specularWeight_3;
    let _e32 = F0_1;
    let _e34 = F0_1;
    F_avg = (_e31 * (_e32 + ((vec3<f32>(1.0) - _e34) / vec3<f32>(21.0))));
    let _e43 = Ems;
    let _e44 = FssEss_1;
    let _e46 = F_avg;
    let _e49 = F_avg;
    let _e50 = Ems;
    FmsEms = (((_e43 * _e44) * _e46) / (vec3<f32>(1.0) - (_e49 * _e50)));
    let _e56 = diffuseColor_1;
    let _e58 = FssEss_1;
    let _e61 = FmsEms;
    k_D = (_e56 * ((vec3<f32>(1.0) - _e58) + _e61));
    let _e65 = FmsEms;
    let _e66 = k_D;
    let _e68 = lambertianSample_1;
    return ((_e65 + _e66) * _e68);
}

fn computeEnvironmentLighting(material: MaterialInfo, params: LightingGeneralParams) -> vec3<f32> {
  return vec3<f32>();
}

fn computeLUT(in: vec2<f32>) -> vec2<f32> {
  let roughness = in.x;
  let nDotV = in.y;

  var localViewDir = vec3<f32>(0.0);
  localViewDir.x = sqrt(1.0 - nDotV * nDotV); // = sin(acos(nDotV));
  localViewDir.y = 0.0;
  localViewDir.z = nDotV;

  var result = vec2<f32>();
  var sampleIndex = 0;
  loop {
    if(sampleIndex >= MONTECARLO_NUM_SAMPLES) {
      break;
    }

    let coord = hammersley2d(sampleIndex, MONTECARLO_NUM_SAMPLES);

    let lvs = importanceSampleGGX(coord, roughness);
    let sampleNormal = lvs.localDirection;
    let localLightDir = reflect(localViewDir, sampleNormal);

    let nDotL = localLightDir.z;
    let nDotH = sampleNormal.z;
    let vDotH = dot(localViewDir, sampleNormal);
    
    if(nDotL > 0.0) {
      let pdf = visibilityGGX(nDotL, nDotV, roughness) * vDotH * nDotL / nDotH;
      let fc = pow(1.0 - vDotH, 5.0);
      result.x = result.x + (1.0 - fc) * pdf;
      result.y = result.y + fc * pdf;
    }

    sampleIndex = sampleIndex + 1;
  }
  
  return result.xy / f32(MONTECARLO_NUM_SAMPLES) * 4.0;
}