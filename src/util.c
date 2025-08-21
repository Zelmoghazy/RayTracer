#include "../include/util.h"

void log_error(int error_code, const char* file, int line)
{
    if (error_code != 0){
        fprintf(stderr,"Error %d, In file: %s, Line : %d \n",error_code, file, line);
        exit(1);
    }
}

void* check_ptr(void *ptr, const char* file, int line)
{
    if(ptr == NULL){
        fprintf(stderr, "In file: %s, Line : %d \n", file, line);
        exit(1);
    }
    return ptr;
}

void swap(int* a, int* b) 
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

static unsigned int g_seed;

inline void fast_srand(int seed) 
{
    g_seed = seed;
}

// Output value in range [0, 32767]
inline int fast_rand(void) 
{
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

void buf_append(abuf *ab, const char *s, int len) 
{
    char *new_buf = (char *)realloc(ab->b, ab->len + len);
    if (!new_buf) return;
    memcpy(&new_buf[ab->len], s, len);
    ab->b = new_buf;
    ab->len += len;
}

void buf_free(abuf *ab) 
{
    free(ab->b);
}

vec2f_t vec2f(float x, float y)
{
    vec2f_t vec2 = {
        .x = x,
        .y = y,
    };
    return  vec2;
}

vec2f_t vec2f_add(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x + b.x;
    vec2.y = a.y + b.y;
    return vec2;
}

vec2f_t vec2f_sub(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x - b.x;
    vec2.y = a.y - b.y;
    return vec2;
}

vec2f_t vec2f_mul(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x * b.x;
    vec2.y = a.y * b.y;
    return vec2;
}

vec2f_t vec2f_div(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x / b.x;
    vec2.y = a.y / b.y;
    return vec2;
}


vec3f_t vec3f_add(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x + b.x, a.y + b.y, a.z + b.z};
    return result;
}

vec3f_t vec3f_sub(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x - b.x, a.y - b.y, a.z - b.z};
    return result;
}

vec3f_t vec3f_mul(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {a.x * b.x, a.y * b.y, a.z * b.z};
    return result;
}

vec3f_t vec3f_scale(vec3f_t v, f32 scalar) 
{
    vec3f_t result = {v.x * scalar, v.y * scalar, v.z * scalar};
    return result;
}

f32 vec3f_dot(vec3f_t a, vec3f_t b) 
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3f_t vec3f_cross(vec3f_t a, vec3f_t b) 
{
    vec3f_t result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}

f32 vec3f_length_sq(vec3f_t v) 
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

f32 vec3f_length(vec3f_t v) 
{
    return sqrt_f32((vec3f_length_sq(v)));
}

vec3f_t vec3f_unit(vec3f_t v) 
{
    f32 len = vec3f_length(v);
    
    if (len > 0.0f) {
        f32 inv_len = 1.0f / len;
        return (vec3f_t){ v.x * inv_len, v.y * inv_len, v.z * inv_len };
    }
    return (vec3f_t){ 0.0f, 0.0f, 0.0f }; // Return zero vector if input is zero
}

vec3f_t vec3f_normalize(vec3f_t v) 
{
    f32 len = vec3f_length(v);
    if (len > 0.0f) {
        f32 inv_len = 1.0f / len;
        return vec3f_scale(v, inv_len);
    }
    return v; // return zero vector if length is zero
}

// returns an linearly interpolated vector based on blend factor 0.0 -> 1.0
vec3f_t vec3f_lerp(vec3f_t a, vec3f_t b, float blend_factor)
{
    return vec3f_add(vec3f_scale(a,(1.0-blend_factor)), vec3f_scale(b,blend_factor));
}

vec3f_t vec3f_random()
{
    return (vec3f_t){RAND_FLOAT(),RAND_FLOAT(),RAND_FLOAT()};
}

vec3f_t vec3f_random_range(float min, float max)
{
    return (vec3f_t){RAND_FLOAT_RANGE(min, max),RAND_FLOAT_RANGE(min, max),RAND_FLOAT_RANGE(min, max)};
}

// direction vector of random points in a unit sphere
vec3f_t vec3f_random_direction()
{
    for(size_t i = 0; i < 100; i++)
    {
        vec3f_t point_in_cube = vec3f_random_range(-1,1);
        f32 lensq = vec3f_length_sq(point_in_cube);
        // check if the point is inside a unit sphere
        if(1e-15 < lensq && lensq <= 1)
        {
            // normalize to unit length
            return vec3f_scale(point_in_cube,1.0f/sqrt_f32(lensq));
        }
    }
    return (vec3f_t){0.0f,0.0f,0.0f};
}

