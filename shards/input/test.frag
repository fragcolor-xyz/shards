// layout(binding = 0) uniform Main {
//   vec2 iResolution;
//   float iTime;
// };

// Cheap vec3 to vec3 hash. Works well enough, but there are other ways.
vec3 hash33(vec3 p) {
  float n = sin(dot(p, vec3(7.0, 157.0, 113.0)));
  return fract(vec3(2097152.0, 262144.0, 32768.0) * n);
}

float map(vec3 p) {
  // Creating the repeat cubes, with slightly convex faces. Standard,
  // flat faced cubes don't capture the light quite as well.

  // Cube center offset, to create a bit of disorder, which breaks the
  // space up a little.
  vec3 o = hash33(floor(p)) * 0.2;

  // 3D space repetition.
  p = fract(p + o) - .5;

  // A bit of roundness. Used to give the cube faces a touch of convexity.
  float r = dot(p, p) - 0.21;

  // Max of abs(x), abs(y) and abs(z) minus a constant gives a cube.
  // Adding a little bit of "r," above, rounds off the surfaces a bit.
  p = abs(p);
  return max(max(p.x, p.y), p.z) * .95 + r * 0.05 - 0.21;

  // Alternative. Egg shapes... kind of.
  // float perturb = sin(p.x*10.)*sin(p.y*10.)*sin(p.z*10.);
  // p += hash33(floor(p))*.2;
  // return length(fract(p)-.5)-0.25 + perturb*0.05;
}

// in vec2 fragCoord;
// out vec4 fragColor;
vec4 mainF(vec2 fragCoord, mat4 camera, mat4 obj, vec2 iResolution, float iTime) {
  // Screen coordinates.
  vec2 uv = (fragCoord - iResolution.xy * .5) / iResolution.y;
  uv.y = -uv.y;

  // Unit direction ray. The last term is one of many ways to fish-lens the camera.
  // For a regular view, set "rd.z" to something like "0.5."
  vec3 rd = normalize(vec3(uv, -(1. - dot(uv, uv) * .5) * .5));
  // rd = normalize(vec3(uv, -0.5));

  // There are a few ways to hide artifacts and inconsistencies. Making things go fast is one of them. :)
  // Ray origin, scene color, and surface postion vector.
  vec3 ro = vec3(0., 0., 0.);
  vec3 col = vec3(0.);
  vec3 sp = vec3(0.);

  // Swivel the unit ray to look around the scene.
  ro = (camera * vec4(ro, 1.)).xyz;
  rd = (camera * vec4(rd, 0.)).xyz;
  // float cs = cos(iTime * 0.375), si = sin(iTime * 0.375);
  // rd.xz = mat2(cs, si, -si, cs) * rd.xz;
  // rd.xy = mat2(cs, si, -si, cs) * rd.xy;

  // Unit ray jitter is another way to hide artifacts. It can also trick the viewer into believing
  // something hard core, like global illumination, is happening. :)
  const float j = 0.035;
  rd *= (1.0-j) + hash33(rd) * j;

  // Ray distance, bail out layer number, surface distance and normalized accumulated distance.
  float t = 0., layers = 0., d, aD;

  // Surface distance threshold. Smaller numbers give a sharper object. I deliberately
  // wanted some blur, so bumped it up slightly.
  float thD = .035; // + smoothstep(-0.2, 0.2, sin(iTime*0.75 - 3.14159*0.4))*0.025;
  thD = .025;

  // Only a few iterations seemed to be enough. Obviously, more looks better, but is slower.
  for (int i = 0; i < 56; i++) {

    // Break conditions. Anything that can help you bail early usually increases frame rate.
    if (layers > 15. || col.x > 1. || t > 10.)
      break;

    // Current ray postion. Slightly redundant here, but sometimes you may wish to reuse
    // it during the accumulation stage.
    sp = ro + rd * t;

    d = map(sp); // Distance to nearest point in the cube field.

    // If we get within a certain distance of the surface, accumulate some surface values.
    // Values further away have less influence on the total.
    //
    // aD - Accumulated distance. I interpolated aD on a whim (see below), because it seemed
    // to look nicer.
    //
    // 1/.(1. + t*t*.25) - Basic distance attenuation. Feel free to substitute your own.

    // Normalized distance from the surface threshold value to our current isosurface value.
    aD = (thD - abs(d) * 15. / 16.) / thD;

    // If we're within the surface threshold, accumulate some color.
    // Two "if" statements in a shader loop makes me nervous. I don't suspect there'll be any
    // problems, but if there are, let us know.
    if (aD > 0.) {
      // Smoothly interpolate the accumulated surface distance value, then apply some
      // basic falloff (fog, if you prefer) using the camera to surface distance, "t."
      col += aD * aD * (3. - 2. * aD) / (1. + t * t * .25) * .2;
      layers++;
    }

    // Kind of weird the way this works. I think not allowing the ray to hone in properly is
    // the very thing that gives an even spread of values. The figures are based on a bit of
    // knowledge versus trial and error. If you have a faster computer, feel free to tweak
    // them a bit.
    t += max(abs(d) * .7, thD * 1.5);
  }

  // I'm virtually positive "col" doesn't drop below zero, but just to be safe...
  col = max(col, 0.);

  // Mixing the greytone color and some firey orange with a sinusoidal pattern that
  // was completely made up on the spot.
  col = mix(col, vec3(min(col.x * 1.5, 1.), pow(col.x, 2.5), pow(col.x, 12.)),
            dot(sin(rd.yzx * 8. + sin(rd.zxy * 8.)), vec3(.1666)) + 0.4);

  // Doing the same again, but this time mixing in some green. I might have gone overboard
  // applying this step. Commenting it out probably looks more sophisticated.
  col =
      mix(col, vec3(col.x * col.x * .85, col.x, col.x * col.x * .3), dot(sin(rd.yzx * 4. + sin(rd.zxy * 4.)), vec3(.1666)) + .25);

  // Presenting the color to the screen -- Note that there isn't any gamma correction. That
  // was a style choice.
  return vec4(clamp(col, 0., 1.), 1);
}

void main() {}
