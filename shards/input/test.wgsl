fn hash33_(p: vec3<f32>) -> vec3<f32> {
    var p_1: vec3<f32>;
    var n: f32;

    p_1 = p;
    _ = p_1;
    _ = vec3<f32>(7.0, 157.0, 113.0);
    let _e7 = p_1;
    _ = dot(_e7, vec3<f32>(7.0, 157.0, 113.0));
    _ = p_1;
    _ = vec3<f32>(7.0, 157.0, 113.0);
    let _e18 = p_1;
    n = sin(dot(_e18, vec3<f32>(7.0, 157.0, 113.0)));
    let _e30 = n;
    _ = (vec3<f32>(2097152.0, 262144.0, 32768.0) * _e30);
    let _e36 = n;
    return fract((vec3<f32>(2097152.0, 262144.0, 32768.0) * _e36));
}

fn map(p_2: vec3<f32>) -> f32 {
    var p_3: vec3<f32>;
    var o: vec3<f32>;
    var r: f32;

    p_3 = p_2;
    _ = p_3;
    let _e3 = p_3;
    _ = floor(_e3);
    _ = p_3;
    let _e6 = p_3;
    let _e8 = hash33_(floor(_e6));
    o = (_e8 * 0.20000000298023224);
    let _e12 = p_3;
    let _e13 = o;
    _ = (_e12 + _e13);
    let _e15 = p_3;
    let _e16 = o;
    p_3 = (fract((_e15 + _e16)) - vec3<f32>(0.5));
    _ = p_3;
    _ = p_3;
    let _e24 = p_3;
    let _e25 = p_3;
    r = (dot(_e24, _e25) - 0.20999999344348907);
    _ = p_3;
    let _e31 = p_3;
    p_3 = abs(_e31);
    let _e33 = p_3;
    _ = _e33.x;
    let _e35 = p_3;
    _ = _e35.y;
    let _e37 = p_3;
    let _e39 = p_3;
    _ = max(_e37.x, _e39.y);
    let _e42 = p_3;
    _ = _e42.z;
    let _e44 = p_3;
    _ = _e44.x;
    let _e46 = p_3;
    _ = _e46.y;
    let _e48 = p_3;
    let _e50 = p_3;
    let _e53 = p_3;
    let _e58 = r;
    return (((max(max(_e48.x, _e50.y), _e53.z) * 0.949999988079071) + (_e58 * 0.05000000074505806)) - 0.20999999344348907);
}

