#ifndef COMMONBLUR
#define COMMONBLUR

// base
#define KERNEL_SIZE 5
const float weight_0 = 0.2270270270;
const vec4 weights = vec4(0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

// linear sample trick
const float offset_lerp[3] = float[](0.0, 1.3846153846, 3.2307692308);

const float weight_lerp[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

#endif