// The MIT License
// Copyright © 2022 Fragcolor
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//--------------------------------------------------------------------------------------
// Math Functions
//--------------------------------------------------------------------------------------

// * denotes a scalar multiplication or dot product
// x denotes a cross product
// ^ denotes to the power of
// a is a scalar. If there are multiple scalars, use a, b, c etc.
// A is a matrix. If there are multiple matrices, use A, B, C etc.
// v is a vector. If there are multiple vectors, use vA, vB, vC etc.
// qA is a quaternion. If there are multiple quaternions, use qA, qB, qC etc.
// i is the imaginary number. For quaternions there are three such bases, i, j and k.

// Define a Quaternion
// We use x*i + y*j + z*k + w where:
// i, j and k are the basic quaternions where:
//   i*j = k, j*k = i, k*i = j and
//   i^2 = j^2 = k^2 = ijk = -1
// x, y and z are real number scalars
// w is a real number and the real number component of the quaternion
// The decision to put the real number component at the end is for practical reasons.
type Quaternion = vec4<f32>;

// Multiplies two quaternions qC = qA * qB
// Note that qA * qB != qB * qA

fn multi(qA: Quaternion, qB: Quaternion) -> Quaternion
{
    var qC: Quaternion;
    // TODO .xyz reference type?
    let qCross = cross(qA.xyz, qB.xyz) + qA.w * qB.xyz + qB.w * qA.xyz;
    qC.x = qCross.x;
    qC.y = qCross.y;
    qC.z = qCross.z;
    qC.w = qA.w * qB.w - dot(qA.xyz, qB.xyz);
    return qC;
}

// Returns the conjugate of q which is q* = -x*i -b*j -z*k + w
fn conj(qA: Quaternion) -> Quaternion
{
    return Quaternion(-qA.xyz, qA.w);
}

// Rotate a point by a quaternion rotation (point or a vector originating from O) 
fn rot(v: vec3<f32>, q: Quaternion) -> vec3<f32>
{
    return multi(multi(q, Quaternion(v.xyz, 0.0f)), conj(q)).xyz;
}

// Transform a point from object space to world space using the object's translation, scale and rotation
// oPos - position (point) in object space
// t - translation of the object in world space
// s - uniform scale of the object in world space
// q - quaternion rotation of the object in world space
fn ObjectToWorldPos(oPos: vec3<f32>, t: vec3<f32>, s: f32, q: Quaternion) -> vec3<f32>
{
    return rot(oPos, q) * s + t;
}

// Transform a point from world space to an object's space using that object's translation, scale and rotation
// wPos - position (point) in world space
// t - translation of the object in world space
// s - uniform scale of the object in world space
// q - quaternion rotation of the object in world space
fn WorldToObjectPos(wPos: vec3<f32>, t: vec3<f32>, s: f32, q: Quaternion) -> vec3<f32>
{
    return rot((wPos - t)/s, conj(q));
}

// Transform a normalized vector from object space to world space using the object's rotation
// oVec - vector in object space
// q - quaternion rotation of the object in world space
fn ObjectToWorldVec(oVec: vec3<f32>, q: Quaternion) -> vec3<f32>
{
    return rot(oVec, q);
}

// Transform a normalized vector from world space to an object's space using that object's rotation
// wVec - normalized vector in world space
// t - translation of the object in world space
// s - uniform scale of the object in world space
// q - quaternion rotation of the object in world space
fn WorldToObjectVec(wVec: vec3<f32>, q: Quaternion) -> vec3<f32>
{
    return rot(wVec, conj(q));
}
