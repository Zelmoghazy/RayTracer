#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./external/include/stb_image.h"
#include "./external/include/stb_image_write.h"

#define PROF_IMPLEMENTATION
#include "./include/util.h"
#include "./include/font.h"
#include "./include/prof.h"
#include "./include/arena.h"
#include "./include/base_graphics.h"

typedef struct ray_t
{
    vec3f_t  orig;
    vec3f_t  dir;
}ray_t;

#define RAY_AT(r,t) vec3f_add(r->orig, vec3f_scale(r->dir, t)) 

typedef struct camera_t
{
    vec3f_t pos;
    vec3f_t target;
    vec3f_t up;
    
    vec3f_t u;
    vec3f_t v;
    vec3f_t w;

    f32 vfov;
    f32 speed;

    f32 defocus_angle;
    f32 focus_dist;

    vec3f_t defocus_disk_u;
    vec3f_t defocus_disk_v;

}camera_t;

enum material_type
{
    Lambertian,
    Metal,     
    Dielectric,
    Emissive,  
};

typedef struct material_t
{
    enum material_type mat_type;
    vec3f_t albedo;                 // whiteness
    f32 fuzz;                     // Lambertian
    f32 refraction_index;         // Dielectric
}material_t;

typedef struct hit_record_t
{
    vec3f_t hit_point;      
    vec3f_t norm;
    f32 hit_dist;
    material_t mat;
    bool front_face;
}hit_record_t;

enum object_type{
    Sphere
};

typedef struct sphere_t 
{
    vec3f_t     center;
    f32         radius;
    material_t  mat;
}sphere_t;

typedef struct scene_object_t
{
    enum object_type type;
    void *object;
}scene_object_t;

typedef struct scene_objects_t
{
    scene_object_t *objects;    
    size_t count;               
    size_t capacity;            
} scene_objects_t;

struct context_t
{
    void               *window;
    u32                 screen_width;
    u32                 screen_height;
    image_view_t        draw_buffer;

    GLuint              texture;
    GLuint              read_fbo;

    vec3f_t             pixel00_loc;
    vec3f_t             pixel_delta_u;
    vec3f_t             pixel_delta_v;

    camera_t            camera;

    i32                 samples_per_pixel;
    i32                 max_depth;

    scene_objects_t     *scene_objects;

    u32                 mouseX;
    u32                 mouseLastX;
    u32                 mouseY;
    u32                 mouseLastY;
    bool                firstMouse;
    bool                left_button_down;
    bool                right_button_down;

    u32                 global_scale;

    /* Text */
    font_t              *font;

    /* FLAGS */
    bool                running;
    bool                resize;
    bool                rescale;
    bool                render;
    bool                dock;
    bool                capture;
    bool                profile;
    bool                changed;
    bool                camera_mode;

    /* TIME */
    u32                 start_time;
    f32                 dt;
    u64                 last_render_time;
    u64                 render_interval;
    #ifdef _WIN32
        LARGE_INTEGER   last_frame_start;
    #else
        struct timespec last_frame_start;
    #endif

    arena_t             *frame_arena;
}gc;

char frametime[512];

char prof_buf[64][512];
i32 prof_buf_count;
i32 prof_max_width;

struct  {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
} test_animation = {0,0,200,200};
i32 new_x = 0;
i32 new_y = 0;
bool test_animation_move = false;

void update_camera_view();
void render_all(void);
void increase_fov();
void decrease_fov();
void set_fov(f32 new_fov);
void adjust_fov(f32 delta);

scene_objects_t* scene_array_create(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = 16; // Default initial capacity
    }
    
    scene_objects_t *array = malloc(sizeof(scene_objects_t));

    if (!array) {
        return NULL;
    }
    
    array->objects = malloc(sizeof(scene_object_t) * initial_capacity);

    if (!array->objects) {
        free(array);
        return NULL;
    }
    
    array->count = 0;
    array->capacity = initial_capacity;
    
    return array;
}

i32 scene_array_resize(scene_objects_t* array, size_t new_capacity)
{
    if (!array || new_capacity < array->count) {
        return -1; 
    }
    
    scene_object_t *new_objects = realloc(array->objects, sizeof(scene_object_t) * new_capacity);
    if (!new_objects) {
        return -1; // Memory allocation failed
    }
    
    array->objects = new_objects;
    array->capacity = new_capacity;
    
    return 0; // Success
}

i32 scene_array_add(scene_objects_t* array, scene_object_t object)
{
    if (!array) {
        return -1;
    }
    
    if (array->count >= array->capacity) 
    {
        size_t new_capacity = array->capacity * 2;
        if (scene_array_resize(array, new_capacity) != 0) {
            return -1; // Resize failed
        }
    }
    
    array->objects[array->count] = object;
    array->count++;
    
    return 0; 
}