bool vec3f_is_near_zero(vec3f_t vec)
{
    f32 s = 1e-8;
    return (abs_f32(vec.x) < s) && (abs_f32(vec.y) < s) && (abs_f32(vec.z) < s);
}

vec3f_t vec3f_reflect(vec3f_t v, vec3f_t n)
{
    return vec3f_sub(v, vec3f_scale(n, 2.0f*vec3f_dot(v, n)));
}

/*
    Column major order
*/
mat4x4_t mat4x4_mult(mat4x4_t const *m, mat4x4_t const *n)
{
    f32 const m00 = m->values[0];
    f32 const m01 = m->values[1];
    f32 const m02 = m->values[2];
    f32 const m03 = m->values[3];
    f32 const m10 = m->values[4];
    f32 const m11 = m->values[5];
    f32 const m12 = m->values[6];
    f32 const m13 = m->values[7];
    f32 const m20 = m->values[8];
    f32 const m21 = m->values[9];
    f32 const m22 = m->values[10];
    f32 const m23 = m->values[11];
    f32 const m30 = m->values[12];
    f32 const m31 = m->values[13];
    f32 const m32 = m->values[14];
    f32 const m33 = m->values[15];

    f32 const n00 = n->values[0];
    f32 const n01 = n->values[1];
    f32 const n02 = n->values[2];
    f32 const n03 = n->values[3];
    f32 const n10 = n->values[4];
    f32 const n11 = n->values[5];
    f32 const n12 = n->values[6];
    f32 const n13 = n->values[7];
    f32 const n20 = n->values[8];
    f32 const n21 = n->values[9];
    f32 const n22 = n->values[10];
    f32 const n23 = n->values[11];
    f32 const n30 = n->values[12];
    f32 const n31 = n->values[13];
    f32 const n32 = n->values[14];
    f32 const n33 = n->values[15];

    mat4x4_t res;

    res.values[0]  = m00*n00+m10*n01+m20*n02+m30*n03;
    res.values[1]  = m01*n00+m11*n01+m21*n02+m31*n03;
    res.values[2]  = m02*n00+m12*n01+m22*n02+m32*n03;
    res.values[3]  = m03*n00+m13*n01+m23*n02+m33*n03;
    res.values[4]  = m00*n10+m10*n11+m20*n12+m30*n13;
    res.values[5]  = m01*n10+m11*n11+m21*n12+m31*n13;
    res.values[6]  = m02*n10+m12*n11+m22*n12+m32*n13;
    res.values[7]  = m03*n10+m13*n11+m23*n12+m33*n13;
    res.values[8]  = m00*n20+m10*n21+m20*n22+m30*n23;
    res.values[9]  = m01*n20+m11*n21+m21*n22+m31*n23;
    res.values[10] = m02*n20+m12*n21+m22*n22+m32*n23;
    res.values[11] = m03*n20+m13*n21+m23*n22+m33*n23;
    res.values[12] = m00*n30+m10*n31+m20*n32+m30*n33;
    res.values[13] = m01*n30+m11*n31+m21*n32+m31*n33;
    res.values[14] = m02*n30+m12*n31+m22*n32+m32*n33;
    res.values[15] = m03*n30+m13*n31+m23*n32+m33*n33;
    
    return res;
}


mat4x4_t mat4x4_mult_simd(mat4x4_t const *m, mat4x4_t const *n)
{
    mat4x4_t res;
    
    // Load columns of matrix m
    __m128 col0 = _mm_load_ps(&m->values[0]);
    __m128 col1 = _mm_load_ps(&m->values[4]);
    __m128 col2 = _mm_load_ps(&m->values[8]);
    __m128 col3 = _mm_load_ps(&m->values[12]);
    
    // Process each column of result
    for (int i = 0; i < 4; i++) 
    {
        __m128 n_col = _mm_load_ps(&n->values[i*4]);
        
        __m128 result = _mm_mul_ps(col0, _mm_shuffle_ps(n_col, n_col, 0x00));               // first element of n_col
        result = _mm_add_ps(result, _mm_mul_ps(col1, _mm_shuffle_ps(n_col, n_col, 0x55)));  // second element of n_col
        result = _mm_add_ps(result, _mm_mul_ps(col2, _mm_shuffle_ps(n_col, n_col, 0xAA)));  // third element of n_col
        result = _mm_add_ps(result, _mm_mul_ps(col3, _mm_shuffle_ps(n_col, n_col, 0xFF)));  // fourth element of n_col
        
        _mm_store_ps(&res.values[i*4], result);
    }
    
    return res;
}

