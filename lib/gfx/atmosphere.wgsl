// REGEXES:
// float ([a-z]\w*)
// $1: f32
//
// vec([0-9]+) ([a-z]\w*)
// var $2: vec$1<f32>
// Translated from https://github.com/Fewes/MinimalAtmosphere/blob/master/Assets/Atmosphere/Shaders/Atmosphere.cginc
//
// Copyright (c) 2021 Felix Westin
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

const PI = 3.14159265359;
const C_RAYLEIGH         : vec3<f32> = vec3<f32>(5.802, 13.558, 33.100);
const C_MIE              : vec3<f32> = vec3<f32>(3.996,  3.996,  3.996);
const C_OZONE            : vec3<f32> = vec3<f32>(0.650,  1.881,  0.085);
const C_SCALE                        = 1e-6;
const ATMOSPHERE_DENSITY : f32       = 1.0;
const EXPOSURE           : f32       = 20.0;
const PLANET_RADIUS     = 63710.0;
const ATMOSPHERE_HEIGHT = 100000.0;

fn GetPlanetCenter() -> vec3<f32> { return vec3<f32>(0.0, -PLANET_RADIUS, 0.0); }
fn GetRayleighHeight() -> f32 { return ATMOSPHERE_HEIGHT * 0.08; }
fn GetMieHeight() -> f32 { return ATMOSPHERE_HEIGHT * 0.012; }

fn SphereIntersection(rayStart_: vec3<f32>, rayDir: vec3<f32>, sphereCenter: vec3<f32>, sphereRadius: f32) -> vec2<f32> {
  let rayStart = rayStart_ - sphereCenter;
  let a = dot(rayDir, rayDir);
  let b = 2.0 * dot(rayStart, rayDir);
  let c = dot(rayStart, rayStart) - (sphereRadius * sphereRadius);
  var d = b * b - 4.0 * a * c;
  if (d < 0.0) {
    return vec2<f32>(-1.0);
  } else {
    d = sqrt(d);
    return vec2<f32>(-b - d, -b + d) / (2.0 * a);
  }
}

fn PlanetIntersection(rayStart: vec3<f32>, rayDir: vec3<f32>) -> vec2<f32> { return SphereIntersection(rayStart, rayDir, GetPlanetCenter(), PLANET_RADIUS); }
fn AtmosphereIntersection(rayStart: vec3<f32>, rayDir: vec3<f32>) -> vec2<f32> {
  return SphereIntersection(rayStart, rayDir, GetPlanetCenter(), PLANET_RADIUS + ATMOSPHERE_HEIGHT);
}

// -------------------------------------
// Phase functions
fn PhaseRayleigh(costh: f32) -> f32 { return 3.0 * (1.0 + costh * costh) / (16.0 * PI); }

const MieGDefault : f32 = 0.85;
fn PhaseMie(costh: f32, g_: f32) -> f32 {
  let g = min(g_, 0.9381);
  var k: f32 = 1.55 * g - 0.55 * g * g * g;
  var kcosth: f32 = k * costh;
  return (1.0 - k * k) / ((4.0 * PI) * (1.0 - kcosth) * (1.0 - kcosth));
}

// -------------------------------------
// Atmosphere
fn AtmosphereHeight(positionWS: vec3<f32>) -> f32 { return distance(positionWS, GetPlanetCenter()) - PLANET_RADIUS; }
fn DensityRayleigh(h: f32) -> f32 { return exp(-max(0.0, h / GetRayleighHeight())); }
fn DensityMie(h: f32) -> f32 { return exp(-max(0.0, h / GetMieHeight())); }
fn DensityOzone(h: f32) -> f32 {
  // The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
  return max(0.0, 1.0 - abs(h - 25000.0) / 15000.0);
}
fn AtmosphereDensity(h: f32) -> vec3<f32> { return vec3(DensityRayleigh(h), DensityMie(h), DensityOzone(h)); }