i32 scene_array_remove(scene_objects_t* array, size_t index)
{
    if (!array || index >= array->count) {
        return -1; // Invalid parameters
    }
    
    // Shift elements to fill the gap
    for (size_t i = index; i < array->count - 1; i++) {
        array->objects[i] = array->objects[i + 1];
    }
    
    array->count--;
    
    if (array->count > 0 && array->count < array->capacity / 4) {
        scene_array_resize(array, array->capacity / 2);
    }
    
    return 0; // Success
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    gc.screen_width  = width;
    gc.screen_height = height;

    glViewport(0, 0, gc.screen_width, gc.screen_height);
}

void errorCallback(int error, const char* description) 
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void window_refresh_callback(GLFWwindow* window)
{
    glfwSwapBuffers(window);
}

void getMouseDelta(f32 *xoffset, f32 *yoffset)
{
    if (gc.firstMouse)
    {
        gc.mouseLastX = (f32)gc.mouseX;
        gc.mouseLastY = (f32)gc.mouseY;
        gc.firstMouse = false;
    }

    *xoffset = (f32)gc.mouseX - gc.mouseLastX;
    *yoffset = gc.mouseLastY - (f32)gc.mouseY; // reversed since y-coordinates range from bottom to top

    gc.mouseLastX = (f32)gc.mouseX;
    gc.mouseLastY = (f32)gc.mouseY;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;

    gc.mouseX = (f32)xpos;
    gc.mouseY = (f32)ypos;

    f32 xoffset, yoffset;
    getMouseDelta(&xoffset, &yoffset);

    if(gc.right_button_down)
    {
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
    (void)window;
    (void)xoffset;
    (void)yoffset;

    f32 scroll_sensitivity = 20.0f;
    f32 scaled_x = (f32)xoffset * scroll_sensitivity;
    f32 scaled_y = (f32)yoffset * scroll_sensitivity;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    (void)window;
    (void)mods;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        gc.left_button_down = (action == GLFW_PRESS);
        if(gc.left_button_down)
        {
            test_animation_move = true;
            new_x = gc.mouseX;
            new_y = gc.mouseY;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        gc.right_button_down = (action == GLFW_PRESS);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT) 
    {
        switch (key) {
            case GLFW_KEY_BACKSPACE:
                break;
            case GLFW_KEY_ENTER:
                break;
            case GLFW_KEY_TAB:
                break;
            case GLFW_KEY_F11:
                gc.profile ^= 1;
                break;
            case GLFW_KEY_F10:
                gc.capture = true;
                break;
            case GLFW_KEY_F7:
                gc.camera_mode ^= 1;
                if(gc.camera_mode)
                {
                    gc.samples_per_pixel = 1;
                    gc.max_depth = 2;
                }else
                {
                    gc.samples_per_pixel = 50;
                    gc.max_depth = 50;
                }
                break;
            case GLFW_KEY_W:
                if(gc.camera_mode)
                {
                    vec3f_t front = vec3f_scale(gc.camera.w, -1.0f);
                    gc.camera.pos = vec3f_add(gc.camera.pos,vec3f_scale(front, gc.camera.speed));
                    update_camera_view();
                }
                break;
            case GLFW_KEY_S:
                if(gc.camera_mode)
                {
                    vec3f_t front = vec3f_scale(gc.camera.w, -1.0f);
                    gc.camera.pos = vec3f_sub(gc.camera.pos,vec3f_scale(front, gc.camera.speed));
                    update_camera_view();
                }
                break;
            case GLFW_KEY_D:
                if(gc.camera_mode)
                {
                    vec3f_t right_scaled = vec3f_scale(gc.camera.u, gc.camera.speed);
                    gc.camera.pos = vec3f_add(gc.camera.pos, right_scaled);
                    update_camera_view();
                }
                break;
            case GLFW_KEY_A:
                if(gc.camera_mode)
                {
                    vec3f_t right_scaled = vec3f_scale(gc.camera.u, gc.camera.speed);
                    gc.camera.pos = vec3f_sub(gc.camera.pos, right_scaled);
                    update_camera_view();
                }
                break;
            case GLFW_KEY_I:
                if(gc.camera_mode)
                {
                    gc.camera.focus_dist++;
                    update_camera_view();
                }
                break;
            case GLFW_KEY_K:
                if(gc.camera_mode)
                {
                    gc.camera.focus_dist--;
                    if(gc.camera.focus_dist < 0.0f){
                        gc.camera.focus_dist = 0.0f;
                    }
                    update_camera_view();
                }
                break;
            case GLFW_KEY_UP:
                if(gc.camera_mode)
                {
                    vec3f_t up_scaled = vec3f_scale(gc.camera.up, gc.camera.speed);
                    gc.camera.pos = vec3f_add(gc.camera.pos, up_scaled);
                    update_camera_view();
                }
                break;
            case GLFW_KEY_DOWN:
                if(gc.camera_mode)
                {
                    vec3f_t up_scaled = vec3f_scale(gc.camera.up, gc.camera.speed);
                    gc.camera.pos = vec3f_sub(gc.camera.pos, up_scaled);
                    update_camera_view();
                }
                break;
            case GLFW_KEY_1:
                set_fov(15.0f);
                break;
            case GLFW_KEY_2:
                set_fov(45.0f);
                break;
            case GLFW_KEY_3:
                set_fov(60.0f);
                break;
            case GLFW_KEY_4:
                set_fov(100.0f);
                break;
            default:
                break;
        }
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) 
{
    if (codepoint >= 32 && codepoint <= 126) {

    }
}

void set_dark_mode(GLFWwindow *window)
{
    #if 0
    #ifdef _WIN32
        HWND hwnd = /**/0;

        BOOL value = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    #endif
    #endif
}

void prof_record_results(void)
{
    for (int i = 0; i < g_prof_storage.count; i++) 
    {
        if (g_prof_storage.entries[i].hit_count > 0) 
        {
            sprintf(prof_buf[i],"[PROFILE] %s[%llu]: %.6f ms (total)", 
                   g_prof_storage.entries[i].label,
                   (unsigned long long)g_prof_storage.entries[i].hit_count,
                   g_prof_storage.entries[i].elapsed_ms);

            int text_length = strlen(prof_buf[i]);
            if(text_length > prof_max_width){
                prof_max_width = text_length;
            }
            
            prof_buf_count++;
        }
    }
}

void render_prof_entries(void)
{
    draw_rect_solid_wh(&gc.draw_buffer, 0,0, 
                       prof_max_width*gc.font->font_char_width,
                       prof_buf_count*gc.font->font_char_height+10,
                       (color4_t){40, 42, 54,255});
    prof_max_width = 0;

    for(int i = 0; i < prof_buf_count; i++)
    {
        rendered_text_t text = {
            .font = gc.font,
            .pos = {.x=0,.y=i*gc.font->font_char_height},
            .color = {.r = 255, .g = 255, .b = 255, .a=255},
            .scale = 1,
            .string = prof_buf[i]
        };
        render_n_string_abs(&gc.draw_buffer, &text);
    }
}

/* 
    We can determine if the ray is inside or outside the sphere.
    by doing the dot product of the ray direction and the outward normal.
    If the dot product is positive, the ray is inside the sphere.
    If it is negative, the ray is outside the sphere. 
*/
void set_face_normal(hit_record_t *record, ray_t *ray, vec3f_t *outward_normal)
{
    record->front_face = vec3f_dot(ray->dir, *outward_normal) < 0;

    if(record->front_face)
    {
        // ray is outside the sphere
        record->norm = *outward_normal;
    }
    else
    {
        // ray is inside the sphere
        record->norm = vec3f_scale(*outward_normal, -1.0f);
    }
}

f32 reflectance(f32 cosine, f32 refraction_index)
{
    f32 r0 = (1-refraction_index)/(1+refraction_index);
    r0 = r0*r0;
    return r0 + (1-r0)*((1-cosine)*(1-cosine)*(1-cosine)*(1-cosine)*(1-cosine));
}

bool ray_scatter(ray_t *ray_in, hit_record_t *hit_info, vec3f_t *attenuation, ray_t *ray_scattered)
{
    switch (hit_info->mat.mat_type) 
    {
        case Lambertian:
            vec3f_t scatter_dir = vec3f_add(hit_info->norm, vec3f_random_direction());

            if(vec3f_is_near_zero(scatter_dir))
            {
                scatter_dir = hit_info->norm;
            }

            *ray_scattered = (ray_t){hit_info->hit_point, scatter_dir};
            *attenuation = hit_info->mat.albedo;

            return true;
            
        case Metal:
            vec3f_t reflected = vec3f_reflect(ray_in->dir, hit_info->norm);
            reflected = vec3f_add(vec3f_unit(reflected), vec3f_scale(vec3f_random_direction(), hit_info->mat.fuzz));
            *ray_scattered = (ray_t){hit_info->hit_point, reflected};
            *attenuation = hit_info->mat.albedo;

            return (vec3f_dot(ray_scattered->dir , hit_info->norm) > 0);

        case Dielectric:
            *attenuation = (vec3f_t){1.0f,1.0f,1.0f};
            f32 ri = hit_info->front_face ? (1.0f/hit_info->mat.refraction_index) : hit_info->mat.refraction_index;

            vec3f_t unit_direction = vec3f_unit(ray_in->dir);

            f32 cos_theta = fmin(vec3f_dot(vec3f_scale(unit_direction,-1.0f), hit_info->norm), 1.0f);
            f32 sin_theta = sqrt_f32(1.0f-cos_theta*cos_theta);

            bool cannot_refract = ri * sin_theta > 1.0;
            vec3f_t direction;

            if(cannot_refract || reflectance(cos_theta,ri) > RAND_FLOAT())
            {

                direction = vec3f_reflect(unit_direction, hit_info->norm);
            }
            else
            {
                direction = vec3f_refract(unit_direction, hit_info->norm, ri);
            }

            *ray_scattered = (ray_t){hit_info->hit_point, direction};

            return true;

        case Emissive:
        default:
            break;
    }

    return false;
}

/*
    Check if the ray hits a sphere
*/
bool hit_sphere(sphere_t *sphere, ray_t *ray, f32 ray_tmin, f32 ray_tmax, hit_record_t *hit_info)
{
    // vector from ray origin to the sphere
    vec3f_t oc = vec3f_sub(sphere->center, ray->orig);

    /*
        A ray: P(t) = origin + t * direction
        A sphere: (P - center)² = radius²

        Solution of the quadratic equation of the sphere

         -b +- discriminant
         ------------------  where discriminant = sqrt(b^2 - 4ac)
                2*a

        gives us two solutions (where ray enters and exits the sphere)

        t = (-b ± √(b² - 4ac)) / 2a
        
        With (h = -b/2):
        t = (h ± √(h² - ac)) / a

    */
    f32 a = vec3f_length_sq(ray->dir);
    f32 h = vec3f_dot(ray->dir, oc);
    f32 c = vec3f_length_sq(oc) - sphere->radius * sphere->radius;

    // is there real solutions ?
    f32 discriminant = h*h - a*c;
    if(discriminant < 0){
        // no intersection
        return false;
    }

    f32 disc_sqrt = sqrt_f32(discriminant);
    f32 root = (h - disc_sqrt) / (a); // distance to the CLOSER hit point (entry point)

    if(!Surrounds(root, ray_tmin, ray_tmax))
    {
        root = (h + disc_sqrt) / (a);  // check other root (exit point)

        // if both intersection outside the range just exit
        if(!Surrounds(root, ray_tmin, ray_tmax))
        {
            return false;
        }
    }

    hit_info->hit_dist = root;   // Distance along ray to hit point
    hit_info->hit_point = RAY_AT(ray, hit_info->hit_dist); // 3D pos of the hit point.

    // calculate the surface normal at the hit point
    // where normal = (hit_point - sphere_center)
    // outward_normal have unit length so divide the sphere radius.
    vec3f_t surface_notmal = vec3f_scale(vec3f_sub(hit_info->hit_point, sphere->center), 1.0f/(f32)sphere->radius);

    set_face_normal(hit_info, ray, &surface_notmal);

    hit_info->mat = sphere->mat;

    return true;
}

bool hit(scene_objects_t *arr, ray_t *ray, f32 ray_tmin, f32 ray_tmax, hit_record_t *hit_info)
{
    hit_record_t temp_rec;
    bool hit_anything = false;
    f32 closest = ray_tmax;

    for(size_t i = 0; i < arr->count; i++)
    {
        switch (arr->objects[i].type) 
        {
            case Sphere:
                if(hit_sphere((sphere_t *)arr->objects[i].object, ray, ray_tmin , closest, &temp_rec))
                {
                    hit_anything = true;
                    closest = temp_rec.hit_dist;
                    *hit_info = temp_rec;
                }
            break;

            default :
            break;
        }
    }

    return hit_anything;
}

vec3f_t random_on_hemisphere(vec3f_t *normal)
{
    vec3f_t on_unit_sphere = vec3f_random_direction();
    if(vec3f_dot(on_unit_sphere, *normal) > 0.0){
        return on_unit_sphere;
    }else{
        return vec3f_scale(on_unit_sphere, -1.0f);
    }
}

vec3f_t ray_color(ray_t ray, int depth)
{
    vec3f_t color = {1.0f, 1.0f, 1.0f};
    ray_t current_ray = ray;

    for(int i = 0; i < depth; i++)
    {
        hit_record_t rec;
    
        if(hit(gc.scene_objects, &current_ray, 0.001f, max_f32, &rec))
        {
            ray_t scattered;
            vec3f_t attenuation;
            if(ray_scatter(&current_ray, &rec, &attenuation, &scattered))
            {
                color = vec3f_mul(color, attenuation);
            }
            current_ray = scattered;
        }
        else
        {
            vec3f_t unit_dir = vec3f_unit(current_ray.dir);
        
            // gradient among the y-axis
            f32 blend_factor = 0.5 * (unit_dir.y + 1.0);
        
            vec3f_t col_norm = vec3f_lerp(
                (vec3f_t){1.0,1.0,1.0}, // white 
                (vec3f_t){0.5,0.7,1.0}, // blue
                blend_factor
            );

            return vec3f_mul(color, col_norm);
        }
    
    }
    return (vec3f_t){0.0f, 0.0f, 0.0f};
}

vec3f_t sample_square()
{
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
    return (vec3f_t){RAND_FLOAT() - 0.5, RAND_FLOAT() - 0.5, 0};
}

vec3f_t defocus_disk_sample()
{
    vec3f_t p = vec3f_random_direction_2d();
    return vec3f_add(gc.camera.pos, vec3f_add(vec3f_scale(gc.camera.defocus_disk_u, p.x), vec3f_scale(gc.camera.defocus_disk_v, p.y)));
}

ray_t get_ray(int x, int y)
{
    vec3f_t offset = sample_square();

    // current pixel (x,y) converted to normalized coordinated wrt to vp
/*
    vec3f_t pixel_center = {
        .x = gc.pixel00_loc.x + (x * gc.pixel_delta_u.x) + (y * gc.pixel_delta_v.x),
        .y = gc.pixel00_loc.y + (x * gc.pixel_delta_u.y) + (y * gc.pixel_delta_v.y),
        .z = gc.pixel00_loc.z + (x * gc.pixel_delta_u.z) + (y * gc.pixel_delta_v.z)
    };
*/

    // randomly sampled point around the pixel location i, j.
    vec3f_t pixel_sample = {
        .x = gc.pixel00_loc.x + ((x+offset.x) * gc.pixel_delta_u.x) + ((y+offset.y) * gc.pixel_delta_v.x),
        .y = gc.pixel00_loc.y + ((x+offset.x) * gc.pixel_delta_u.y) + ((y+offset.y) * gc.pixel_delta_v.y),
        .z = gc.pixel00_loc.z + ((x+offset.x) * gc.pixel_delta_u.z) + ((y+offset.y) * gc.pixel_delta_v.z)
    };

    vec3f_t ray_origin = (gc.camera.defocus_angle <= 0 ) ? gc.camera.pos : defocus_disk_sample();
    // The vector from the camera center to the pixel center.
    vec3f_t ray_dir = vec3f_sub(pixel_sample, ray_origin);

    ray_t ray = {ray_origin, ray_dir};

    return ray;
}

void increase_fov() 
{
    gc.camera.vfov += 5.0f;
    if (gc.camera.vfov > 120.0f) {
        gc.camera.vfov = 120.0f; 
    }
    update_camera_view();
}

void decrease_fov() 
{
    gc.camera.vfov -= 5.0f;
    if (gc.camera.vfov < 10.0f) {
        gc.camera.vfov = 10.0f; 
    }
    update_camera_view();
}

void set_fov(f32 new_fov)
{
    gc.camera.vfov = new_fov;
    if (gc.camera.vfov < 10.0f) gc.camera.vfov = 10.0f;
    if (gc.camera.vfov > 120.0f) gc.camera.vfov = 120.0f;
    update_camera_view();
}

void adjust_fov(f32 delta)
{
    gc.camera.vfov += delta;
    if (gc.camera.vfov < 10.0f) gc.camera.vfov = 10.0f;
    if (gc.camera.vfov > 120.0f) gc.camera.vfov = 120.0f;
    update_camera_view();
}

void update_camera_view() 
{
    /* 
        The viewport is a virtual rectangle in 3D space that represents our "screen"
        All rays will be cast from camera through points on this viewport

        The viewport height of 2.0 means the virtual screen is 2 world units tall
        Viewport goes from -1 to +1 in both X and Y directions
        camera_t at origin (0,0,0) looks at a 2×2 square centered at (0,0,-1)

                                            (0,1)
                                 ┌────────────┬───────────┐                         
                                 │            │           │                         
                                 │            │           │                         
                                 │            │(0,0)      │                         
                          (-1,0) |────────────┼───────────| (1,0)                       
                                 │            │           │                         
                                 │            │           │                         
                                 │            │           │                         
                                 └────────────┴───────────┘                         
                                            (0,-1)
                                     
    */

    // distance from camera to the viewport
    // f32 focal_length = vec3f_length(vec3f_sub(gc.camera.pos, gc.camera.target));

    f32 theta = radians_from_degrees_f32(gc.camera.vfov);
    f32 h = tan_f32(theta/2.0f);

    f32 vp_height = 2.0f * h * gc.camera.focus_dist;
    f32 vp_width = vp_height * ((f32)gc.screen_width/(f32)gc.screen_height);

    gc.camera.w = vec3f_unit(vec3f_sub(gc.camera.pos, gc.camera.target));
    gc.camera.u = vec3f_unit(vec3f_cross(gc.camera.up, gc.camera.w));
    gc.camera.v = vec3f_cross(gc.camera.w, gc.camera.u);

    // define viewport orientation in world space
    vec3f_t vp_u = vec3f_scale(gc.camera.u, vp_width);
    vec3f_t vp_v = vec3f_scale(gc.camera.v, -vp_height);

    // how much to move in world space when moving one pixel
    // mapping between screen pixels and viewport coordinates
    gc.pixel_delta_u = vec3f_scale(vp_u, 1.0f/(f32)gc.screen_width);
    gc.pixel_delta_v = vec3f_scale(vp_v, 1.0f/(f32)gc.screen_height);

    // positioned relative to the camera center
    // shifted left by half the width 
    // and up by half the height
    // and back by focal length
    vec3f_t viewport_center = vec3f_sub(gc.camera.pos, vec3f_scale(gc.camera.w, gc.camera.focus_dist));
    vec3f_t vp_upper_left = vec3f_sub(viewport_center, 
                                      vec3f_add(vec3f_scale(vp_u, 0.5f), 
                                               vec3f_scale(vp_v, 0.5f)));


    // calculate the center of the first pixel normalized to the viewport
    gc.pixel00_loc = vec3f_add(vp_upper_left, 
                      vec3f_add(vec3f_scale(gc.pixel_delta_u, 0.5f), 
                               vec3f_scale(gc.pixel_delta_v, 0.5f)));
    
    f32 defocus_radius = gc.camera.focus_dist * tan_f32(radians_from_degrees_f32(gc.camera.defocus_angle / 2));
    gc.camera.defocus_disk_u = vec3f_scale(gc.camera.u, defocus_radius);
    gc.camera.defocus_disk_v = vec3f_scale(gc.camera.v, defocus_radius);

}

void init_camera(int window_width, f32 aspect_ratio)
{
    u32 window_height = (u32)(window_width / aspect_ratio);

    assert(window_height > 1);

    gc.screen_width  = window_width;
    gc.screen_height = window_height;

    gc.camera.pos = (vec3f_t){13.0f, 2.0f, 3.0f};
    gc.camera.target = (vec3f_t){0.0f, 0.0f, 0.0f};
    gc.camera.up = (vec3f_t){0.0f, 1.0f, 0.0f};
    gc.camera.vfov  = 60.0f;
    gc.camera.speed = 2.0f;

    gc.camera.defocus_angle = 0.6f;
    gc.camera.focus_dist = 10.0f;

    update_camera_view();

    gc.samples_per_pixel = 1;
    gc.max_depth = 2;
}

void poll_events(void)
{
    glfwPollEvents();
}

void render_to_screen(void *pixels, u32 width, u32 height)
{
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    (int)width, (int)height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // make the default framebuffer active again ?
    
    // transfer pixel values from one region of a read framebuffer to another region of a draw framebuffer.
    glBlitFramebuffer(0, 0, width, height,  // src
                      0, height, width, 0,  // dst (flipped)    
                      GL_COLOR_BUFFER_BIT,    
                      GL_NEAREST);      
}

typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_SINE,
    EASE_OUT_SINE,
    EASE_IN_OUT_SINE,
    EASE_OUT_BOUNCE
}easing_type;

typedef struct {
    u64         id;
    f32         start_x, start_y;
    f32         current_x, current_y;
    f32         target_x, target_y;
    f32         duration;
    f32         elapsed;
    easing_type easing;
    bool        done;
}animation_t;

#define ANIMATION_MAX_ITEMS 32
animation_t animation_items[ANIMATION_MAX_ITEMS];
int animation_item_count;

/* https://easings.net/ */
float apply_easing(float t, easing_type easing) 
{
    switch (easing) 
    {
        case EASE_LINEAR:
            return t;
            
        case EASE_IN_QUAD:
            return t * t;
            
        case EASE_OUT_QUAD:
            return t * (2 - t);
            
        case EASE_IN_OUT_QUAD:
            if (t < 0.5f) return 2 * t * t;
            return -1 + (4 - 2 * t) * t;
            
        case EASE_IN_CUBIC:
            return t * t * t;
            
        case EASE_OUT_CUBIC:
            return (--t) * t * t + 1;
            
        case EASE_IN_OUT_CUBIC:
            if (t < 0.5f) return 4 * t * t * t;
            return (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
            
        case EASE_IN_SINE:
            return 1 - cosf(t * M_PI_2);
            
        case EASE_OUT_SINE:
            return sinf(t * M_PI_2);
            
        case EASE_IN_OUT_SINE:
            return -0.5f * (cosf(M_PI * t) - 1);

        case EASE_OUT_BOUNCE:
            const float n1 = 7.5625f;
            const float d1 = 2.75f;

            if (t < 1.0f / d1) {
                return n1 * t * t;
            } else if (t < 2.0f / d1) {
                return n1 * (t -= 1.5f / d1) * t + 0.75f;
            } else if (t < 2.5f / d1) {
                return n1 * (t -= 2.25f / d1) * t + 0.9375f;
            } else {
                return n1 * (t -= 2.625f / d1) * t + 0.984375f;
            }
        default:
            return t;
    }
}

void animation_start(u64 id, float start_x, float start_y, 
                     float target_x, float target_y, 
                     float duration, easing_type easing) 
{
    for (int i = 0; i < animation_item_count; i++) 
    {
        animation_t *it = &animation_items[i];
        if (it->id == id) {
            it->elapsed = 0;
            return;
        }
    }

    // push new item if we have room
    if (animation_item_count < ANIMATION_MAX_ITEMS) 
    {
        animation_items[animation_item_count++] = (animation_t){
            .id = id,
            .start_x = start_x,
            .start_y = start_y,
            .target_x = target_x,
            .target_y = target_y,
            .easing = easing,
            .duration = duration,
            .elapsed = 0,
            .done = false
        };
    }
}

void animation_update(float dt) 
{
    for(int i = animation_item_count-1; i>=0; i--)
    {
        animation_t *anim = &animation_items[i];
        
        anim->elapsed += dt;
        
        if (anim->elapsed >= anim->duration) 
        {
            anim->done = true;
            anim->current_x = anim->target_x;
            anim->current_y = anim->target_y;
            // remove it from the list when done
            // just replace it with the last item and decrement the count
            *anim = animation_items[--animation_item_count];
            continue;
        }
        
        float t = anim->elapsed / anim->duration;
        float eased_t = apply_easing(t, anim->easing);
        
        anim->current_x = anim->start_x + (anim->target_x - anim->start_x) * eased_t;
        anim->current_y = anim->start_y + (anim->target_y - anim->start_y) * eased_t;
    }
}

vec2f_t animation_get(u64 id)
{
    for (int i = 0; i < animation_item_count; i++) 
    {
        animation_t *it = &animation_items[i];
        if (it->id == id) {
            return (vec2f_t){it->current_x,it->current_y};
        }
    }
    return (vec2f_t){0.0f,0.0f};
}

void render_all(void)
{
    gc.draw_buffer.height = gc.screen_height;
    gc.draw_buffer.width  = gc.screen_width;

    u32 height       = gc.draw_buffer.height;
    u32 width        = gc.draw_buffer.width;

    PROFILE("Rendering : Allocating Frame Memory")
    {
        gc.draw_buffer.pixels = ARENA_ALLOC(gc.frame_arena, height * width * sizeof(color4_t));
    }
    
    clear_screen(&gc.draw_buffer, HEX_TO_COLOR4(0x282a36));
    
    #if 0    
    PROFILE("Rendering : Ray Tracing")
    {
        vec3f_t color;

        for (u32 y = 0; y < height; ++y)
        {
            poll_events();
            for(u32 x = 0; x < width; ++x)
            {
                color = (vec3f_t){0, 0, 0};
                PROFILE("Ray Tracing: Getting Color")
                {
                    for(int sample = 0; sample < gc.samples_per_pixel; sample++)
                    {
                        ray_t ray = get_ray(x, y);
                        color = vec3f_add(color, ray_color(ray, gc.max_depth));
                    }
                    color = vec3f_scale(color, (f32)1.0f/(f32)gc.samples_per_pixel);
                    color = linear_to_gamma(color);
                }
                PROFILE("Just putting a pixel")
                {
                    set_pixel(&gc.draw_buffer, x, y, to_color4(color));
                }
            }
            if(y%8)present_frame(&gc.draw_buffer);
        }
    }
    #endif
    
    #if 1
    PROFILE("Rendering : Scissors test")
    {
        draw_rect_outline_wh(&gc.draw_buffer, 50, 50, 200, 300, COLOR_MAGENTA);

        push_scissor(50, 50, 200, 300);
            draw_rect_scissored(&gc.draw_buffer,60, 60, 180, 500, COLOR_RED);   
            draw_rect_scissored(&gc.draw_buffer,80, 200, 60, 40, COLOR_GREEN);
            draw_rect_scissored(&gc.draw_buffer, gc.mouseX, gc.mouseY, 50, 30, COLOR_BLUE);
        pop_scissor();

        if(test_animation_move)
        {
            animation_start((u64)&test_animation.x, 
                                test_animation.x, test_animation.y,
                                new_x, new_y, 1.0f,
                                EASE_OUT_BOUNCE);
            test_animation_move = false;
        }

        animation_update(gc.dt);

        vec2f_t res = animation_get((u64)&test_animation.x);
        if(res.x != 0.0f && res.y != 0.0f){
            test_animation.x = res.x;
            test_animation.y = res.y;
        }

        draw_rect_scissored(&gc.draw_buffer, test_animation.x, test_animation.y,
                            test_animation.w, test_animation.h, COLOR_YELLOW);   
    }
    #endif

    PROFILE("Rendering : Text")
    {
        #if 1
            u32 scale = 2;
            u32 pos = gc.screen_width-20*gc.font->font_char_width*scale;

            rendered_text_t text = {
                .font = gc.font,
                .pos = {.x=pos, .y=0},
                .color = {.r = 255, .g = 255, .b = 255, .a=255},
                .scale = scale,
                .string = frametime
            };
            
            sprintf(frametime,"%.2f ms", gc.dt*1000);
            render_n_string_abs(&gc.draw_buffer, &text);
        #endif

        if(gc.profile)
        {
            render_prof_entries();
        }
    }
    prof_buf_count = 0;
    
    if(gc.capture)
    {
        export_image(&gc.draw_buffer, "capture.tga");
        gc.capture = false;
    }
}

void present_frame(image_view_t* view) 
{
    render_to_screen(view->pixels, view->width, view->height);
    glfwSwapBuffers((GLFWwindow*)gc.window);
}

void init_framebuffer(void)
{
    glGenTextures(1, &gc.texture);
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    // Generate a full screen texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 (int)1920, (int)1080,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &gc.read_fbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);

    // attach the texture to the framebuffer
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gc.texture, 0);
}

sphere_t ground_sphere;
sphere_t large_sphere_1;  // glass sphere at center
sphere_t large_sphere_2;  // brown lambertian sphere
sphere_t large_sphere_3;  // metal sphere
sphere_t small_spheres[484]; // 22x22 grid, but some will be skipped

void init_scene(void)
{
    gc.scene_objects = scene_array_create(0);
    
    ground_sphere = (sphere_t){
        .mat = {
            .mat_type = Lambertian,
            .albedo = {0.5f, 0.5f, 0.5f}
        },
        .center = (vec3f_t){0.0f, -1000.0f, 0.0f},
        .radius = 1000.0f
    };
    scene_array_add(gc.scene_objects, (scene_object_t){.type=Sphere, .object=&ground_sphere});
    
    #if 0
        // Generate small random spheres
        int sphere_count = 0;
        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                f32 choose_mat = RAND_FLOAT();
                vec3f_t center = (vec3f_t){
                    a + 0.9f * RAND_FLOAT(), 
                    0.2f, 
                    b + 0.9f * RAND_FLOAT()
                };
                
                // Check distance from point (4, 0.2, 0)
                vec3f_t reference_point = (vec3f_t){4.0f, 0.2f, 0.0f};
                vec3f_t diff = vec3f_sub(center, reference_point);
                if (vec3f_length(diff) > 0.9f) {
                    if (choose_mat < 0.8f) {
                        // Diffuse material
                        vec3f_t albedo = vec3f_mul(vec3f_random(), vec3f_random());
                        small_spheres[sphere_count] = (sphere_t){
                            .mat = {
                                .mat_type = Lambertian,
                                .albedo = albedo
                            },
                            .center = center,
                            .radius = 0.2f
                        };
                    } else if (choose_mat < 0.95f) {
                        // Metal material
                        vec3f_t albedo = vec3f_random_range(0.5f, 1.0f);
                        f32 fuzz = RAND_FLOAT_RANGE(0.0f, 0.5f);
                        small_spheres[sphere_count] = (sphere_t){
                            .mat = {
                                .mat_type = Metal,
                                .albedo = albedo,
                                .fuzz = fuzz
                            },
                            .center = center,
                            .radius = 0.2f
                        };
                    } else {
                        // Glass material
                        small_spheres[sphere_count] = (sphere_t){
                            .mat = {
                                .mat_type = Dielectric,
                                .refraction_index = 1.5f
                            },
                            .center = center,
                            .radius = 0.2f
                        };
                    }
                    
                    scene_array_add(gc.scene_objects, (scene_object_t){
                        .type = Sphere, 
                        .object = &small_spheres[sphere_count]
                    });
                    sphere_count++;
                }
            }
        }
    #endif

    large_sphere_1 = (sphere_t){
        .mat = {
            .mat_type = Dielectric,
            .refraction_index = 1.5f
        },
        .center = (vec3f_t){0.0f, 1.0f, 0.0f},
        .radius = 1.0f
    };
    scene_array_add(gc.scene_objects, (scene_object_t){.type=Sphere, .object=&large_sphere_1});
    
    large_sphere_2 = (sphere_t){
        .mat = {
            .mat_type = Lambertian,
            .albedo = {0.4f, 0.2f, 0.1f}
        },
        .center = (vec3f_t){-4.0f, 1.0f, 0.0f},
        .radius = 1.0f
    };
    scene_array_add(gc.scene_objects, (scene_object_t){.type=Sphere, .object=&large_sphere_2});
    
    large_sphere_3 = (sphere_t){
        .mat = {
            .mat_type = Metal,
            .albedo = {0.7f, 0.6f, 0.5f},
            .fuzz = 0.0f
        },
        .center = (vec3f_t){4.0f, 1.0f, 0.0f},
        .radius = 1.0f
    };
    scene_array_add(gc.scene_objects, (scene_object_t){.type=Sphere, .object=&large_sphere_3});
}

