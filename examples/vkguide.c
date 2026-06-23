#include <stdbool.h>
#include "kuta/kuta_types.h"
#include "kuta/kuta.h"
#include "kuta/kuta_platform.h"
#include "kuta/kuta_renderer.h"
#include "cglm/cglm.h"
#include <string.h>
#include <vulkan/vulkan.h>

int main(void) {
    KtPlatform platform = kt_sdl3_platform(&(KtPlatformDesc){
        .window_title  = "Hello Kuta",
        .window_width  = 800,
        .window_height = 600,
    });

    KtRenderer renderer = kt_vulkan_renderer(&(KtRendererDesc){
        .platform    = &platform,
        .vsync       = true,
        .render_scale = 1.0f,
        .app_name    = "Hello Kuta",
        .api_version = VK_API_VERSION_1_4,
    });

    uint32_t sword = renderer.load_mesh(&renderer, "assets/basicmesh.glb");

    // identity matrix — mat4 is float[16]
    KtMat4 identity = {.m = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    }};


    mat4 view_m, proj_m;
    glm_translate_make(view_m, (vec3){0.0f, 0.0f, -5.0f});

    glm_perspective(glm_rad(70.0f), 800.0f / 600.0f, 0.1f, 10000.0f, proj_m);
    proj_m[1][1] *= -1;

    KtMat4 view, proj;
    memcpy(view.m, view_m, sizeof(float) * 16);
    memcpy(proj.m, proj_m, sizeof(float) * 16);

    while (platform.is_running(&platform)) {
        platform.poll(&platform);
        if (platform.key_pressed(&platform, KT_KEY_ESCAPE))
            platform.quit(&platform);

        if (renderer.begin_frame(&renderer)) {
            renderer.set_camera(&renderer, view, proj);
            renderer.submit(&renderer, sword, 2, 0, identity);
            renderer.end_frame(&renderer);
        }
    }

    renderer.destroy_mesh(&renderer, sword);
    renderer.destroy(&renderer);
    platform.destroy(&platform);
}
