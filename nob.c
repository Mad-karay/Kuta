#define NOB_IMPLEMENTATION
#include "nob.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>


#define ENGINE_SRC   "engine/src"
#define ENGINE_INC   "engine/include"
#define EXAMPLES_DIR "examples"
#define OBJ_DIR      "build/obj"
#define BIN_DIR      "build/bin"

#define ENGINE_LIB   BIN_DIR "/libkuta.so"
#define EDITOR_OUT   BIN_DIR "/kuta_editor"

#define EXT          "external"
#define CGLM_INC     EXT "/cglm/include"
#define CGLTF_INC    EXT "/cgltf"
#define MICROUI_INC  EXT "/microui/src"
#define SDL_INC      EXT "/SDL-release-3.4.10/include"
#define STB_INC      EXT
#define VMA_INC      EXT

#define VMA_IMPL_SRC ENGINE_SRC "/vma_impl.cpp"
#define VMA_IMPL_OBJ OBJ_DIR "/vma_impl.cpp.o"


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

static bool compile_c(Nob_Cmd *cmd, const char *src, const char *obj)
{
    if (!nob_needs_rebuild1(obj, src)) return true;
    if (!mkdir_p(nob_temp_dir_name(obj))) return false;

    nob_log(NOB_INFO, "CC   %s", src);

    cmd->count = 0;
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, "-std=c11", "-g", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter");
    nob_cmd_append(cmd, "-fPIC");
    nob_cmd_append(cmd, "-I"ENGINE_SRC, "-I"ENGINE_INC, "-I"CGLM_INC, "-I"CGLTF_INC, "-I"MICROUI_INC);
    nob_cmd_append(cmd, "-I"SDL_INC, "-I"STB_INC, "-I"VMA_INC);
    nob_cmd_append(cmd, "-c", src, "-o", obj);
    return nob_cmd_run_sync(*cmd);
}

static bool compile_cpp(Nob_Cmd *cmd, const char *src, const char *obj)
{
    if (!nob_needs_rebuild1(obj, src)) return true;
    if (!mkdir_p(nob_temp_dir_name(obj))) return false;

    nob_log(NOB_INFO, "CXX  %s", src);

    cmd->count = 0;
    nob_cmd_append(cmd, "c++");
    nob_cmd_append(cmd, "-std=c++17", "-g", "-O2");
    nob_cmd_append(cmd, "-fPIC");
    nob_cmd_append(cmd, "-I"ENGINE_SRC, "-I"ENGINE_INC, "-I"VMA_INC);
    nob_cmd_append(cmd, "-c", src, "-o", obj);
    return nob_cmd_run_sync(*cmd);
}

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

static bool build_engine_library(Nob_Cmd *cmd, Nob_File_Paths *library_srcs)
{
    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < library_srcs->count; i++) {
        nob_da_append(&objs, src_to_obj(library_srcs->items[i]));
    }
    nob_da_append(&objs, VMA_IMPL_OBJ);

    int needed = nob_needs_rebuild(ENGINE_LIB, objs.items, objs.count);
    if (needed < 0)  { nob_da_free(objs); return false; }
    if (needed == 0) { nob_da_free(objs); return true; }

    nob_log(NOB_INFO, "LD   %s", ENGINE_LIB);
    cmd->count = 0;
    nob_cmd_append(cmd, "cc", "-shared", "-fPIC", "-o", ENGINE_LIB);
    nob_da_append_many(cmd, objs.items, objs.count);
    // libkuta.so carries all its own deps — users only link -lkuta
    nob_cmd_append(cmd, "-lSDL3", "-lvulkan", "-lm", "-lstdc++");

    bool success = nob_cmd_run_sync(*cmd);
    nob_da_free(objs);
    return success;
}

