// Bootstrap (first time only):
//   cc nob.c -o nob && ./nob
// Every run after:
//   ./nob

#define NOB_IMPLEMENTATION
#include "nob.h"
#include <sys/stat.h>
#include <unistd.h>

// ─── paths ────────────────────────────────────────────────────────────────────

#define ENGINE_SRC  "engine/src"
#define OBJ_DIR     "build/obj"
#define BIN_DIR     "bin"
#define TARGET      BIN_DIR "/engine"

#define EXT         "external"
#define CGLM_INC    EXT "/cglm/include"
#define CGLTF_INC   EXT "/cgltf"
#define MICROUI_INC EXT "/microui/src"
#define SDL_INC     EXT "/SDL-release-3.4.10/include"
#define STB_INC     EXT
#define VMA_INC     EXT

// ─── mkdir -p ─────────────────────────────────────────────────────────────────

static bool mkdir_p(const char *path)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    struct stat st;
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (stat(tmp, &st) != 0)
            if (!nob_mkdir_if_not_exists(tmp)) return false;
        *p = '/';
    }
    if (stat(tmp, &st) != 0)
        if (!nob_mkdir_if_not_exists(tmp)) return false;
    return true;
}

// ─── file collector ───────────────────────────────────────────────────────────

static bool collect_c_files(const char *dir, Nob_File_Paths *out)
{
    Nob_File_Paths entries = {0};
    if (!nob_read_entire_dir(dir, &entries)) return false;

    for (size_t i = 0; i < entries.count; i++) {
        const char *name = entries.items[i];
        if (name[0] == '.') continue;

        const char *full = nob_temp_sprintf("%s/%s", dir, name);

        if (nob_get_file_type(full) == NOB_FILE_DIRECTORY) {
            if (!collect_c_files(full, out)) {
                nob_da_free(entries);
                return false;
            }
            continue;
        }

        Nob_String_View sv = nob_sv_from_cstr(name);
        if (!nob_sv_end_with(sv, ".c")) continue;
        nob_da_append(out, nob_temp_strdup(full));
    }

    nob_da_free(entries);
    return true;
}

static const char *src_to_obj(const char *src)
{
    return nob_temp_sprintf(OBJ_DIR "/%s.o", src);
}

// ─── compile ──────────────────────────────────────────────────────────────────

static bool compile_c(Nob_Cmd *cmd, const char *src, const char *obj)
{
    if (!nob_needs_rebuild1(obj, src)) return true;
    if (!mkdir_p(nob_temp_dir_name(obj))) return false;

    nob_log(NOB_INFO, "CC  %s", src);

    // reset then build the command in one shot
    cmd->count = 0;
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, "-std=c11", "-g", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter");
    nob_cmd_append(cmd, "-I"ENGINE_SRC, "-I"CGLM_INC, "-I"CGLTF_INC, "-I"MICROUI_INC);
    nob_cmd_append(cmd, "-I"SDL_INC, "-I"STB_INC, "-I"VMA_INC);
    nob_cmd_append(cmd, "-c", src, "-o", obj);
    return nob_cmd_run_sync(*cmd);
}

// ─── shaders ──────────────────────────────────────────────────────────────────

static bool compile_shaders(Nob_Cmd *cmd)
{
    if (!nob_file_exists("shaders")) return true;

    Nob_File_Paths entries = {0};
    if (!nob_read_entire_dir("shaders", &entries)) return false;

    bool ok = true;
    for (size_t i = 0; i < entries.count && ok; i++) {
        const char *name = entries.items[i];
        Nob_String_View sv = nob_sv_from_cstr(name);
        bool is_shader =
            nob_sv_end_with(sv, ".vert") ||
            nob_sv_end_with(sv, ".frag") ||
            nob_sv_end_with(sv, ".comp");
        if (!is_shader) continue;

        const char *src = nob_temp_sprintf("shaders/%s",     name);
        const char *spv = nob_temp_sprintf("shaders/%s.spv", name);
        if (!nob_needs_rebuild1(spv, src)) continue;

        nob_log(NOB_INFO, "GLSLC %s", src);
        cmd->count = 0;
        nob_cmd_append(cmd, "glslc", src, "-o", spv);
        if (!nob_cmd_run_sync(*cmd)) ok = false;
    }

    nob_da_free(entries);
    return ok;
}

// ─── link ─────────────────────────────────────────────────────────────────────

static bool link_engine(Nob_Cmd *cmd, Nob_File_Paths *srcs)
{
    // collect obj paths and check if relinking is needed
    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < srcs->count; i++)
        nob_da_append(&objs, src_to_obj(srcs->items[i]));

    int needed = nob_needs_rebuild(TARGET, objs.items, objs.count);
    if (needed < 0)  { nob_da_free(objs); return false; }
    if (needed == 0) {
        nob_log(NOB_INFO, "%s is up to date", TARGET);
        nob_da_free(objs);
        return true;
    }

    nob_log(NOB_INFO, "LD  %s", TARGET);
    cmd->count = 0;
    nob_cmd_append(cmd, "cc");
    nob_da_append_many(cmd, objs.items, objs.count);
    nob_cmd_append(cmd, "-lSDL3", "-lvulkan", "-lm", "-o", TARGET);

    nob_da_free(objs);
    return nob_cmd_run_sync(*cmd);
}

// ─── compile_commands.json ────────────────────────────────────────────────────

static bool write_compile_commands(Nob_File_Paths *srcs)
{
    FILE *f = fopen("compile_commands.json", "w");
    if (!f) {
        nob_log(NOB_ERROR, "could not open compile_commands.json");
        return false;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        nob_log(NOB_ERROR, "getcwd failed");
        fclose(f);
        return false;
    }

    const char *flags =
        "cc -std=c11 -g -O2 -Wall -Wextra -Wno-unused-parameter"
        " -I"ENGINE_SRC " -I"CGLM_INC " -I"CGLTF_INC
        " -I"MICROUI_INC " -I"SDL_INC " -I"STB_INC " -I"VMA_INC;

    fprintf(f, "[\n");
    for (size_t i = 0; i < srcs->count; i++) {
        const char *src   = srcs->items[i];
        const char *comma = (i + 1 < srcs->count) ? "," : "";
        fprintf(f,
            "  {\n"
            "    \"directory\": \"%s\",\n"
            "    \"command\":   \"%s -c %s -o %s\",\n"
            "    \"file\":      \"%s\"\n"
            "  }%s\n",
            cwd, flags, src, src_to_obj(src), src, comma);
    }
    fprintf(f, "]\n");
    fclose(f);
    nob_log(NOB_INFO, "wrote compile_commands.json (%zu entries)", srcs->count);
    return true;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BIN_DIR)) return 1;
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_DIR)) return 1;

    Nob_Cmd cmd = {0};

    nob_log(NOB_INFO, "=== shaders ===");
    if (!compile_shaders(&cmd)) return 1;

    nob_log(NOB_INFO, "=== engine ===");
    Nob_File_Paths srcs = {0};
    if (!collect_c_files(ENGINE_SRC, &srcs)) return 1;

    if (srcs.count == 0) {
        nob_log(NOB_WARNING, "no .c files found under " ENGINE_SRC);
        return 0;
    }

    for (size_t i = 0; i < srcs.count; i++)
        if (!compile_c(&cmd, srcs.items[i], src_to_obj(srcs.items[i]))) return 1;

    if (!write_compile_commands(&srcs)) return 1;
    if (!link_engine(&cmd, &srcs))     return 1;

    nob_log(NOB_INFO, "done -> " TARGET);
    nob_da_free(srcs);
    return 0;
}
