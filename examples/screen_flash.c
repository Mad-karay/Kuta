#include <vulkan/vulkan_core.h>
#include "util_mod/log.h"
#include "kuta/kuta.h"

int main(){
    KutaCtx *ctx = kuta_create();

    if(init_engine(ctx, "Kuta", "Kudo", "Hello, Kuta", 800, 600, VK_API_VERSION_1_4)) {
      LOG_E("Init engine failed");
      return 1;
    }

    main_loop(ctx);

    deinit_engine(ctx);

    return 0;
}