static bool build_and_link_executable(Nob_Cmd *cmd, const char *src_path, const char *out_binary_path)
{
    const char *obj_path = src_to_obj(src_path);
    if (!compile_c(cmd, src_path, obj_path)) return false;

    const char *deps[] = { obj_path, ENGINE_LIB };
    int needs_lnk = nob_needs_rebuild(out_binary_path, deps, 2);
    if (needs_lnk < 0) return false;
    if (needs_lnk == 0) {
        nob_log(NOB_INFO, "%s is up to date", out_binary_path);
        return true;
    }

    nob_log(NOB_INFO, "LD   %s", out_binary_path);
    cmd->count = 0;
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, obj_path);
    nob_cmd_append(cmd, "-L"BIN_DIR, "-lkuta");
    nob_cmd_append(cmd, "-Wl,-rpath," BIN_DIR);
    nob_cmd_append(cmd, "-o", out_binary_path);
    return nob_cmd_run_sync(*cmd);
}

static bool write_compile_commands(Nob_File_Paths *srcs)
{
    FILE *f = fopen("compile_commands.json", "w");
    if (!f) return false;

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) { fclose(f); return false; }

    const char *flags =
        "cc -std=c11 -g -O2 -Wall -Wextra -Wno-unused-parameter -fPIC"
        " -I"ENGINE_SRC
        " -I"ENGINE_INC
        " -I"CGLM_INC
        " -I"CGLTF_INC
        " -I"MICROUI_INC
        " -I"SDL_INC
        " -I"STB_INC
        " -I"VMA_INC;

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
    return true;
}


int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *selected_example = (argc > 1) ? argv[1] : NULL;

    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!nob_mkdir_if_not_exists(BIN_DIR)) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_DIR)) return 1;

    Nob_Cmd cmd = {0};

    nob_log(NOB_INFO, "=== Stage 1: Shaders ===");
    if (!compile_shaders(&cmd)) return 1;

    nob_log(NOB_INFO, "=== Stage 2: Engine Library ===");
    Nob_File_Paths engine_srcs = {0};
    if (!collect_c_files(ENGINE_SRC, &engine_srcs)) return 1;

    for (size_t i = 0; i < engine_srcs.count; i++) {
        if (!compile_c(&cmd, engine_srcs.items[i], src_to_obj(engine_srcs.items[i]))) return 1;
    }
    if (!compile_cpp(&cmd, VMA_IMPL_SRC, VMA_IMPL_OBJ)) return 1;
    if (!build_engine_library(&cmd, &engine_srcs)) return 1;

    nob_log(NOB_INFO, "=== Stage 3: Examples ===");

    Nob_File_Paths example_srcs = {0};
    if (!collect_c_files(EXAMPLES_DIR, &example_srcs)) return 1;

    if (selected_example) {
        const char *src_path = nob_temp_sprintf("%s/%s.c", EXAMPLES_DIR, selected_example);
        const char *bin_path = nob_temp_sprintf("%s/%s",   BIN_DIR,      selected_example);

        if (!nob_file_exists(src_path)) {
            nob_log(NOB_ERROR, "Example '%s' not found at %s", selected_example, src_path);
            nob_da_free(engine_srcs);
            nob_da_free(example_srcs);
            return 1;
        }

        if (!build_and_link_executable(&cmd, src_path, bin_path)) return 1;

        nob_log(NOB_INFO, "=== Run: %s ===", bin_path);
        cmd.count = 0;
        nob_cmd_append(&cmd, bin_path);
        if (!nob_cmd_run_sync(cmd)) return 1;
    }

    const char *editor_main_src = "editor/src/main.c";
    if (nob_file_exists(editor_main_src)) {
        nob_log(NOB_INFO, "=== Stage 4: Editor ===");
        if (!build_and_link_executable(&cmd, editor_main_src, EDITOR_OUT)) return 1;
    }

    Nob_File_Paths all_srcs = {0};
    nob_da_append_many(&all_srcs, engine_srcs.items,  engine_srcs.count);
    nob_da_append_many(&all_srcs, example_srcs.items, example_srcs.count);

    if (!write_compile_commands(&all_srcs)) return 1;
    nob_log(NOB_INFO, "compile_commands.json written (%zu entries)", all_srcs.count);

    nob_da_free(engine_srcs);
    nob_da_free(example_srcs);
    nob_da_free(all_srcs);
    return 0;
}