mat4x4_t mat_perspective(f32 n, f32 f, f32 fovY, f32 aspect_ratio)
{
    f32 top   = n * tanf(fovY / 2.f);
    f32 right = top * aspect_ratio;

    return (mat4x4_t) {
        n / right,      0.f,       0.f,                    0.f,
        0.f,            n / top,   0.f,                    0.f,
        0.f,            0.f,       -(f + n) / (f - n),     - 2.f * f * n / (f - n),
        0.f,            0.f,       -1.f,                   0.f,
    };
}

mat4x4_t mat_identity(void)
{
    return (mat4x4_t){
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_scale(vec3f_t s)
{
    return (mat4x4_t) {
        s.x,  0.f,  0.f,  0.f,
        0.f,  s.y,  0.f,  0.f,
        0.f,  0.f,  s.z,  0.f,
        0.f,  0.f,  0.f,  1.f,
    };
}

mat4x4_t mat_scale_const(f32 s)
{
    vec3f_t vec = {
        .x = s,
        .y = s,
        .z = s,
    };
    return mat_scale(vec);
}

mat4x4_t mat_translate(vec3f_t s)
{
    return (mat4x4_t){
        1.f, 0.f, 0.f, s.x,
        0.f, 1.f, 0.f, s.y,
        0.f, 0.f, 1.f, s.z,
        0.f, 0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_rotate_xy(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
        cos, -sin, 0.f, 0.f,
        sin,  cos, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
        0.f,  0.f, 0.f, 1.f,
    };
}

mat4x4_t mat_rotate_yz(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
        1.f, 0.f,  0.f, 0.f,
        0.f, cos, -sin, 0.f,
        0.f, sin,  cos, 0.f,
        0.f, 0.f,  0.f, 1.f,
    };
}

mat4x4_t mat_rotate_zx(f32 angle)
{
    f32 cos = cosf(angle);
    f32 sin = sinf(angle);

    return (mat4x4_t){
         cos, 0.f, sin, 0.f,
         0.f, 1.f, 0.f, 0.f,
        -sin, 0.f, cos, 0.f,
         0.f, 0.f, 0.f, 1.f,
    };
}

float trig_values[] = { 
    0.0000f,0.0175f,0.0349f,0.0523f,0.0698f,0.0872f,0.1045f,0.1219f,
    0.1392f,0.1564f,0.1736f,0.1908f,0.2079f,0.2250f,0.2419f,0.2588f,
    0.2756f,0.2924f,0.3090f,0.3256f,0.3420f,0.3584f,0.3746f,0.3907f,
    0.4067f,0.4226f,0.4384f,0.4540f,0.4695f,0.4848f,0.5000f,0.5150f,
    0.5299f,0.5446f,0.5592f,0.5736f,0.5878f,0.6018f,0.6157f,0.6293f,
    0.6428f,0.6561f,0.6691f,0.6820f,0.6947f,0.7071f,0.7071f,0.7193f,
    0.7314f,0.7431f,0.7547f,0.7660f,0.7771f,0.7880f,0.7986f,0.8090f,
    0.8192f,0.8290f,0.8387f,0.8480f,0.8572f,0.8660f,0.8746f,0.8829f,
    0.8910f,0.8988f,0.9063f,0.9135f,0.9205f,0.9272f,0.9336f,0.9397f,
    0.9455f,0.9511f,0.9563f,0.9613f,0.9659f,0.9703f,0.9744f,0.9781f,
    0.9816f,0.9848f,0.9877f,0.9903f,0.9925f,0.9945f,0.9962f,0.9976f,
    0.9986f,0.9994f,0.9998f,1.0000f
};

float sine(int x)
{
    x = x % 360;
    if (x < 0) {
        x += 360;
    }
    if (x == 0){
        return 0;
    }else if (x == 90){
        return 1;
    }else if (x == 180){
        return 0;
    }else if (x == 270){
        return -1;
    }

    if(x > 270){
        return - trig_values[360-x];
    }else if(x>180){
        return - trig_values[x-180];
    }else if(x>90){
        return trig_values[180-x];
    }else{
        return trig_values[x];
    }
}

float cosine(int x){
    return sine(90-x);
}