// Optical depth is a unitless measurement of the amount of absorption of a participating medium (such as the atmosphere).
// This function calculates just that for our three atmospheric elements:
// R: Rayleigh
// G: Mie
// B: Ozone
// If you find the term "optical depth" confusing, you can think of it as "how much density was found along the ray in total".
fn IntegrateOpticalDepth(rayStart: vec3<f32>, rayDir: vec3<f32>) -> vec3<f32> {
  var intersection: vec2<f32> = AtmosphereIntersection(rayStart, rayDir);
  var rayLength: f32 = intersection.y;

  var sampleCount: i32 = 8;
  var stepSize: f32 = rayLength / f32(sampleCount);

  var opticalDepth: vec3<f32> = vec3(0.0);

  var i : i32 = 0;
  loop {
    if i >= sampleCount { break; }
    var localPosition: vec3<f32> = rayStart + rayDir * (f32(i) + 0.5) * stepSize;
    var localHeight: f32 = AtmosphereHeight(localPosition);
    var localDensity: vec3<f32> = AtmosphereDensity(localHeight);

    opticalDepth = opticalDepth + localDensity * f32(stepSize);
    i = i + 1;
  }

  return opticalDepth;
}

// Calculate a luminance transmittance value from optical depth.
fn Absorb(opticalDepth: vec3<f32>) -> vec3<f32> {
  // Note that Mie results in slightly more light absorption than scattering, about 10%
  return exp(-(opticalDepth.x * C_RAYLEIGH * C_SCALE + opticalDepth.y * C_MIE * C_SCALE * 1.1 + opticalDepth.z * C_OZONE * C_SCALE) * ATMOSPHERE_DENSITY);
}

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
fn IntegrateScattering(rayStart_: vec3<f32>, rayDir: vec3<f32>, rayLength_: f32, lightDir: vec3<f32>, lightColor: vec3<f32>, transmittance: ptr<function, vec3<f32>>) -> vec3<f32>  {
  // We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
  // This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
  var rayHeight: f32 = AtmosphereHeight(rayStart_);
  var sampleDistributionExponent: f32 =
      1.0 + clamp(1.0 - rayHeight / ATMOSPHERE_HEIGHT, 0.0, 1.0) * 8.0; // Slightly arbitrary max exponent of 9

  var rayStart = rayStart_;
  var intersection: vec2<f32> = AtmosphereIntersection(rayStart, rayDir);
  var rayLength = min(rayLength_, intersection.y);
  if (intersection.x > 0.0) {
    // Advance ray to the atmosphere entry point
    rayStart = rayStart + rayDir * intersection.x;
    rayLength = rayLength - intersection.x;
  }

  var costh: f32 = dot(rayDir, lightDir);
  var phaseR: f32 = PhaseRayleigh(costh);
  var phaseM: f32 = PhaseMie(costh, MieGDefault);

  let sampleCount: i32 = 64;

  var opticalDepth: vec3<f32> = vec3(0.0);
  var rayleigh: vec3<f32> = vec3(0.0);
  var mie: vec3<f32> = vec3(0.0);

  var prevRayTime: f32 = 0.0;

  var i : i32 = 0;
  loop {
    if i >= sampleCount { break; }
    var rayTime: f32 = pow(f32(i) / f32(sampleCount), sampleDistributionExponent) * rayLength;
    // Because we are distributing the samples exponentially, we have to calculate the step size per sample.
    var stepSize: f32 = (rayTime - prevRayTime);

    var localPosition: vec3<f32> = rayStart + rayDir * rayTime;
    var localHeight: f32 = AtmosphereHeight(localPosition);
    var localDensity: vec3<f32> = AtmosphereDensity(localHeight);

    opticalDepth = opticalDepth +  localDensity * stepSize;

    // The atmospheric transmittance from rayStart to localPosition
    var viewTransmittance: vec3<f32> = Absorb(opticalDepth);

    var opticalDepthlight: vec3<f32> = IntegrateOpticalDepth(localPosition, lightDir);
    // The atmospheric transmittance of light reaching localPosition
    var lightTransmittance: vec3<f32> = Absorb(opticalDepthlight);

    rayleigh = rayleigh +  viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
    mie = mie +  viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

    prevRayTime = rayTime;
    i = i + 1;
  }

  *transmittance = Absorb(opticalDepth);

  return (rayleigh * C_RAYLEIGH * C_SCALE + mie * C_MIE * C_SCALE) * lightColor * EXPOSURE;
}
