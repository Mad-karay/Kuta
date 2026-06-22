#include <stdbool.h>

typedef struct KtPlatform KtPlatform;

struct KtPlatform {
    void   (*poll)       (KtPlatform *self);
    void   (*quit)       (KtPlatform *self);
    bool   (*is_running) (KtPlatform *self);
    float  (*delta_time) (KtPlatform *self);
    float  (*total_time) (KtPlatform *self);
    void  *(*native_window)(KtPlatform *self); 
    void   (*destroy)    (KtPlatform *self);

    void *ctx;
};
