#version 330

uniform float uTime;
uniform vec2 uResolution;

// basic sphere
float sphereDist(vec3 ro, vec3 rd) {
    vec3 c = vec3(0.0, 0.0, -3.0);
    float r = 1.0;
    vec3 oc = ro - c;

    float b = dot(oc, rd);
    float c2 = dot(oc, oc) - r*r;
    float h = b*b - c2;
    if (h < 0.0) return -1.0;
    return -b - sqrt(h);
}

void main() {
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;

    vec3 ro = vec3(0.0, 0.0, 0.0);
    vec3 rd = normalize(vec3(uv, -1.0));

    float t = sphereDist(ro, rd);

    if (t > 0.0) {
        vec3 p = ro + rd * t;
        vec3 n = normalize(p - vec3(0.0, 0.0, -3.0));
        float light = max(dot(n, normalize(vec3(1.0, 1.0, -1.0))), 0.0);
        gl_FragColor = vec4(vec3(light), 1.0);
    } else {
        gl_FragColor = vec4(0.1, 0.1, 0.15, 1.0);
    }
}