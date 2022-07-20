// The MIT License
// Copyright © 2014 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// The MIT License
// Copyright © 2021 Alexander Radkov
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// The MIT License
// Copyright © 2022 Fragcolor
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//--------------------------------------------------------------------------------------
// Signed Distance Functions
//--------------------------------------------------------------------------------------
let PI = 3.14159265358979323846;

//fn dot2(v: vec2<f32>) -> f32
//{ return dot(v, v); }

//fn dot2(v: vec3<f32>) -> f32
//{ return dot(v, v); }

fn ndot(a: vec2<f32>, b: vec2<f32>) -> f32
{ return a.x * b.x - a.y * b.y; }

// ------------------------------------
// Signed Distance Functions
// ------------------------------------

// Union between d1 and d2
fn opUnion(d1: f32, d2: f32) -> f32
{ return min(d1, d2); }

// Union between d1 and d2, but it also returns the blend (lerp) factor
fn opUnionFactor(d1: f32, d2: f32, factor: ptr<private, f32>) -> f32
{
    let newDist: f32 = min(d1, d2);
    if (d1 < d2)
      { *factor = 0.f; }
    else
      { *factor = 1.f; }
    return newDist;
}

// Subtracts d1 from d2
fn opSubtraction(d1: f32, d2: f32)  -> f32
{ return max(-d1, d2); }

// Subtracts d1 from d2, but it also returns the blend (lerp) factor
fn opSubtractionFactor(d1: f32, d2: f32, factor: ptr<private, f32>) -> f32
{
    let newDist: f32 = max(-d1, d2);
    if (-d1 > d2)
      { *factor = 0.f; }
    else
      { *factor = 1.f; }
    return newDist;
}

// Intersection between d1 and d2
fn opIntersection(d1: f32, d2: f32) -> f32
{ return max(d1, d2); }

// Intersection between d1 and d2, but it also returns the blend (lerp) factor
fn opIntersectionFactor(d1: f32, d2: f32, factor: ptr<private, f32>) -> f32
{
    let newDist: f32 = max(d1, d2);
    if (d1 > d2)
      { *factor = 0.f; }
    else
      { *factor = 1.f; }
    return newDist;
}

fn opSmoothUnion(d1: f32, d2: f32, k: f32) -> f32
{
    let h: f32 = max(k - abs(d1 - d2), 0.0);
    return min(d1, d2) - h * h * 0.25 / k;
}

fn opSmoothUnionFactor(d1: f32, d2: f32, k: f32, factor: ptr<private, f32>) -> f32
{
    let h: f32 = max(k - abs(d1 - d2), 0.0);
    let newDist: f32 = min(d1, d2) - h * h * 0.25 / k;
    let hMid: f32 = clamp(h / k, 0.0, 1.0) * 0.5f;
    if (d1 < d2)
      { *factor = hMid; }
    else
      { *factor = 1.f - hMid; }
    return newDist;
}

// Subtract d1 from d2
fn opSmoothSubtraction(d1: f32, d2: f32, k: f32) -> f32
{
    let h: f32 = max(k - abs(-d1 - d2), 0.0);
    return max(-d1, d2) + h * h * 0.25 / k;
}

// Subtract d1 from d2
fn opSmoothSubtractionFactor(d1: f32, d2: f32, k: f32, factor: ptr<private, f32>) -> f32
{
    let h: f32 = max(k - abs(-d1 - d2), 0.0);
    let newDist: f32 = max(-d1, d2) + h * h * 0.25 / k;
    let hMid: f32 = clamp(h / k, 0.0, 1.0) * 0.5f;
    if (-d1 > d2)
      { *factor = hMid; }
    else
      { *factor = 1.f - hMid; }
    return newDist;
}

fn opSmoothIntersection(d1: f32, d2: f32, k: f32) -> f32
{
    let h: f32 = max(k - abs(d1 - d2), 0.0);
    return max(d1, d2) + h * h * 0.25 / k;
}

fn opSmoothIntersectionFactor(d1: f32, d2: f32, k: f32, factor: ptr<private, f32>) -> f32
{
    let h: f32 = max(k - abs(d1 - d2), 0.0);
    let newDist: f32 = max(d1, d2) + h * h * 0.25 / k;
    let hMid: f32 = clamp(h / k, 0.0, 1.0) * 0.5f;
    if (d1 > d2)
      { *factor = hMid; }
    else
      { *factor = 1.f - hMid; }
    return newDist;
}

//-------------------------------------------------
fn rand(co: vec2<f32>) -> f32
{
    let a: f32 = 12.9898f;
    let b: f32 = 78.233f;
    let c: f32 = 43758.5453f;

    return fract(sin(dot(co.xy, vec2<f32>(a, b))) * c);
}