fn mainF(fragCoord: vec2<f32>, camera: mat4x4<f32>, obj: mat4x4<f32>, iResolution: vec2<f32>, iTime: f32) -> vec4<f32> {
    var fragCoord_1: vec2<f32>;
    var camera_1: mat4x4<f32>;
    var obj_1: mat4x4<f32>;
    var iResolution_1: vec2<f32>;
    var iTime_1: f32;
    var uv: vec2<f32>;
    var rd: vec3<f32>;
    var ro: vec3<f32>;
    var col: vec3<f32>;
    var sp: vec3<f32>;
    var j: f32;
    var t: f32;
    var layers: f32;
    var d: f32;
    var aD: f32;
    var thD: f32;
    var i: i32;

    fragCoord_1 = fragCoord;
    camera_1 = camera;
    obj_1 = obj;
    iResolution_1 = iResolution;
    iTime_1 = iTime;
    let _e10 = fragCoord_1;
    let _e11 = iResolution_1;
    let _e16 = iResolution_1;
    uv = ((_e10 - (_e11.xy * 0.5)) / vec2<f32>(_e16.y));
    let _e22 = uv;
    uv.y = -(_e22.y);
    let _e25 = uv;
    _ = uv;
    _ = uv;
    let _e29 = uv;
    let _e30 = uv;
    _ = vec3<f32>(_e25.x, _e25.y, (-((1.0 - (dot(_e29, _e30) * 0.5))) * 0.5));
    let _e41 = uv;
    _ = uv;
    _ = uv;
    let _e45 = uv;
    let _e46 = uv;
    rd = normalize(vec3<f32>(_e41.x, _e41.y, (-((1.0 - (dot(_e45, _e46) * 0.5))) * 0.5)));
    ro = vec3<f32>(0.0, 0.0, 0.0);
    col = vec3<f32>(0.0);
    sp = vec3<f32>(0.0);
    let _e70 = camera_1;
    let _e71 = ro;
    ro = (_e70 * vec4<f32>(_e71.x, _e71.y, _e71.z, 1.0)).xyz;
    let _e79 = camera_1;
    let _e80 = rd;
    rd = (_e79 * vec4<f32>(_e80.x, _e80.y, _e80.z, 0.0)).xyz;
    j = 0.03500000014901161;
    let _e90 = rd;
    let _e92 = j;
    _ = rd;
    let _e95 = rd;
    let _e96 = hash33_(_e95);
    let _e97 = j;
    rd = (_e90 * (vec3<f32>((1.0 - _e92)) + (_e96 * _e97)));
    t = 0.0;
    layers = 0.0;
    thD = 0.03500000014901161;
    thD = 0.02500000037252903;
    i = 0;
    loop {
        let _e113 = i;
        if !((_e113 < 56)) {
            break;
        }
        {
            let _e120 = layers;
            let _e123 = col;
            let _e128 = t;
            if (((_e120 > 15.0) || (_e123.x > 1.0)) || (_e128 > 10.0)) {
                break;
            }
            let _e132 = ro;
            let _e133 = rd;
            let _e134 = t;
            sp = (_e132 + (_e133 * _e134));
            _ = sp;
            let _e138 = sp;
            let _e139 = map(_e138);
            d = _e139;
            let _e140 = thD;
            _ = d;
            let _e142 = d;
            let _e149 = thD;
            aD = ((_e140 - ((abs(_e142) * 15.0) / 16.0)) / _e149);
            let _e151 = aD;
            if (_e151 > 0.0) {
                {
                    let _e154 = col;
                    let _e155 = aD;
                    let _e156 = aD;
                    let _e160 = aD;
                    let _e165 = t;
                    let _e166 = t;
                    col = (_e154 + vec3<f32>(((((_e155 * _e156) * (3.0 - (2.0 * _e160))) / (1.0 + ((_e165 * _e166) * 0.25))) * 0.20000000298023224)));
                    let _e176 = layers;
                    layers = (_e176 + 1.0);
                }
            }
            let _e179 = t;
            _ = d;
            let _e181 = d;
            _ = (abs(_e181) * 0.699999988079071);
            let _e185 = thD;
            _ = (_e185 * 1.5);
            _ = d;
            let _e189 = d;
            let _e193 = thD;
            t = (_e179 + max((abs(_e189) * 0.699999988079071), (_e193 * 1.5)));
        }
        continuing {
            let _e117 = i;
            i = (_e117 + 1);
        }
    }
    _ = col;
    let _e200 = col;
    col = max(_e200, vec3<f32>(0.0));
    _ = col;
    let _e205 = col;
    _ = (_e205.x * 1.5);
    let _e210 = col;
    let _e216 = col;
    _ = _e216.x;
    let _e219 = col;
    let _e223 = col;
    _ = _e223.x;
    let _e226 = col;
    _ = vec3<f32>(min((_e210.x * 1.5), 1.0), pow(_e219.x, 2.5), pow(_e226.x, 12.0));
    let _e231 = rd;
    let _e235 = rd;
    _ = (_e235.zxy * 8.0);
    let _e239 = rd;
    _ = ((_e231.yzx * 8.0) + sin((_e239.zxy * 8.0)));
    let _e245 = rd;
    let _e249 = rd;
    _ = (_e249.zxy * 8.0);
    let _e253 = rd;
    _ = sin(((_e245.yzx * 8.0) + sin((_e253.zxy * 8.0))));
    _ = vec3<f32>(0.16660000383853912);
    let _e262 = rd;
    let _e266 = rd;
    _ = (_e266.zxy * 8.0);
    let _e270 = rd;
    _ = ((_e262.yzx * 8.0) + sin((_e270.zxy * 8.0)));
    let _e276 = rd;
    let _e280 = rd;
    _ = (_e280.zxy * 8.0);
    let _e284 = rd;
    _ = (dot(sin(((_e276.yzx * 8.0) + sin((_e284.zxy * 8.0)))), vec3<f32>(0.16660000383853912)) + 0.4000000059604645);
    let _e296 = col;
    let _e297 = col;
    _ = (_e297.x * 1.5);
    let _e302 = col;
    let _e308 = col;
    _ = _e308.x;
    let _e311 = col;
    let _e315 = col;
    _ = _e315.x;
    let _e318 = col;
    let _e323 = rd;
    let _e327 = rd;
    _ = (_e327.zxy * 8.0);
    let _e331 = rd;
    _ = ((_e323.yzx * 8.0) + sin((_e331.zxy * 8.0)));
    let _e337 = rd;
    let _e341 = rd;
    _ = (_e341.zxy * 8.0);
    let _e345 = rd;
    _ = sin(((_e337.yzx * 8.0) + sin((_e345.zxy * 8.0))));
    _ = vec3<f32>(0.16660000383853912);
    let _e354 = rd;
    let _e358 = rd;
    _ = (_e358.zxy * 8.0);
    let _e362 = rd;
    _ = ((_e354.yzx * 8.0) + sin((_e362.zxy * 8.0)));
    let _e368 = rd;
    let _e372 = rd;
    _ = (_e372.zxy * 8.0);
    let _e376 = rd;
    col = mix(_e296, vec3<f32>(min((_e302.x * 1.5), 1.0), pow(_e311.x, 2.5), pow(_e318.x, 12.0)), vec3<f32>((dot(sin(((_e368.yzx * 8.0) + sin((_e376.zxy * 8.0)))), vec3<f32>(0.16660000383853912)) + 0.4000000059604645)));
    _ = col;
    let _e391 = col;
    let _e393 = col;
    let _e398 = col;
    let _e400 = col;
    let _e402 = col;
    _ = vec3<f32>(((_e391.x * _e393.x) * 0.8500000238418579), _e398.x, ((_e400.x * _e402.x) * 0.30000001192092896));
    let _e408 = rd;
    let _e412 = rd;
    _ = (_e412.zxy * 4.0);
    let _e416 = rd;
    _ = ((_e408.yzx * 4.0) + sin((_e416.zxy * 4.0)));
    let _e422 = rd;
    let _e426 = rd;
    _ = (_e426.zxy * 4.0);
    let _e430 = rd;
    _ = sin(((_e422.yzx * 4.0) + sin((_e430.zxy * 4.0))));
    _ = vec3<f32>(0.16660000383853912);
    let _e439 = rd;
    let _e443 = rd;
    _ = (_e443.zxy * 4.0);
    let _e447 = rd;
    _ = ((_e439.yzx * 4.0) + sin((_e447.zxy * 4.0)));
    let _e453 = rd;
    let _e457 = rd;
    _ = (_e457.zxy * 4.0);
    let _e461 = rd;
    _ = (dot(sin(((_e453.yzx * 4.0) + sin((_e461.zxy * 4.0)))), vec3<f32>(0.16660000383853912)) + 0.25);
    let _e473 = col;
    let _e474 = col;
    let _e476 = col;
    let _e481 = col;
    let _e483 = col;
    let _e485 = col;
    let _e491 = rd;
    let _e495 = rd;
    _ = (_e495.zxy * 4.0);
    let _e499 = rd;
    _ = ((_e491.yzx * 4.0) + sin((_e499.zxy * 4.0)));
    let _e505 = rd;
    let _e509 = rd;
    _ = (_e509.zxy * 4.0);
    let _e513 = rd;
    _ = sin(((_e505.yzx * 4.0) + sin((_e513.zxy * 4.0))));
    _ = vec3<f32>(0.16660000383853912);
    let _e522 = rd;
    let _e526 = rd;
    _ = (_e526.zxy * 4.0);
    let _e530 = rd;
    _ = ((_e522.yzx * 4.0) + sin((_e530.zxy * 4.0)));
    let _e536 = rd;
    let _e540 = rd;
    _ = (_e540.zxy * 4.0);
    let _e544 = rd;
    col = mix(_e473, vec3<f32>(((_e474.x * _e476.x) * 0.8500000238418579), _e481.x, ((_e483.x * _e485.x) * 0.30000001192092896)), vec3<f32>((dot(sin(((_e536.yzx * 4.0) + sin((_e544.zxy * 4.0)))), vec3<f32>(0.16660000383853912)) + 0.25)));
    _ = col;
    let _e561 = col;
    let _e566 = clamp(_e561, vec3<f32>(0.0), vec3<f32>(1.0));
    return vec4<f32>(_e566.x, _e566.y, _e566.z, f32(1));
}

fn main_1() {
    return;
}

@fragment 
fn main() {
    main_1();
    return;
}