void *create_window(u32 width, u32 height, char *title)
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 8); // Enable multisampling for smoother edges

    
    GLFWwindow *window = CHECK_PTR(glfwCreateWindow((int)gc.screen_width, (int)gc.screen_height,
                                                    title, NULL, NULL));

    glfwMakeContextCurrent(window);

    glViewport(0, 0, (int)gc.screen_width, (int)gc.screen_height);
    
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetErrorCallback(errorCallback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization error: %s\n", glewGetErrorString(err));
        return NULL;
    }

    glEnable(GL_BLEND);

    glfwSwapInterval(1);
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    return window;
}

bool init_all(void)
{
    f32 aspect_ratio = 16.0f / 9.0f;
    u32 window_width = 1000;

    init_camera(window_width, aspect_ratio);
    init_scene();

    gc.window = create_window(gc.screen_width, gc.screen_height, "Ray");

    // Preallocate it 
    gc.frame_arena = arena_new();
    (void)ARENA_ALLOC(gc.frame_arena, 1024*1024*10);
    arena_reset(gc.frame_arena);

    init_framebuffer();

    gc.font = init_font((u32*)font_pixels);
    
    prof_init();

    gc.running     = true;
    gc.resize      = true;
    gc.rescale     = true;
    gc.render      = true;
    gc.dock        = false;
    gc.profile     = false;
    gc.changed     = true;
    gc.camera_mode = true;

    gc.global_scale = 1;

    gc.render_interval  = 20;
    gc.last_render_time = 0;

    get_time(&gc.last_frame_start);

    set_dark_mode(gc.window);

    return true;
}

int main(void)
{   
    init_all();

    while(!glfwWindowShouldClose((GLFWwindow*)gc.window))
    {
        poll_events();

        gc.dt = get_time_difference(&gc.last_frame_start);
        
        PROFILE("Rendering all")
        {
            render_all();
        }

        present_frame(&gc.draw_buffer);

        PROFILE("Reseting the arena")
        {
            arena_reset(gc.frame_arena);
        }

        prof_sort_results();
        prof_record_results();
        prof_reset();
    }
    return 0;
}
