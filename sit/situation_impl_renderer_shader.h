/***************************************************************************************************
*
*   situation_impl_renderer_shader.h - Shader Load, Compile, and Pipeline Creation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Shader file loading, GLSL/SPIR-V compile, async workers, pipeline/cache paths.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_SHADER_H
#define SITUATION_IMPL_RENDERER_SHADER_H

static char* _SituationReadSpirvFile(const char* filename, size_t* out_size) {
    if (!filename) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationReadSpirvFile: filename cannot be NULL.");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (!out_size) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationReadSpirvFile: out_size cannot be NULL.");
        return NULL;
    }

    *out_size = 0;
    unsigned int bytes_read_u32 = 0;
    unsigned char* buffer_tmp = NULL;
    SituationLoadFileData(filename, &bytes_read_u32, &buffer_tmp);
    char* buffer = (char*)buffer_tmp;

    if (!buffer) {
        char detail[SITUATION_MAX_ERROR_MSG_LEN];
        char* prior = NULL;
        const char* prior_text = "";
        if (SituationGetLastErrorMsg(&prior) == SITUATION_SUCCESS && prior && prior[0]) {
            prior_text = prior;
        }
        snprintf(detail, sizeof(detail),
                 "SPIR-V file '%s': %s", filename, prior_text);
        _SituationSetErrorFromCode(SITUATION_ERROR_SPIRV_FILE_READ_FAILED, detail);
        SituationFreeString(prior);
        return NULL;
    }

    if ((bytes_read_u32 & 3u) != 0) {
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "SPIR-V file '%s': size %u is not a multiple of 4 bytes", filename, bytes_read_u32);
        SIT_FREE(buffer);
        *out_size = 0;
        _SituationSetErrorFromCode(SITUATION_ERROR_SPIRV_INVALID_BINARY, detail);
        return NULL;
    }

    *out_size = (size_t)bytes_read_u32;
    return buffer;
}

#if defined(SITUATION_USE_OPENGL)

static void _SituationBindGLProgramStorageBlocks(GLuint program) {
    GLint ssbo_count = 0;
    glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &ssbo_count);
    if (ssbo_count <= 0) {
        return;
    }

    /* SPIR-V reflection can report the same GL_BUFFER_BINDING for multiple blocks (e.g. both 0).
     * Assign unique binding points in block_index order so layout(binding=1) blocks are not
     * overwritten when the host binds set_index 0 vs 1 (demon_hunt sky pass). */
    enum { kSitMaxSsboBlocks = 16 };
    GLuint block_indices[kSitMaxSsboBlocks];
    GLint desired_bindings[kSitMaxSsboBlocks];
    GLint assigned_bindings[kSitMaxSsboBlocks];
    int block_count = ssbo_count > kSitMaxSsboBlocks ? kSitMaxSsboBlocks : (int)ssbo_count;
    int assigned_count = 0;

    for (int i = 0; i < block_count; i++) {
        GLenum binding_prop = GL_BUFFER_BINDING;
        GLint binding_point = 0;
        block_indices[i] = (GLuint)i;
        glGetProgramResourceiv(
            program,
            GL_SHADER_STORAGE_BLOCK,
            (GLuint)i,
            1,
            &binding_prop,
            1,
            NULL,
            &binding_point);
        desired_bindings[i] = binding_point >= 0 ? binding_point : i;
    }

    for (int i = 0; i < block_count; i++) {
        GLint want = desired_bindings[i];
        int clash = 0;
        for (int j = 0; j < assigned_count; j++) {
            if (assigned_bindings[j] == want) {
                clash = 1;
                break;
            }
        }
        if (clash) {
            GLint free_slot = 0;
            for (;;) {
                int used = 0;
                for (int j = 0; j < assigned_count; j++) {
                    if (assigned_bindings[j] == free_slot) {
                        used = 1;
                        break;
                    }
                }
                if (!used) {
                    want = free_slot;
                    break;
                }
                free_slot++;
            }
        }
        assigned_bindings[assigned_count++] = want;
        glShaderStorageBlockBinding(program, block_indices[i], (GLuint)want);
    }
}

/** Assign std140 UBO blocks to layout(binding=N) after SPIR-V link (name lookup often fails). */
static void _SituationBindGLProgramUniformBlocks(GLuint program) {
    GLint ubo_count = 0;
    glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &ubo_count);
    if (ubo_count <= 0) {
        return;
    }

    enum { kSitMaxUboBlocks = 16 };
    int block_count = ubo_count > kSitMaxUboBlocks ? kSitMaxUboBlocks : (int)ubo_count;
    for (int i = 0; i < block_count; i++) {
        GLenum binding_prop = GL_BUFFER_BINDING;
        GLint binding_point = 0;
        glGetProgramResourceiv(
            program, GL_UNIFORM_BLOCK, (GLuint)i, 1, &binding_prop, 1, NULL, &binding_point);
        GLuint want = (GLuint)(binding_point >= 0 ? binding_point : i);
        glUniformBlockBinding(program, (GLuint)i, want);
    }
}

/** Fetch the full OpenGL program or shader info log (size from GL_INFO_LOG_LENGTH). Caller frees with SIT_FREE. */
static char* _SituationDupGLInfoLog(GLuint name, int is_program) {
    GLint log_len = 0;
    if (is_program) {
        glGetProgramiv(name, GL_INFO_LOG_LENGTH, &log_len);
    } else {
        glGetShaderiv(name, GL_INFO_LOG_LENGTH, &log_len);
    }
    if (log_len <= 0) {
        char* empty = (char*)SIT_MALLOC(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }
    char* buf = (char*)SIT_MALLOC((size_t)log_len + 1);
    if (!buf) {
        return NULL;
    }
    GLsizei written = 0;
    if (is_program) {
        glGetProgramInfoLog(name, log_len, &written, buf);
    } else {
        glGetShaderInfoLog(name, log_len, &written, buf);
    }
    buf[written > 0 ? (size_t)written : 0] = '\0';
    return buf;
}

static SituationError _SituationSetGLErrorFromSpirvStage(
    SituationError code, const char* stage, size_t blob_bytes, const char* driver_log) {
    char detail[SITUATION_MAX_ERROR_MSG_LEN];
    const char* log = (driver_log && driver_log[0]) ? driver_log : "(no driver log)";
    int prefix = snprintf(detail, sizeof(detail), "%s SPIR-V (%zu bytes):\n", stage, blob_bytes);
    if (prefix > 0 && (size_t)prefix < sizeof(detail)) {
        size_t cap = sizeof(detail) - (size_t)prefix - 1u;
        strncat(detail, log, cap);
    }
    return _SituationSetErrorFromCode(code, detail);
}

static SituationError _SituationValidateSpirvBinary(const void* data, size_t size, const char* label) {
    if (!data || size == 0) {
        char detail[160];
        snprintf(detail, sizeof(detail), "%s: null or empty SPIR-V blob", label);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY, detail);
        return SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY;
    }
    if ((size & 3u) != 0) {
        char detail[160];
        snprintf(detail, sizeof(detail), "%s: SPIR-V size %zu is not a multiple of 4", label, size);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY, detail);
        return SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Creates a complete, linked OpenGL shader program from SPIR-V binary blobs.
 * @details Uses `GL_ARB_gl_spirv` to load pre-compiled SPIR-V (no runtime GLSL compile).
 */
static GLuint _SituationCreateGLShaderProgramFromSpirv(const SituationSpirvBinary* vs_blob, const SituationSpirvBinary* fs_blob, SituationError* error_code) {
    if (!vs_blob || !fs_blob) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SPIR-V blob pointers cannot be null.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }
    SituationError v_err = _SituationValidateSpirvBinary(vs_blob->data, vs_blob->size, "vertex");
    if (v_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = v_err;
        return 0;
    }
    SituationError f_err = _SituationValidateSpirvBinary(fs_blob->data, fs_blob->size, "fragment");
    if (f_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = f_err;
        return 0;
    }

    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "GL_ARB_gl_spirv is required to load SPIR-V on OpenGL.");
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE;
        return 0;
    }

    GLint success = 0;

    // --- Create and load the Vertex Shader ---
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderBinary(1, &vs, GL_SHADER_BINARY_FORMAT_SPIR_V, vs_blob->data, (GLsizei)vs_blob->size);
    glSpecializeShader(vs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char* infoLog = _SituationDupGLInfoLog(vs, 0);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            SITUATION_ERROR_OPENGL_SPIRV_VS_SPECIALIZE_FAILED, "vertex", vs_blob->size,
            infoLog ? infoLog : "");
        if (error_code) {
            *error_code = spirv_err;
        }
        if (infoLog) {
            SIT_FREE(infoLog);
        }
        glDeleteShader(vs);
        return 0;
    }

    // --- Create and load the Fragment Shader ---
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderBinary(1, &fs, GL_SHADER_BINARY_FORMAT_SPIR_V, fs_blob->data, (GLsizei)fs_blob->size);
    glSpecializeShader(fs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char* infoLog = _SituationDupGLInfoLog(fs, 0);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED, "fragment", fs_blob->size,
            infoLog ? infoLog : "");
        if (error_code) {
            *error_code = spirv_err;
        }
        if (infoLog) {
            SIT_FREE(infoLog);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    // --- Link the shaders into a program ---
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);
    SIT_CHECK_GL_ERROR();

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char* infoLog = _SituationDupGLInfoLog(program, 1);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED, "graphics program",
            vs_blob->size + fs_blob->size, infoLog ? infoLog : "");
        if (error_code) {
            *error_code = spirv_err;
        }
        if (infoLog) {
            SIT_FREE(infoLog);
        }
        glDeleteProgram(program);
        return 0;
    }

    _SituationBindGLProgramStorageBlocks(program);
    _SituationBindGLProgramUniformBlocks(program);

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief Load a graphics pipeline from in-memory SPIR-V (vertex + fragment).
 * @details OpenGL: uses GL_ARB_gl_spirv (`glShaderBinary` + `glSpecializeShader`). Caller must pass SPIR-V word-aligned sizes (multiple of 4). Shader bytes need only remain valid until this function returns.
 */
SITAPI SituationError SituationLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_spirv || !fs_spirv) return SITUATION_ERROR_INVALID_PARAM;
    if (vs_len == 0 || fs_len == 0) return SITUATION_ERROR_INVALID_PARAM;
    SituationError v_err = _SituationValidateSpirvBinary(vs_spirv, vs_len, "vertex");
    if (v_err != SITUATION_SUCCESS) return v_err;
    SituationError f_err = _SituationValidateSpirvBinary(fs_spirv, fs_len, "fragment");
    if (f_err != SITUATION_SUCCESS) return f_err;
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "GL_ARB_gl_spirv is required for SituationLoadShaderFromSpirvMemory on OpenGL.");
        return SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE;
    }

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        _SituationReleaseHostGLContextForRenderThread();
        return SituationGetLastErrorCode();
    }

    SituationSpirvBinary vs_blob = { vs_spirv, vs_len };
    SituationSpirvBinary fs_blob = { fs_spirv, fs_len };
    SituationError err = SITUATION_SUCCESS;

    _SituationMakeGLContextCurrentForHostThread();
    slot->gl_program_id = _SituationCreateGLShaderProgramFromSpirv(&vs_blob, &fs_blob, &err);
    if (err == SITUATION_SUCCESS) {
        /* Lazy uniform cache: SituationSetShaderUniform fills locations on demand.
         * Skipping a full-program scan here avoids multi-second stalls after SPIR-V link
         * on very large fragment shaders (e.g. Demon Hunt ~1MB SPIR-V). */
        slot->uniform_map = NULL;
        /* Keep loader context current for back-to-back host init (CreateBuffer / CreateMesh). */
    }

    if (err != SITUATION_SUCCESS) {
        _SitFreeShaderSlot(handle);
        _SituationReleaseHostGLContextForRenderThread();
        return err;
    }

    *out_shader = handle;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader) {
    (void)layout_profile;
    return SituationLoadShaderFromSpirvMemory(vs_spirv, vs_len, fs_spirv, fs_len, out_shader);
}

SITAPI SituationError SituationBindShaderStorageBlock(SituationShader shader, const char* block_name, uint32_t binding_point) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!block_name) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || slot->gl_program_id == 0) return SITUATION_ERROR_INVALID_PARAM;

    _SituationMakeGLContextCurrentForHostThread();
    GLuint block_index = _SituationGLFindProgramResourceIndex(
        slot->gl_program_id, GL_SHADER_STORAGE_BLOCK, block_name);
    if (block_index == GL_INVALID_INDEX) {
        block_index = _SituationGLFindBlockIndexByBinding(
            slot->gl_program_id, GL_SHADER_STORAGE_BLOCK, binding_point);
        if (block_index == GL_INVALID_INDEX) {
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }
    glShaderStorageBlockBinding(slot->gl_program_id, block_index, binding_point);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationBindUniformBlock(SituationShader shader, const char* block_name, uint32_t binding_point) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!block_name) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || slot->gl_program_id == 0) return SITUATION_ERROR_INVALID_PARAM;

    _SituationMakeGLContextCurrentForHostThread();
    GLuint block_index = _SituationGLFindProgramResourceIndex(
        slot->gl_program_id, GL_UNIFORM_BLOCK, block_name);
    if (block_index == GL_INVALID_INDEX) {
        block_index = _SituationGLFindBlockIndexByBinding(
            slot->gl_program_id, GL_UNIFORM_BLOCK, binding_point);
        if (block_index == GL_INVALID_INDEX) {
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }
    glUniformBlockBinding(slot->gl_program_id, block_index, binding_point);
    return SITUATION_SUCCESS;
}

/**
 * @brief Load a graphics pipeline from precompiled SPIR-V files (`.spv`).
 * @details OpenGL: uses GL_ARB_gl_spirv (glShaderBinary + glSpecializeShader) — no GLSL compile at launch.
 */
SITAPI SituationError SituationLoadShaderFromSpirv(const char* vs_spv_path, const char* fs_spv_path, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_spv_path || !fs_spv_path) return SITUATION_ERROR_INVALID_PARAM;

    size_t vs_size = 0;
    size_t fs_size = 0;
    char* vs_data = _SituationReadSpirvFile(vs_spv_path, &vs_size);
    char* fs_data = _SituationReadSpirvFile(fs_spv_path, &fs_size);
    if (!vs_data || !fs_data) {
        if (vs_data) SIT_FREE(vs_data);
        if (fs_data) SIT_FREE(fs_data);
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationLoadShaderFromSpirvMemory(vs_data, vs_size, fs_data, fs_size, out_shader);
    SIT_FREE(vs_data);
    SIT_FREE(fs_data);

    if (err != SITUATION_SUCCESS) {
        return err;
    }

    _SituationShaderSlot* slot = _SitGetShaderSlot(*out_shader);
    if (slot) {
        slot->vs_path = _sit_strdup(vs_spv_path);
        slot->fs_path = _sit_strdup(fs_spv_path);
        slot->vs_mod_time = SituationGetFileModTime(vs_spv_path);
        slot->fs_mod_time = SituationGetFileModTime(fs_spv_path);
    }
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
/**
 * @brief [INTERNAL] Creates a single-stage (compute) OpenGL shader program from a single SPIR-V blob.
 * @details This is the modern, preferred creation path for compute shaders on the OpenGL backend when `GL_ARB_gl_spirv` is available. It uses `glShaderBinary` to load the pre-compiled SPIR-V bytecode directly, bypassing the driver's GLSL compiler.
 *
 * @par Creation Process
 *   1.  Creates a new shader object of type `GL_COMPUTE_SHADER`.
 *   2.  Loads the binary SPIR-V data into the shader object using `glShaderBinary`.
 *   3.  "Specializes" the shader using `glSpecializeShader`, which is the SPIR-V equivalent of compiling the binary for the driver.
 *   4.  Checks the `GL_COMPILE_STATUS` to verify that specialization was successful.
 *   5.  Creates a program object, attaches the specialized shader, and links the program.
 *   6.  The intermediate shader object is deleted after a successful link.
 *
 * @param cs_blob A pointer to the `_SituationSpirvBlob` containing the compiled compute shader bytecode.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful creation.
 * @return `0` on failure. A detailed error message is set internally.
 *
 * @note This function is for internal use by `_SituationCreateGLComputeProgram` only.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram`.
 *
 * @see _SituationCreateGLComputeProgram(), glShaderBinary(), glSpecializeShader()
 */
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code) {
    if (error_code) *error_code = SITUATION_SUCCESS;

    if (!cs_blob) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SPIR-V compute blob pointer is null.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }
    SituationError blob_err = _SituationValidateSpirvBinary(cs_blob->data, cs_blob->size, "compute");
    if (blob_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = blob_err;
        return 0;
    }
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "GL_ARB_gl_spirv is required to load SPIR-V compute shaders on OpenGL.");
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE;
        return 0;
    }

    GLint success = 0;
    char infoLog[SITUATION_MAX_SHADER_LOG_LEN];

    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderBinary(1, &cs, GL_SHADER_BINARY_FORMAT_SPIR_V, cs_blob->data, (GLsizei)cs_blob->size);
    glSpecializeShader(cs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    glGetShaderiv(cs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(cs, sizeof(infoLog), NULL, infoLog);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            SITUATION_ERROR_OPENGL_SPIRV_CS_SPECIALIZE_FAILED, "compute", cs_blob->size, infoLog);
        if (error_code) {
            *error_code = spirv_err;
        }
        glDeleteShader(cs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, cs);
    glLinkProgram(program);
    glDeleteShader(cs);
    SIT_CHECK_GL_ERROR();

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED, "compute program",
            cs_blob->size, infoLog);
        if (error_code) {
            *error_code = spirv_err;
        }
        glDeleteProgram(program);
        return 0;
    }

    return program;
}
#endif // Shader Compiler

#endif // SITUATION_USE_OPENGL

// =================================================================================
// --- Core internal GPU shader file loading (sit/gpu/) — shared across backends ---
// =================================================================================

/**
 * @brief [INTERNAL] Loads a core renderer shader from sit/gpu/ with dev path fallbacks.
 * @details Tries relative_path as-is, then ../, ../../, ../../../ prefixes (B5).
 */
static SituationError _SituationLoadCoreShaderFile(const char* relative_path, char** out_src) {
    if (!relative_path || !out_src) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationLoadCoreShaderFile: NULL argument.");
    }
    *out_src = NULL;

    char* text = SituationLoadFileText(relative_path);
    if (text) {
        *out_src = text;
        return SITUATION_SUCCESS;
    }

    const char* prefixes[] = { "../", "../../", "../../../", "../../../../" };
    char candidate[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        int n = snprintf(candidate, sizeof(candidate), "%s%s", prefixes[i], relative_path);
        if (n < 0 || (size_t)n >= sizeof(candidate)) {
            continue;
        }
        text = SituationLoadFileText(candidate);
        if (text) {
            *out_src = text;
            return SITUATION_SUCCESS;
        }
    }

    char* exe_base = SituationGetBasePath();
    if (exe_base) {
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
            char* joined = SituationJoinPath(exe_base, relative_path);
            if (joined) {
                text = SituationLoadFileText(joined);
                SIT_FREE(joined);
                if (text) {
                    SIT_FREE(exe_base);
                    *out_src = text;
                    return SITUATION_SUCCESS;
                }
            }
            int n = snprintf(candidate, sizeof(candidate), "%s%s", prefixes[i], relative_path);
            if (n < 0 || (size_t)n >= sizeof(candidate)) {
                continue;
            }
            joined = SituationJoinPath(exe_base, candidate);
            if (joined) {
                text = SituationLoadFileText(joined);
                SIT_FREE(joined);
                if (text) {
                    SIT_FREE(exe_base);
                    *out_src = text;
                    return SITUATION_SUCCESS;
                }
            }
        }
        SIT_FREE(exe_base);
    }

    char detail[640];
    snprintf(detail, sizeof(detail),
        "_SituationLoadCoreShaderFile: could not load core shader '%s' (tried CWD, parent prefixes, and exe-relative paths).",
        relative_path);
    return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_NOT_FOUND, detail);
}

#if defined(SITUATION_USE_OPENGL)
/** Inserts #define lines immediately after the #version directive (GLSL requires #version first). */
static char* _SituationInjectGLSLDefinesAfterVersion(const char* src, const char* defines_block) {
    if (!src) return NULL;
    if (!defines_block || !defines_block[0]) {
        return _sit_strdup(src);
    }

    const char* version_marker = "#version";
    const char* p = strstr(src, version_marker);
    if (!p) {
        size_t def_len = strlen(defines_block);
        size_t src_len = strlen(src);
        char* out = (char*)SIT_MALLOC(def_len + src_len + 1);
        if (!out) return NULL;
        memcpy(out, defines_block, def_len);
        memcpy(out + def_len, src, src_len + 1);
        return out;
    }

    const char* line_end = strchr(p, '\n');
    if (!line_end) {
        line_end = src + strlen(src);
    } else {
        ++line_end;
    }

    size_t prefix_len = (size_t)(line_end - src);
    size_t def_len = strlen(defines_block);
    size_t rest_len = strlen(line_end);
    char* out = (char*)SIT_MALLOC(prefix_len + def_len + rest_len + 1);
    if (!out) return NULL;
    memcpy(out, src, prefix_len);
    memcpy(out + prefix_len, defines_block, def_len);
    memcpy(out + prefix_len + def_len, line_end, rest_len + 1);
    return out;
}

static GLuint _SituationCreateGLCoreShaderProgram(const char* vs_path, const char* fs_path, SituationError* error_code) {
    char* vs_raw = NULL;
    char* fs_raw = NULL;
    char* vs_src = NULL;
    char* fs_src = NULL;
    GLuint program = 0;

    if (!vs_path || !fs_path) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationCreateGLCoreShaderProgram: NULL shader path.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    SituationError load_err = _SituationLoadCoreShaderFile(vs_path, &vs_raw);
    if (load_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = load_err;
        return 0;
    }
    load_err = _SituationLoadCoreShaderFile(fs_path, &fs_raw);
    if (load_err != SITUATION_SUCCESS) {
        SIT_FREE(vs_raw);
        if (error_code) *error_code = load_err;
        return 0;
    }

    static const char k_gl_core_defines[] = "#define SITUATION_USE_OPENGL 1\n";
    vs_src = _SituationInjectGLSLDefinesAfterVersion(vs_raw, k_gl_core_defines);
    fs_src = _SituationInjectGLSLDefinesAfterVersion(fs_raw, k_gl_core_defines);
    SIT_FREE(vs_raw);
    SIT_FREE(fs_raw);
    if (!vs_src || !fs_src) {
        SIT_FREE(vs_src);
        SIT_FREE(fs_src);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationCreateGLCoreShaderProgram: define injection failed.");
        if (error_code) *error_code = SITUATION_ERROR_MEMORY_ALLOCATION;
        return 0;
    }

    program = _SituationCreateGLShaderProgram(vs_src, fs_src, error_code);
    SIT_FREE(vs_src);
    SIT_FREE(fs_src);
    return program;
}

#include "sit_vd_compositor_gl_spirv_embed.h"

static GLuint _SituationCreateGLVdCompositorShaderProgram(bool path_b, SituationError* error_code) {
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "_SituationCreateGLVdCompositorShaderProgram: GL_ARB_gl_spirv required for VD compositor (#include pattern library).");
        if (error_code) {
            *error_code = SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE;
        }
        return 0;
    }

    const unsigned char* vs_data = path_b ? sit_vd_gl_compositor_vs_path_b_spv : sit_vd_gl_compositor_vs_path_a_spv;
    size_t vs_len = path_b ? sit_vd_gl_compositor_vs_path_b_spv_len : sit_vd_gl_compositor_vs_path_a_spv_len;
    const unsigned char* fs_data = path_b ? sit_vd_gl_vd_fs_spv : sit_vd_gl_composite_fs_spv;
    size_t fs_len = path_b ? sit_vd_gl_vd_fs_spv_len : sit_vd_gl_composite_fs_spv_len;

    if (vs_len < 4 || fs_len < 4) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY,
            "_SituationCreateGLVdCompositorShaderProgram: VD compositor GL SPIR-V embed missing or empty; run build/compile_vd_compositor_gl.ps1");
        if (error_code) {
            *error_code = SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY;
        }
        return 0;
    }

    SituationSpirvBinary vs_bin = { vs_data, vs_len };
    SituationSpirvBinary fs_bin = { fs_data, fs_len };
    return _SituationCreateGLShaderProgramFromSpirv(&vs_bin, &fs_bin, error_code);
}
#endif // SITUATION_USE_OPENGL

#if defined(SITUATION_USE_VULKAN)
/* Vulkan stubs for OpenGL-only block-binding API.
 * In Vulkan, binding points are set at pipeline creation via descriptor set layouts;
 * these calls are no-ops that return a clear error rather than failing to link. */
SITAPI SituationError SituationBindShaderStorageBlock(SituationShader shader, const char* block_name, uint32_t binding_point) {
    (void)shader; (void)block_name; (void)binding_point;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}
SITAPI SituationError SituationBindUniformBlock(SituationShader shader, const char* block_name, uint32_t binding_point) {
    (void)shader; (void)block_name; (void)binding_point;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}
#endif // SITUATION_USE_VULKAN (stubs)

#if defined(SITUATION_USE_VULKAN)

// =============================================================================
// --- Vulkan Shader Cache helpers (Phase 1) ---
// =============================================================================

/* FNV-1a 64-bit hash over arbitrary bytes.
 * Used to key SPIR-V blobs and GLSL source in Layer A/B/C caches.
 * Do NOT use _sit_hash_string (djb2, NUL-terminated only) for binary data. */
static inline uint64_t _SitVkHashBytes64(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 14695981039346656037ULL; /* FNV offset basis */
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL; /* FNV prime */
    }
    return h;
}

/* Fingerprint of the shaderc compile options used in _SituationVulkanCompileGLSLtoSPIRV (~6714).
 * MUST be updated if that function's compile options change. */
static inline uint64_t _SitVkShadercOptionsFingerprint(void) {
    uint64_t fp = 0x564B5F5348494E45ULL; /* "VK_SHINE" salt */
#if !defined(NDEBUG)
    fp ^= 0x1ULL; /* generate_debug_info enabled in debug builds */
#endif
    fp ^= ((uint64_t)shaderc_target_env_vulkan << 32) | (uint64_t)shaderc_env_version_vulkan_1_1;
    fp ^= ((uint64_t)shaderc_optimization_level_performance << 16);
    /* include path resolver v2: multi-path search + requesting_source relative (P0 test patterns) */
    fp ^= 0x2ULL;
    return fp;
}

#if SIT_VK_SHADER_CACHE_PHASE2
static inline VkPipelineCache _SitVkPipelineCacheHandle(void) {
    return sit_render.vk.pipeline_cache;
}
#else
static inline VkPipelineCache _SitVkPipelineCacheHandle(void) {
    return VK_NULL_HANDLE;
}
#endif

#if SIT_VK_SHADER_CACHE_PHASE2
static inline uint32_t _SitVkDynamicStateMask(void) {
    uint32_t m = 0;
    if (sit_render.vk.extended_dynamic_state_enabled) m |= 1u;
    if (sit_render.vk.depth_bias_dynamic_enabled) m |= 2u;
    if (sit_render.vk.extended_dynamic_state3_polygon_mode_enabled) m |= 4u;
    if (sit_render.vk.extended_dynamic_state3_color_write_enabled) m |= 8u;
    if (sit_render.vk.pfn_cmd_set_stencil_test_enable) m |= 16u;
    return m;
}

static inline uint32_t _SitVkCapsFingerprint(void) {
    return (sit_render.enabled_features_mask & SIT_FEATURE_FILL_MODE_NON_SOLID) ? 1u : 0u;
}
#endif

static inline void _SitVkFillCacheKey(_SitVkShaderCacheKey* key, uint64_t vs_hash, uint64_t fs_hash, uint8_t profile) {
    memset(key, 0, sizeof(*key));
    key->vs_spirv_hash = vs_hash;
    key->fs_spirv_hash = fs_hash;
    key->layout_profile = profile;
#if SIT_VK_SHADER_CACHE_PHASE2
    key->render_pass_compatibility_id = sit_render.vk.render_pass_compatibility_id;
    key->subpass_index = 0;
    key->dynamic_state_mask = _SitVkDynamicStateMask();
    key->caps_fingerprint = _SitVkCapsFingerprint();
#endif
}

/* Safe bundle dereference — the ONLY approved way to read bundle-owned GPU state.
 *
 * CRITICAL SAFETY: EVERY bind/resolve/hot-reload path MUST go through this function.
 * grep -n 'vk_bundle_ref\.bundle' situation_impl_renderer.h must return ZERO hits
 * outside this function definition.
 *
 * Returns NULL when:
 *   - ref or ref->bundle is NULL (no bundle attached)
 *   - generation mismatch (bundle was evicted and possibly recycled)
 *   - bundle state is DESTROYED or >= STALE (Phase 2+)
 * Caller must fall back to slot-owned pipelines on NULL return. */
static inline _SitVkPipelineBundle* _SitVkDerefBundle(const _SitVkPipelineBundleRef* ref) {
    if (!ref || !ref->bundle) return NULL;
    if (ref->bundle->generation != ref->generation) {
#if !defined(NDEBUG)
        sit_render.vk.shader_cache.stats.stale_derefs++;
#endif
        return NULL; /* stale — slot must re-acquire */
    }
    if (ref->bundle->state == SIT_VK_BUNDLE_DESTROYED) return NULL;
    if (ref->bundle->state >= SIT_VK_BUNDLE_STALE) return NULL; /* Phase 2+: STALE and above */
    return ref->bundle;
}

/* Phase 5 — retention revival: reload may reattach bundles marked EVICT_PENDING (not yet destroyed). */
static inline bool _SitVkShaderCacheBundleRetainedForAcquire(const _SitVkPipelineBundle* b) {
    return b && (b->state == SIT_VK_BUNDLE_READY || b->state == SIT_VK_BUNDLE_EVICT_PENDING);
}

/* Caller holds c->mutex. ref++ ; promote EVICT_PENDING → READY. */
static inline _SitVkPipelineBundle* _SitVkShaderCacheRefBundleLocked(
    _SitVkShaderCache* c,
    _SitVkPipelineBundle* b,
    uint32_t current_frame)
{
    if (!_SitVkShaderCacheBundleRetainedForAcquire(b)) return NULL;
    atomic_fetch_add(&b->ref_count, 1u);
    b->last_used_frame = current_frame;
    if (b->state == SIT_VK_BUNDLE_EVICT_PENDING)
        b->state = SIT_VK_BUNDLE_READY;
#if !defined(NDEBUG)
    c->stats.hits++;
#endif
    return b;
}

/* Caller holds c->mutex. */
static inline _SitVkPipelineBundle* _SitVkShaderCacheFindAndRefBundleLocked(
    _SitVkShaderCache* c,
    const _SitVkShaderCacheKey* key,
    uint64_t bucket_key,
    uint32_t current_frame)
{
    uint32_t bucket = _SitVkCacheBucket(bucket_key);
    for (_SitVkShaderCacheEntry* e = c->pipeline_bundle_cache[bucket]; e; e = e->next) {
        if (_SitVkCacheKeyEqual(&e->key, key) && e->bundle) {
            _SitVkPipelineBundle* b = _SitVkShaderCacheRefBundleLocked(c, e->bundle, current_frame);
            if (b) return b;
        }
    }
    return NULL;
}

// =============================================================================
// --- Vulkan Shader Cache API (Phase 1, Gate 1B) ---
// =============================================================================

/* Helper: bucket index for a 64-bit key. */
static inline uint32_t _SitVkCacheBucket(uint64_t key) {
    return (uint32_t)(key % SIT_VK_SHADER_CACHE_MAX_ENTRIES);
}

/* Helper: compare cache keys (Phase 2 uses full key; Phase 1 fields always compared). */
static inline bool _SitVkCacheKeyEqual(const _SitVkShaderCacheKey* a, const _SitVkShaderCacheKey* b) {
    if (a->vs_spirv_hash != b->vs_spirv_hash
        || a->fs_spirv_hash != b->fs_spirv_hash
        || a->layout_profile != b->layout_profile) {
        return false;
    }
#if SIT_VK_SHADER_CACHE_PHASE2
    return a->render_pass_compatibility_id == b->render_pass_compatibility_id
        && a->subpass_index == b->subpass_index
        && a->dynamic_state_mask == b->dynamic_state_mask
        && a->caps_fingerprint == b->caps_fingerprint;
#else
    return true;
#endif
}

/* ---- Init / Shutdown ---- */

static void _SitVkShaderCacheInit(_SitVkShaderCache* c) {
    memset(c, 0, sizeof(*c));
    if (mtx_init(&c->mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,
            "Shader cache: failed to initialize cache mutex.");
        return;
    }
    c->mutex_initialized = true;
#if SIT_VK_SHADER_CACHE_PHASE2
    for (uint32_t i = 0; i < SIT_VK_SHADER_BUILD_TICKET_MAX; i++) {
        atomic_init(&c->build_tickets[i].phase, 0);
        atomic_init(&c->build_tickets[i].waiter_count, 0u);
        c->build_tickets[i].layer_a_key = 0;
        c->build_tickets[i].result_bundle = NULL;
        c->build_tickets[i].leader_submit_ns = 0;
    }
#endif
}

static void _SitVkShaderCacheShutdown(_SitVkShaderCache* c) {
    if (!c || !c->mutex_initialized) return;

#if !defined(NDEBUG)
    /* Print stats before tearing down. */
    if (c->stats.hits + c->stats.misses > 0) {
        fprintf(stderr,
            "Situation [Vulkan Debug]: shader cache — hits=%llu misses=%llu evictions=%llu "
            "stale_derefs=%llu total_build_ns=%llu"
#if SIT_VK_SHADER_CACHE_PHASE2
            " variant_lazies=%llu compile_dedup_joins=%llu legacy_slot_builds=%llu bundle_slot_fallbacks=%llu"
#endif
            "\n",
            (unsigned long long)c->stats.hits,
            (unsigned long long)c->stats.misses,
            (unsigned long long)c->stats.evictions,
            (unsigned long long)c->stats.stale_derefs,
            (unsigned long long)c->stats.total_build_time_ns
#if SIT_VK_SHADER_CACHE_PHASE2
            ,
            (unsigned long long)c->stats.variant_lazies,
            (unsigned long long)c->stats.compile_dedup_joins,
            (unsigned long long)c->stats.legacy_slot_pipeline_builds,
            (unsigned long long)c->stats.bundle_resolve_slot_fallbacks
#endif
        );
        fflush(stderr);
    }
#endif

    enum { SIT_VK_HANDLE_SEEN_MAX = 512 };
    VkPipeline seen_pipelines[SIT_VK_HANDLE_SEEN_MAX];
    VkPipelineLayout seen_layouts[SIT_VK_HANDLE_SEEN_MAX];
    uint32_t seen_pipeline_count = 0;
    uint32_t seen_layout_count = 0;

    /* Drain Layer C — pipeline bundles. Destroy GPU objects immediately (device still up). */
    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitVkShaderCacheEntry* e = c->pipeline_bundle_cache[i];
        while (e) {
            _SitVkShaderCacheEntry* next = e->next;
            if (e->bundle) {
                _SitVkPipelineBundle* b = e->bundle;
                VkPipeline pipes_to_destroy[1 + SIT_VK_PIPE_VARIANT_COUNT];
                int pipe_destroy_count = 0;
                if (b->default_pipeline != VK_NULL_HANDLE) {
                    pipes_to_destroy[pipe_destroy_count++] = b->default_pipeline;
                }
#if SIT_VK_SHADER_CACHE_PHASE2
                for (uint32_t vi = 0; vi < SIT_VK_PIPE_VARIANT_COUNT; vi++) {
                    if (b->variants[vi] != VK_NULL_HANDLE) {
                        pipes_to_destroy[pipe_destroy_count++] = b->variants[vi];
                    }
                }
#endif
                for (int pi = 0; pi < pipe_destroy_count; ++pi) {
                    VkPipeline pipe = pipes_to_destroy[pi];
                    bool already = false;
                    for (uint32_t si = 0; si < seen_pipeline_count; ++si) {
                        if (seen_pipelines[si] == pipe) { already = true; break; }
                    }
                    if (already) continue;
                    if (seen_pipeline_count < SIT_VK_HANDLE_SEEN_MAX) {
                        seen_pipelines[seen_pipeline_count++] = pipe;
                    }
                    vkDestroyPipeline(sit_render.vk.device, pipe, NULL);
                }
                if (b->owns_layout && b->layout != VK_NULL_HANDLE) {
                    VkPipelineLayout layout = b->layout;
                    bool already = false;
                    for (uint32_t si = 0; si < seen_layout_count; ++si) {
                        if (seen_layouts[si] == layout) { already = true; break; }
                    }
                    if (!already) {
                        if (seen_layout_count < SIT_VK_HANDLE_SEEN_MAX) {
                            seen_layouts[seen_layout_count++] = layout;
                        }
                        vkDestroyPipelineLayout(sit_render.vk.device, layout, NULL);
                    }
                }
                b->state = SIT_VK_BUNDLE_DESTROYED;
                SIT_FREE(b);
            }
            SIT_FREE(e);
            e = next;
        }
        c->pipeline_bundle_cache[i] = NULL;
    }

    /* Drain Layer B — shader modules (each VkShaderModule handle at most once). */
    VkShaderModule seen_modules[SIT_VK_HANDLE_SEEN_MAX];
    uint32_t seen_module_count = 0;

    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitVkModulePairEntry* e = c->module_pair_cache[i];
        while (e) {
            _SitVkModulePairEntry* next = e->next;
            VkShaderModule to_destroy[2] = { e->vs_module, e->fs_module };
            for (int mi = 0; mi < 2; ++mi) {
                VkShaderModule mod = to_destroy[mi];
                if (mod == VK_NULL_HANDLE) continue;
                bool already = false;
                for (uint32_t si = 0; si < seen_module_count; ++si) {
                    if (seen_modules[si] == mod) { already = true; break; }
                }
                if (already) continue;
                if (seen_module_count < SIT_VK_HANDLE_SEEN_MAX) {
                    seen_modules[seen_module_count++] = mod;
                }
                vkDestroyShaderModule(sit_render.vk.device, mod, NULL);
            }
            SIT_FREE(e);
            e = next;
        }
        c->module_pair_cache[i] = NULL;
    }

    /* Drain Layer A — SPIR-V blobs (CPU only). */
    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitVkSpirvBlobEntry* e = c->spirv_blob_cache[i];
        while (e) {
            _SitVkSpirvBlobEntry* next = e->next;
            SIT_FREE(e->vs_data);
            SIT_FREE(e->fs_data);
            SIT_FREE(e);
            e = next;
        }
        c->spirv_blob_cache[i] = NULL;
    }

    mtx_destroy(&c->mutex);
    c->mutex_initialized = false;
}

/* ---- Layer A: SPIR-V blob cache ---- */

/* Look up or insert a SPIR-V blob pair.
 * On hit: increments ref_count, returns pointer into cached blob.
 * On miss: allocates copies of vs/fs data and inserts; ref_count = 1.
 * Returns NULL only on allocation failure. Caller must hold cache mutex. */
static _SitVkSpirvBlobEntry* _SitVkShaderCacheLookupOrInsertSpirv(
    _SitVkShaderCache* c,
    uint64_t layer_a_key,
    const uint8_t* vs_data, size_t vs_size,
    const uint8_t* fs_data, size_t fs_size)
{
    uint32_t bucket = _SitVkCacheBucket(layer_a_key);
    _SitVkSpirvBlobEntry* e = c->spirv_blob_cache[bucket];
    while (e) {
        if (e->layer_a_key == layer_a_key) {
            atomic_fetch_add(&e->ref_count, 1u);
            return e;
        }
        e = e->next;
    }
    /* Miss — insert. */
    _SitVkSpirvBlobEntry* n = (_SitVkSpirvBlobEntry*)SIT_MALLOC(sizeof(_SitVkSpirvBlobEntry));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->vs_data = (uint8_t*)SIT_MALLOC(vs_size);
    n->fs_data = (uint8_t*)SIT_MALLOC(fs_size);
    if (!n->vs_data || !n->fs_data) {
        SIT_FREE(n->vs_data); SIT_FREE(n->fs_data); SIT_FREE(n);
        return NULL;
    }
    memcpy(n->vs_data, vs_data, vs_size);
    memcpy(n->fs_data, fs_data, fs_size);
    n->vs_size = vs_size;
    n->fs_size = fs_size;
    n->layer_a_key = layer_a_key;
    atomic_init(&n->ref_count, 1u);
    n->next = c->spirv_blob_cache[bucket];
    c->spirv_blob_cache[bucket] = n;
    return n;
}

/* ---- Layer B: shader module cache ---- */

/* Look up or create a module pair for a given SPIR-V hash pair.
 * On hit: increments ref_count.
 * On miss: calls vkCreateShaderModule x2 (main thread only!) and inserts.
 * Returns NULL on Vulkan failure. Caller must NOT hold cache mutex during this call
 * (mutex is taken internally for map access only). */
static _SitVkModulePairEntry* _SitVkShaderCacheAcquireModules(
    _SitVkShaderCache* c,
    uint64_t vs_spirv_hash, uint64_t fs_spirv_hash,
    const void* vs_data, size_t vs_size,
    const void* fs_data, size_t fs_size)
{
    uint64_t combined = vs_spirv_hash ^ (fs_spirv_hash << 1);
    uint32_t bucket = _SitVkCacheBucket(combined);

    mtx_lock(&c->mutex);
    _SitVkModulePairEntry* e = c->module_pair_cache[bucket];
    while (e) {
        if (e->vs_spirv_hash == vs_spirv_hash && e->fs_spirv_hash == fs_spirv_hash) {
            atomic_fetch_add(&e->ref_count, 1u);
            mtx_unlock(&c->mutex);
            return e;
        }
        e = e->next;
    }
    mtx_unlock(&c->mutex);

    /* Miss — create modules outside the lock (vkCreate* must never run under it). */
    VkShaderModule vs_mod = _SituationVulkanCreateShaderModuleEx(
        vs_data, vs_size, "cache_vs", SITUATION_ERROR_VULKAN_SPIRV_VS_MODULE_FAILED);
    if (vs_mod == VK_NULL_HANDLE) return NULL;
    VkShaderModule fs_mod = _SituationVulkanCreateShaderModuleEx(
        fs_data, fs_size, "cache_fs", SITUATION_ERROR_VULKAN_SPIRV_FS_MODULE_FAILED);
    if (fs_mod == VK_NULL_HANDLE) {
        vkDestroyShaderModule(sit_render.vk.device, vs_mod, NULL);
        return NULL;
    }

    _SitVkModulePairEntry* n = (_SitVkModulePairEntry*)SIT_MALLOC(sizeof(_SitVkModulePairEntry));
    if (!n) {
        vkDestroyShaderModule(sit_render.vk.device, vs_mod, NULL);
        vkDestroyShaderModule(sit_render.vk.device, fs_mod, NULL);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "Shader cache: failed to allocate _SitVkModulePairEntry.");
        return NULL;
    }
    memset(n, 0, sizeof(*n));
    n->vs_spirv_hash = vs_spirv_hash;
    n->fs_spirv_hash = fs_spirv_hash;
    n->vs_module = vs_mod;
    n->fs_module = fs_mod;
    atomic_init(&n->ref_count, 1u);

    mtx_lock(&c->mutex);
    /* Re-check after vkCreate* — a concurrent load may have inserted while we were unlocked. */
    e = c->module_pair_cache[bucket];
    while (e) {
        if (e->vs_spirv_hash == vs_spirv_hash && e->fs_spirv_hash == fs_spirv_hash) {
            atomic_fetch_add(&e->ref_count, 1u);
            mtx_unlock(&c->mutex);
            vkDestroyShaderModule(sit_render.vk.device, vs_mod, NULL);
            vkDestroyShaderModule(sit_render.vk.device, fs_mod, NULL);
            SIT_FREE(n);
            return e;
        }
        e = e->next;
    }
    n->next = c->module_pair_cache[bucket];
    c->module_pair_cache[bucket] = n;
    mtx_unlock(&c->mutex);
    return n;
}

/* ---- _SitVkCreateDefaultSimplePipeline ---- */

/* Builds the single Phase 1 "default" pipeline:
 *   layout = caller-supplied (MESH profile: dynamic_ubo_layout + text_sampler_layout, 128B push constants)
 *   topology  = TRIANGLE_LIST
 *   vertex    = 1 binding, stride 3*float, 1 attr (POSITION, R32G32B32_SFLOAT, offset 0)
 *   cull      = NONE, front = CW, polygon = FILL, flags = 0u
 * The bundle stores the returned handle as default_pipeline.
 * This is the pipeline used by _SitVulkanResolveGraphicsPipeline when bundle is active
 * and simple stride + fill + no-back-cull is requested.
 *
 * State parity with _SituationVulkanCreateGraphicsPipeline(flags=0u, topology=TRIANGLE_LIST):
 *   depth_test=ON, depth_write=OFF, compare=LESS  (TRIANGLE_LIST, flags=0 branch ~23544)
 *   blend=ON, SRC_ALPHA/ONE_MINUS_SRC_ALPHA (flags=0, not BLEND_OPAQUE ~23554)
 *   dynamic states via _SitVulkanFillGraphicsDynamicStates (~23638) */
static VkPipeline _SitVkCreateDefaultSimplePipeline(
    VkPipelineLayout layout,
    VkShaderModule vs_module,
    VkShaderModule fs_module)
{
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride  = 3u * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attr = {
        .binding  = 0,
        .location = SIT_ATTR_POSITION,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 0
    };

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" }
    };
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions    = &attr
    };
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };
    VkPipelineRasterizationStateCreateInfo rast = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode      = VK_POLYGON_MODE_FILL,
        .cullMode         = VK_CULL_MODE_NONE,
        .frontFace        = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth        = 1.0f,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE
    };
    /* Parity with _SituationVulkanCreateGraphicsPipeline(flags=0u, TRIANGLE_LIST):
     * depth test ON, depth write OFF, compare LESS. */
    VkPipelineDepthStencilStateCreateInfo ds = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable   = VK_TRUE,
        .depthWriteEnable  = VK_FALSE,   /* TRIANGLE_LIST + flags=0 → write off */
        .depthCompareOp    = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
    /* Parity: flags=0 → blendEnable=TRUE, SRC_ALPHA/ONE_MINUS_SRC_ALPHA. */
    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable             = VK_TRUE,
        .srcColorBlendFactor     = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor     = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp            = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor     = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor     = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp            = VK_BLEND_OP_ADD
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &blend_att
    };
    /* Dynamic states: must match _SitVulkanFillGraphicsDynamicStates (~23506). */
    VkDynamicState dyn_states[16];
    uint32_t dyn_count = 0;
    _SitVulkanFillGraphicsDynamicStates(dyn_states, &dyn_count);
    VkPipelineDynamicStateCreateInfo dyn = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dyn_count,
        .pDynamicStates    = dyn_states
    };
    VkGraphicsPipelineCreateInfo ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState      = &vp,
        .pRasterizationState = &rast,
        .pMultisampleState   = &ms,
        .pDepthStencilState  = &ds,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dyn,
        .layout              = layout,
        .renderPass          = sit_render.vk.main_window_render_pass,
        .subpass             = 0
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(sit_render.vk.device, _SitVkPipelineCacheHandle(), 1, &ci, NULL, &pipeline) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "Shader cache: vkCreateGraphicsPipelines for default bundle pipeline failed.");
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

static VkPipeline _SitVkCreateVDDynamicPipelineFromModules(
    VkPipelineLayout layout, VkShaderModule vs_module, VkShaderModule fs_module,
    VkFormat color_fmt, VkFormat depth_fmt, VkSampleCountFlagBits rasterization_samples)
{
    if (layout == VK_NULL_HANDLE || vs_module == VK_NULL_HANDLE || fs_module == VK_NULL_HANDLE ||
        color_fmt == VK_FORMAT_UNDEFINED || !sit_render.vk.dynamic_rendering_enabled) {
        return VK_NULL_HANDLE;
    }
    if (rasterization_samples == 0) {
        rasterization_samples = VK_SAMPLE_COUNT_1_BIT;
    }

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride  = 3u * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attr = {
        .binding  = 0,
        .location = SIT_ATTR_POSITION,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 0
    };

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" }
    };
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions    = &attr
    };
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };
    VkPipelineRasterizationStateCreateInfo rast = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode      = VK_POLYGON_MODE_FILL,
        .cullMode         = VK_CULL_MODE_NONE,
        .frontFace        = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth        = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = rasterization_samples,
        .sampleShadingEnable = (rasterization_samples != VK_SAMPLE_COUNT_1_BIT &&
                                 sit_render.vk.dynamic_ms_sample_shading_enable) ? VK_TRUE : VK_FALSE,
        .minSampleShading = sit_render.vk.dynamic_ms_min_sample_shading,
        .alphaToCoverageEnable = (rasterization_samples != VK_SAMPLE_COUNT_1_BIT &&
                                  sit_render.vk.dynamic_ms_alpha_to_coverage_enable) ? VK_TRUE : VK_FALSE
    };
    VkBool32 depth_test_enable = (depth_fmt != VK_FORMAT_UNDEFINED) ? VK_TRUE : VK_FALSE;
    VkPipelineDepthStencilStateCreateInfo ds = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable   = depth_test_enable,
        .depthWriteEnable  = VK_FALSE,
        .depthCompareOp    = VK_COMPARE_OP_LESS
    };
    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable             = VK_TRUE,
        .srcColorBlendFactor     = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor     = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp            = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor     = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor     = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp            = VK_BLEND_OP_ADD
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_att
    };
    VkDynamicState dyn_states[16];
    uint32_t dyn_count = 0;
    _SitVulkanFillGraphicsDynamicStates(dyn_states, &dyn_count);
    VkPipelineDynamicStateCreateInfo dyn = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dyn_count,
        .pDynamicStates    = dyn_states
    };
    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_fmt,
        .depthAttachmentFormat = depth_fmt
    };
    VkGraphicsPipelineCreateInfo ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering_info,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState      = &vp,
        .pRasterizationState = &rast,
        .pMultisampleState   = &ms,
        .pDepthStencilState  = &ds,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dyn,
        .layout              = layout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(sit_render.vk.device, _SitVkPipelineCacheHandle(), 1, &ci, NULL, &pipeline) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

#if SIT_VK_SHADER_CACHE_PHASE2
/* Builds one lazy pipeline variant from bundle-owned modules + layout. Main thread only. */
static VkPipeline _SitVkCreateBundlePipelineForVariant(_SitVkPipelineBundle* bundle, int variant_id) {
    if (!bundle || variant_id < 0 || variant_id >= SIT_VK_PIPE_VARIANT_COUNT) return VK_NULL_HANDLE;

    VkVertexInputBindingDescription binding = {0};
    VkVertexInputAttributeDescription attrs[4];
    uint32_t attr_count = 0;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
    VkFrontFace front = VK_FRONT_FACE_CLOCKWISE;
    VkPolygonMode poly = VK_POLYGON_MODE_FILL;

    switch (( _SitVkPipeVariantId)variant_id) {
        case SIT_VK_VAR_PBR_NONE:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TANGENT, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 6 * sizeof(float) };
            attrs[3] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 10 * sizeof(float) };
            attr_count = 4;
            break;
        case SIT_VK_VAR_PBR_BACK_CCW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TANGENT, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 6 * sizeof(float) };
            attrs[3] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 10 * sizeof(float) };
            attr_count = 4;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case SIT_VK_VAR_PBR_BACK_CW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TANGENT, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 6 * sizeof(float) };
            attrs[3] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 10 * sizeof(float) };
            attr_count = 4;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_CLOCKWISE;
            break;
        case SIT_VK_VAR_PBR_LINE:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TANGENT, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 6 * sizeof(float) };
            attrs[3] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 10 * sizeof(float) };
            attr_count = 4;
            poly = VK_POLYGON_MODE_LINE;
            break;
        case SIT_VK_VAR_LEGACY_NONE:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 6 * sizeof(float) };
            attr_count = 3;
            break;
        case SIT_VK_VAR_LEGACY_BACK_CCW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 6 * sizeof(float) };
            attr_count = 3;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case SIT_VK_VAR_LEGACY_BACK_CW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 6 * sizeof(float) };
            attr_count = 3;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_CLOCKWISE;
            break;
        case SIT_VK_VAR_LEGACY_LINE:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attrs[1] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_NORMAL, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 3 * sizeof(float) };
            attrs[2] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_TEXCOORD_0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 6 * sizeof(float) };
            attr_count = 3;
            poly = VK_POLYGON_MODE_LINE;
            break;
        case SIT_VK_VAR_SIMPLE_BACK_CCW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = 3u * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attr_count = 1;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case SIT_VK_VAR_SIMPLE_BACK_CW:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = 3u * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attr_count = 1;
            cull = VK_CULL_MODE_BACK_BIT; front = VK_FRONT_FACE_CLOCKWISE;
            break;
        case SIT_VK_VAR_SIMPLE_LINE:
            binding = (VkVertexInputBindingDescription){ .binding = 0, .stride = 3u * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            attrs[0] = (VkVertexInputAttributeDescription){ .binding = 0, .location = SIT_ATTR_POSITION, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 };
            attr_count = 1;
            poly = VK_POLYGON_MODE_LINE;
            break;
        default:
            return VK_NULL_HANDLE;
    }

    if (poly == VK_POLYGON_MODE_LINE &&
        (sit_render.enabled_features_mask & SIT_FEATURE_FILL_MODE_NON_SOLID) == 0u) {
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = bundle->vs_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = bundle->fs_module, .pName = "main" }
    };
    VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = attr_count,
        .pVertexAttributeDescriptions    = attrs
    };
    VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };
    VkPipelineRasterizationStateCreateInfo rast = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = poly, .cullMode = cull, .frontFace = front,
        .lineWidth = 1.0f, .depthClampEnable = VK_FALSE, .rasterizerDiscardEnable = VK_FALSE
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE
    };
    VkPipelineDepthStencilStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE
    };
    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &blend_att
    };
    VkDynamicState dyn_states[16];
    uint32_t dyn_count = 0;
    _SitVulkanFillGraphicsDynamicStates(dyn_states, &dyn_count);
    VkPipelineDynamicStateCreateInfo dyn = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dyn_count, .pDynamicStates = dyn_states
    };
    VkGraphicsPipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vi, .pInputAssemblyState = &ia,
        .pViewportState = &vp, .pRasterizationState = &rast,
        .pMultisampleState = &ms, .pDepthStencilState = &ds,
        .pColorBlendState = &blend, .pDynamicState = &dyn,
        .layout = bundle->layout,
        .renderPass = sit_render.vk.main_window_render_pass,
        .subpass = 0
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(sit_render.vk.device, _SitVkPipelineCacheHandle(), 1, &ci, NULL, &pipeline) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "Shader cache: vkCreateGraphicsPipelines for lazy bundle variant failed.");
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

/* Lazy-create a pipeline variant on the main thread during draw resolve. */
static VkPipeline _SitVkEnsurePipelineVariant(_SitVkPipelineBundle* bundle, int variant_id) {
    if (!bundle || variant_id < 0 || variant_id >= SIT_VK_PIPE_VARIANT_COUNT)
        return bundle ? bundle->default_pipeline : VK_NULL_HANDLE;

    uint32_t bit = 1u << (uint32_t)variant_id;
    if (atomic_load(&bundle->variant_ready_mask) & bit)
        return bundle->variants[variant_id];

    VkPipeline created = _SitVkCreateBundlePipelineForVariant(bundle, variant_id);
    if (created == VK_NULL_HANDLE)
        return bundle->default_pipeline;

    _SitVkShaderCache* c = &sit_render.vk.shader_cache;
    mtx_lock(&c->mutex);
    if (!(atomic_load(&bundle->variant_ready_mask) & bit)) {
        bundle->variants[variant_id] = created;
        atomic_fetch_or(&bundle->variant_ready_mask, bit);
#if !defined(NDEBUG)
        c->stats.variant_lazies++;
#endif
    } else {
        vkDestroyPipeline(sit_render.vk.device, created, NULL);
        created = bundle->variants[variant_id];
    }
    mtx_unlock(&c->mutex);
    return created;
}

/* Map current draw state to a lazy variant index, or -1 for default_pipeline (simple/none/fill). */
static int _SitVkPickVariantForDraw(size_t stride, VkCullModeFlags cull, VkFrontFace front, VkPolygonMode poly) {
    bool simple = (stride <= 3u * sizeof(float) || stride == 0u);
    bool legacy = (!simple && stride <= (3u + 3u + 2u) * sizeof(float));

    if (poly == VK_POLYGON_MODE_LINE) {
        if (simple) return SIT_VK_VAR_SIMPLE_LINE;
        if (legacy) return SIT_VK_VAR_LEGACY_LINE;
        return SIT_VK_VAR_PBR_LINE;
    }
    if (cull == VK_CULL_MODE_BACK_BIT) {
        bool ccw = (front == VK_FRONT_FACE_COUNTER_CLOCKWISE);
        if (simple) return ccw ? SIT_VK_VAR_SIMPLE_BACK_CCW : SIT_VK_VAR_SIMPLE_BACK_CW;
        if (legacy) return ccw ? SIT_VK_VAR_LEGACY_BACK_CCW : SIT_VK_VAR_LEGACY_BACK_CW;
        return ccw ? SIT_VK_VAR_PBR_BACK_CCW : SIT_VK_VAR_PBR_BACK_CW;
    }
    if (simple) return -1;
    if (legacy) return SIT_VK_VAR_LEGACY_NONE;
    return SIT_VK_VAR_PBR_NONE;
}

static _SitVkShaderBuildTicket* _SitVkAcquireBuildTicket(_SitVkShaderCache* c, uint64_t layer_a_key, bool* out_leader) {
    *out_leader = false;
    if (!c || !c->mutex_initialized) return NULL;

    uint64_t now_ns = _SitGetMonotonicTimeNS();

    mtx_lock(&c->mutex);
    _SitVkShaderBuildTicket* found = NULL;
    _SitVkShaderBuildTicket* free_slot = NULL;
    for (uint32_t i = 0; i < SIT_VK_SHADER_BUILD_TICKET_MAX; i++) {
        _SitVkShaderBuildTicket* t = &c->build_tickets[i];
        int phase = atomic_load(&t->phase);
        if (t->layer_a_key == layer_a_key && phase != 0) {
            /* Skip stale tickets: a phase-1 leader that has been sitting longer than the
             * async compile deadline is assumed to be orphaned (e.g. longjmp without cleanup).
             * Joining it would cause _SitVkWaitBuildTicketLayerA to spin until timeout.
             * Instead, we fall through and allocate a fresh leader slot below. */
            if (phase == 1 && t->leader_submit_ns != 0 &&
                (now_ns - t->leader_submit_ns) > SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS) {
                continue; /* stale leader — do not join */
            }
            found = t;
            break;
        }
        if (phase == 0 && atomic_load(&t->waiter_count) == 0u && !free_slot)
            free_slot = t;
    }
    _SitVkShaderBuildTicket* t = found ? found : free_slot;
    if (!t) {
        mtx_unlock(&c->mutex);
        return NULL;
    }
    atomic_fetch_add(&t->waiter_count, 1u);
    if (!found) {
        t->layer_a_key = layer_a_key;
        t->result_bundle = NULL;
        t->leader_submit_ns = now_ns;
        atomic_store(&t->phase, 1);
        *out_leader = true;
    } else {
#if !defined(NDEBUG)
        c->stats.compile_dedup_joins++;
#endif
    }
    mtx_unlock(&c->mutex);
    return t;
}

static void _SitVkReleaseBuildTicket(_SitVkShaderCache* c, _SitVkShaderBuildTicket* ticket) {
    if (!c || !ticket) return;
    mtx_lock(&c->mutex);
    uint32_t prev = atomic_fetch_sub(&ticket->waiter_count, 1u);
    if (prev == 1u) {
        atomic_store(&ticket->phase, 0);
        ticket->layer_a_key = 0;
        ticket->result_bundle = NULL;
    }
    mtx_unlock(&c->mutex);
}

static void _SitVkShaderCacheMarkBundlesStale(_SitVkShaderCache* c) {
    if (!c || !c->mutex_initialized) return;
    mtx_lock(&c->mutex);
    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitVkShaderCacheEntry* e = c->pipeline_bundle_cache[i];
        while (e) {
            if (e->bundle && e->bundle->state == SIT_VK_BUNDLE_READY)
                e->bundle->state = SIT_VK_BUNDLE_STALE;
            e = e->next;
        }
    }
    mtx_unlock(&c->mutex);
}

/* Bump render-pass epoch after main_window_render_pass create (init or recreate). */
static void _SitVkShaderCacheOnMainRenderPassCreated(void) {
    _SitVkShaderCache* c = &sit_render.vk.shader_cache;
    if (sit_render.vk.render_pass_compatibility_id != 0u) {
        _SitVkShaderCacheMarkBundlesStale(c);
        sit_render.vk.render_pass_compatibility_id++;
        if (sit_render.vk.render_pass_compatibility_id == 0u)
            sit_render.vk.render_pass_compatibility_id = 1u;
    } else {
        sit_render.vk.render_pass_compatibility_id = 1u;
    }
}

static bool _SitVkWaitBuildTicketLayerA(_SitVkShaderBuildTicket* ticket) {
    if (!ticket) return true;
    for (uint32_t spin = 0; spin < 200000000u; spin++) {
        int ph = atomic_load(&ticket->phase);
        if (ph >= 2) return true;
        if (ph == 0) return false;
        if (spin > 100) {
            SITUATION_SLEEP_MS(1);
        } else {
            thrd_yield();
        }
    }
    return false;
}

static SituationError _SitVkSyncLoadFromBuildTicketFollower(
    _SituationShaderSlot* slot,
    _SitVkShaderCache* c,
    _SitVkShaderBuildTicket* ticket,
    uint64_t layer_a_key,
    SituationShader handle,
    SituationShader* out_shader)
{
    if (!_SitVkWaitBuildTicketLayerA(ticket)) {
        _SitVkReleaseBuildTicket(c, ticket);
        _SitFreeShaderSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED,
            "Sync build ticket follower: leader compile failed or timed out.");
    }
    _SitVkSpirvBlobEntry* la = NULL;
    mtx_lock(&c->mutex);
    uint32_t fb = _SitVkCacheBucket(layer_a_key);
    for (_SitVkSpirvBlobEntry* e = c->spirv_blob_cache[fb]; e; e = e->next) {
        if (e->layer_a_key == layer_a_key) { la = e; break; }
    }
    mtx_unlock(&c->mutex);
    if (!la) {
        _SitVkReleaseBuildTicket(c, ticket);
        _SitFreeShaderSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED,
            "Sync build ticket follower: Layer A missing after leader compile.");
    }
    _SitVkTryAttachBundle(slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size,
        SIT_SPIRV_LAYOUT_PROFILE_MESH);
    if (!_SitVkDerefBundle(&slot->vk_bundle_ref)) {
        _SitVkReleaseBuildTicket(c, ticket);
        _SitFreeShaderSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "Sync build ticket follower: bundle attach failed.");
    }
    _SitVkReleaseBuildTicket(c, ticket);
    *out_shader = handle;
    return SITUATION_SUCCESS;
}

static void _SitVkFinishSyncBuildTicket(_SitVkShaderCache* c, _SitVkShaderBuildTicket* ticket,
        bool leader, int final_phase) {
    if (!ticket) return;
    if (leader && final_phase > 0)
        atomic_store(&ticket->phase, final_phase);
    _SitVkReleaseBuildTicket(c, ticket);
}
#endif /* SIT_VK_SHADER_CACHE_PHASE2 */

/* ---- Layer C: pipeline bundle cache ---- */

/* Acquire a bundle for the given key (Phase 1: 3-field match).
 * On HIT:  increments ref_count; fills bundle_ref; updates last_used_frame.
 * On MISS: creates layout (MESH profile) + default_pipeline via _SitVkCreateDefaultSimplePipeline;
 *          inserts; ref_count = 1.
 * modules is the Layer B entry (must be non-NULL and already ref'd by caller).
 * Returns NULL on Vulkan failure or OOM. Caller must NOT hold cache mutex. */
static _SitVkPipelineBundle* _SitVkShaderCacheAcquireBundle(
    _SitVkShaderCache* c,
    const _SitVkShaderCacheKey* key,
    _SitVkModulePairEntry* modules,
    uint32_t current_frame)
{
    uint64_t bucket_key = key->vs_spirv_hash ^ (key->fs_spirv_hash << 1) ^ key->layout_profile;
    uint32_t bucket = _SitVkCacheBucket(bucket_key);

    mtx_lock(&c->mutex);
    _SitVkPipelineBundle* retained = _SitVkShaderCacheFindAndRefBundleLocked(c, key, bucket_key, current_frame);
    mtx_unlock(&c->mutex);
    if (retained) return retained;

    /* Miss — build pipeline outside the lock. */
#if !defined(NDEBUG)
    uint64_t t0 = _SitGetMonotonicTimeNS();
#endif

    /* Create pipeline layout for MESH profile (owns it). */
    VkDescriptorSetLayout layouts[] = {
        sit_render.vk.dynamic_ubo_layout,
        sit_render.vk.text_sampler_layout
    };
    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0, .size = 128
    };
    VkPipelineLayoutCreateInfo pli = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 2,
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pcr
    };
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(sit_render.vk.device, &pli, NULL, &pl) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "Shader cache: vkCreatePipelineLayout for bundle failed.");
        return NULL;
    }

    VkPipeline pipeline = _SitVkCreateDefaultSimplePipeline(pl, modules->vs_module, modules->fs_module);
    if (pipeline == VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(sit_render.vk.device, pl, NULL);
        return NULL;
    }

    _SitVkPipelineBundle* b = (_SitVkPipelineBundle*)SIT_MALLOC(sizeof(_SitVkPipelineBundle));
    if (!b) {
        vkDestroyPipeline(sit_render.vk.device, pipeline, NULL);
        vkDestroyPipelineLayout(sit_render.vk.device, pl, NULL);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "Shader cache: failed to allocate _SitVkPipelineBundle.");
        return NULL;
    }
    memset(b, 0, sizeof(*b));
    b->key              = *key;
    b->content_hash     = key->vs_spirv_hash ^ key->fs_spirv_hash;
    atomic_init(&b->ref_count, 1u);
    b->last_used_frame  = current_frame;
    b->state            = SIT_VK_BUNDLE_READY;
    b->layout           = pl;
    b->owns_layout      = true;
    b->vs_module        = modules->vs_module;  /* not owned — Layer B owns modules */
    b->fs_module        = modules->fs_module;
    b->default_pipeline = pipeline;
#if SIT_VK_SHADER_CACHE_PHASE2
    atomic_init(&b->pin_count, 0u);
    atomic_init(&b->variant_ready_mask, 0u);
    memset(b->variants, 0, sizeof(b->variants));
#endif
    /* generation starts at 0; bump on eviction. */

    _SitVkShaderCacheEntry* entry = (_SitVkShaderCacheEntry*)SIT_MALLOC(sizeof(_SitVkShaderCacheEntry));
    if (!entry) {
        vkDestroyPipeline(sit_render.vk.device, pipeline, NULL);
        vkDestroyPipelineLayout(sit_render.vk.device, pl, NULL);
        SIT_FREE(b);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "Shader cache: failed to allocate _SitVkShaderCacheEntry.");
        return NULL;
    }
    entry->key    = *key;
    entry->bundle = b;

    mtx_lock(&c->mutex);
    /* Re-check after vkCreate* — a concurrent load may have inserted while we were unlocked. */
    retained = _SitVkShaderCacheFindAndRefBundleLocked(c, key, bucket_key, current_frame);
    if (retained) {
        mtx_unlock(&c->mutex);
        vkDestroyPipeline(sit_render.vk.device, pipeline, NULL);
        vkDestroyPipelineLayout(sit_render.vk.device, pl, NULL);
        SIT_FREE(b);
        SIT_FREE(entry);
        return retained;
    }
    entry->next = c->pipeline_bundle_cache[bucket];
    c->pipeline_bundle_cache[bucket] = entry;
    mtx_unlock(&c->mutex);

#if !defined(NDEBUG)
    c->stats.misses++;
    c->stats.total_build_time_ns += _SitGetMonotonicTimeNS() - t0;
#endif
    return b;
}

/* Decrement ref_count. Bundle is eligible for eviction once ref hits 0.
 * Clears bundle_ref on the slot. */
static void _SitVkShaderCacheReleaseBundle(
    _SitVkShaderCache* c,
    _SitVkPipelineBundleRef* ref)
{
    if (!ref || !ref->bundle) return;
    _SitVkPipelineBundle* b = ref->bundle;
    /* Clear ref before decrement to prevent re-entry. */
    ref->bundle     = NULL;
    ref->generation = 0;
    mtx_lock(&c->mutex);
    uint32_t prev = atomic_fetch_sub(&b->ref_count, 1u);
    if (prev == 1u && b->state == SIT_VK_BUNDLE_READY) {
        b->state = SIT_VK_BUNDLE_EVICT_PENDING;
    }
    mtx_unlock(&c->mutex);
}

/* ---- _SitVkAttachBundleRef: single approved write path for vk_bundle_ref ----
 *
 * CRITICAL SAFETY: This is the ONLY place that may ATTACH a live bundle to a slot.
 * Reads go through _SitVkDerefBundle only.
 *
 * The only other sites that write vk_bundle_ref.bundle are the two stale-ref cleanup
 * branches in SituationUnloadShader and SituationReloadShader (both set .bundle = NULL
 * only when _SitVkDerefBundle already returned NULL, meaning the bundle is already
 * evicted/destroyed and no ref-count decrement is appropriate). Those NULL-clears are
 * intentional and do NOT violate this rule — they are cleanup, not attach.
 *
 * grep -n 'vk_bundle_ref\.bundle\s*=' situation_impl_renderer.h should return exactly
 * three hits: this assign, the unload stale-clear, and the reload stale-clear. */
static inline void _SitVkAttachBundleRef(_SituationShaderSlot* slot, _SitVkPipelineBundle* bundle) {
    slot->vk_bundle_ref.bundle     = bundle;
    slot->vk_bundle_ref.generation = bundle->generation;
    slot->vk_bound_pipeline_cache  = VK_NULL_HANDLE;
#if SIT_VK_SHADER_CACHE_PHASE2
    if (atomic_load(&bundle->pin_count) < SIT_VK_SHADER_CACHE_HOT_PIN_MAX)
        atomic_fetch_add(&bundle->pin_count, 1u);
#endif
    /* Propagate the bundle's layout onto the slot so CmdBindPipeline can set
     * current_pipeline_layout_for_push_constants correctly on cache-hit-only slots
     * (which have no slot->vk_pipeline_layout of their own).
     * profile is always MESH for Phase 1 bundles. */
    slot->vk_pipeline_layout         = bundle->layout;
    slot->vk_owns_pipeline_layout    = false; /* bundle owns it */
    slot->vk_spirv_layout_profile    = SIT_SPIRV_LAYOUT_PROFILE_MESH;
}

/* ---- _SitVkInsertLayerA: populate Layer A blob cache after shaderc succeeds ----
 *
 * Called once after shaderc produces SPIR-V and the pipeline build is about to start.
 * Idempotent: if the entry already exists (concurrent load), just bumps its ref.
 * Caller holds no mutex — this function takes and releases shader_cache.mutex internally.
 *
 * layer_a_key = hash(vs_src) ^ (hash(fs_src) << 1) ^ _SitVkShadercOptionsFingerprint()
 * Caller must supply the key it computed during the pre-shaderc Layer A miss check. */
static void _SitVkInsertLayerA(
    uint64_t layer_a_key,
    const void* vs_data, size_t vs_size,
    const void* fs_data, size_t fs_size)
{
#if !defined(SIT_VK_SHADER_CACHE_ENABLE) || !SIT_VK_SHADER_CACHE_ENABLE
    (void)layer_a_key;
    (void)vs_data; (void)vs_size; (void)fs_data; (void)fs_size;
#else
    _SitVkShaderCache* c = &sit_render.vk.shader_cache;
    mtx_lock(&c->mutex);
    uint32_t bucket = _SitVkCacheBucket(layer_a_key);
    _SitVkSpirvBlobEntry* e = c->spirv_blob_cache[bucket];
    while (e) {
        if (e->layer_a_key == layer_a_key) {
            atomic_fetch_add(&e->ref_count, 1u); /* already present — bump ref */
            mtx_unlock(&c->mutex);
            return;
        }
        e = e->next;
    }
    /* Miss — allocate outside the mutex would be cleaner but insertion is rare;
     * alloc with mutex held is safe since no Vulkan calls happen here. */
    _SitVkSpirvBlobEntry* n = (_SitVkSpirvBlobEntry*)SIT_MALLOC(sizeof(_SitVkSpirvBlobEntry));
    if (!n) { mtx_unlock(&c->mutex); return; }
    memset(n, 0, sizeof(*n));
    n->vs_data = (uint8_t*)SIT_MALLOC(vs_size);
    n->fs_data = (uint8_t*)SIT_MALLOC(fs_size);
    if (!n->vs_data || !n->fs_data) {
        SIT_FREE(n->vs_data); SIT_FREE(n->fs_data); SIT_FREE(n);
        mtx_unlock(&c->mutex);
        return;
    }
    memcpy(n->vs_data, vs_data, vs_size);
    memcpy(n->fs_data, fs_data, fs_size);
    n->vs_size       = vs_size;
    n->fs_size       = fs_size;
    n->layer_a_key   = layer_a_key;
    atomic_init(&n->ref_count, 1u);
    n->next = c->spirv_blob_cache[bucket];
    c->spirv_blob_cache[bucket] = n;
    mtx_unlock(&c->mutex);
#endif
}


/* ---- _SitVkTryAttachBundle: post-compile bundle attach ----
 *
 * Called after SPIR-V is available (either from shaderc or memory load).
 * Layout profile MESH only; non-MESH loads skip this and take the legacy path.
 *
 * On success: _SitVkAttachBundleRef populates slot->vk_bundle_ref (and layout).
 * On failure: bundle_ref is left zeroed; caller continues with legacy pipeline. */
static void _SitVkTryAttachBundle(
    _SituationShaderSlot* slot,
    const void*  vs_data, size_t vs_size,
    const void*  fs_data, size_t fs_size,
    SituationSpirvLayoutProfile layout_profile)
{
#if !defined(SIT_VK_SHADER_CACHE_ENABLE) || !SIT_VK_SHADER_CACHE_ENABLE
    (void)slot; (void)vs_data; (void)vs_size; (void)fs_data; (void)fs_size; (void)layout_profile;
    return;
#else
    /* Phase 1: only cache MESH-profile shaders. */
    if (layout_profile != SIT_SPIRV_LAYOUT_PROFILE_MESH) return;

    _SitVkShaderCache* c = &sit_render.vk.shader_cache;

    uint64_t vs_hash = _SitVkHashBytes64(vs_data, vs_size);
    uint64_t fs_hash = _SitVkHashBytes64(fs_data, fs_size);

    /* Layer B: acquire or create module pair (handles its own locking). */
    _SitVkModulePairEntry* modules = _SitVkShaderCacheAcquireModules(
        c, vs_hash, fs_hash, vs_data, vs_size, fs_data, fs_size);
    if (!modules) return; /* module creation failed; fall through to legacy slot pipelines */

    /* Layer C: acquire or create bundle. */
    _SitVkShaderCacheKey key;
    _SitVkFillCacheKey(&key, vs_hash, fs_hash, (uint8_t)layout_profile);
    _SitVkPipelineBundle* bundle = _SitVkShaderCacheAcquireBundle(
        c, &key, modules, sit_render.vk.current_frame_index);
    if (!bundle) return; /* pipeline creation failed; fall through */

    _SitVkAttachBundleRef(slot, bundle);
    _SitVkPinLastSpirvOnSlot(slot, vs_data, vs_size, fs_data, fs_size);
#endif
}



/* Must be called AFTER _SituationFlushGraveyard(frame_index) each frame.
 * Scans Layer C for bundles where:
 *   ref_count == 0  AND  last_used_frame + EVICT_DELAY <= current_frame  AND  state == EVICT_PENDING
 * Eligible bundles have their GPU objects queued to the graveyard, generation bumped,
 * and the map entry freed. */
static void _SitVkShaderCacheProcessEvictions(_SitVkShaderCache* c, uint32_t current_frame) {
    if (!c->mutex_initialized) return;
    mtx_lock(&c->mutex);
#if SIT_VK_SHADER_CACHE_PHASE2
    for (uint32_t pi = 0; pi < SIT_VK_SHADER_CACHE_MAX_ENTRIES; pi++) {
        _SitVkShaderCacheEntry* pe = c->pipeline_bundle_cache[pi];
        while (pe) {
            _SitVkPipelineBundle* pb = pe->bundle;
            if (pb && atomic_load(&pb->pin_count) > 0u
                    && pb->last_used_frame + SIT_VK_SHADER_CACHE_HOT_PIN_FRAMES <= current_frame) {
                atomic_fetch_sub(&pb->pin_count, 1u);
            }
            pe = pe->next;
        }
    }
#endif
    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitVkShaderCacheEntry** prev_ptr = &c->pipeline_bundle_cache[i];
        _SitVkShaderCacheEntry*  e        = *prev_ptr;
        while (e) {
            _SitVkPipelineBundle* b = e->bundle;
            bool evict = b
                && atomic_load(&b->ref_count) == 0u
                && b->state == SIT_VK_BUNDLE_EVICT_PENDING
                && (b->last_used_frame + SIT_VK_SHADER_CACHE_EVICT_DELAY_FRAMES <= current_frame)
#if SIT_VK_SHADER_CACHE_PHASE2
                && atomic_load(&b->pin_count) == 0u
#endif
                ;
            if (evict) {
                b->state = SIT_VK_BUNDLE_DESTROYED;
                b->generation++;  /* invalidate any live BundleRefs */
                /* Destroy GPU objects via graveyard (fence already passed for this frame_index). */
                mtx_unlock(&c->mutex);
#if SIT_VK_SHADER_CACHE_PHASE2
                for (uint32_t vi = 0; vi < SIT_VK_PIPE_VARIANT_COUNT; vi++) {
                    if (b->variants[vi] != VK_NULL_HANDLE)
                        _SituationDeferDestroyPipeline(b->variants[vi], VK_NULL_HANDLE);
                }
#endif
                _SituationDeferDestroyPipeline(b->default_pipeline,
                    b->owns_layout ? b->layout : VK_NULL_HANDLE);
                /* Modules are owned by Layer B — decrement Layer B ref instead.
                 * For Phase 1 simplicity: destroy modules directly if ref hits zero.
                 * (Layer B cleanup runs at shutdown; runtime module eviction is deferred.) */
                mtx_lock(&c->mutex);
                b->default_pipeline = VK_NULL_HANDLE;
                b->layout           = VK_NULL_HANDLE;
                SIT_FREE(b);
                e->bundle = NULL;
                _SitVkShaderCacheEntry* dead = e;
                *prev_ptr = e->next;
                e = e->next;
                SIT_FREE(dead);
#if !defined(NDEBUG)
                c->stats.evictions++;
#endif
            } else {
                prev_ptr = &e->next;
                e = e->next;
            }
        }
    }
    mtx_unlock(&c->mutex);
}


#if defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SituationVkAsyncShaderLoad {
    _Atomic int compile_done; /* 0 = pending, -3 = compiling, 1 = SPIR-V ready, -1 = failed, -2 = abandoned (worker frees) */
    /* Job-queue handle of the compile job (0 = ran inline / no job). Lets the poll path
     * detect a LOST job: handle settled in the queue while compile_done is still 0 means
     * the worker never executed our function — report SITUATION_ERROR_THREAD_JOB_LOST
     * instead of returning IN_PROGRESS forever. */
    SituationJobId compile_job;
    /* Monotonic submit timestamp (0 = not applicable). Lets the poll path enforce
     * SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS and report
     * SITUATION_ERROR_SHADER_COMPILE_TIMEOUT for a wedged/starved compile worker. */
    uint64_t submit_time_ns;
    SituationSpirvLayoutProfile layout_profile;
    char* vs_src;
    char* fs_src;
    uint8_t* vs_spirv_copy;
    size_t vs_spirv_len;
    uint8_t* fs_spirv_copy;
    size_t fs_spirv_len;
    _SituationSpirvBlob vs_spirv;
    _SituationSpirvBlob fs_spirv;
#if SIT_VK_SHADER_CACHE_PHASE2
    struct _SitVkShaderBuildTicket* build_ticket;
    bool build_ticket_leader;
    uint64_t layer_a_key;
#endif
} _SituationVkAsyncShaderLoad;

/* HARDENING: void by design — single free path for async compile ctx. */
static void _SituationVkAsyncCompileFreeCtx(_SituationVkAsyncShaderLoad* ctx) {
    if (!ctx) return;
#if SIT_VK_SHADER_CACHE_PHASE2
    if (ctx->build_ticket) {
        // [FIX] If we are freeing before Poll has inserted the results into the cache,
        // we MUST abort the ticket (phase 0) so followers retry, rather than tricking
        // them into thinking the cache is populated (phase 3).
        _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, ctx->build_ticket, ctx->build_ticket_leader, 0);
        ctx->build_ticket = NULL;
    }
#endif
    if (ctx->vs_src) SIT_FREE(ctx->vs_src);
    if (ctx->fs_src) SIT_FREE(ctx->fs_src);
    if (ctx->vs_spirv_copy) SIT_FREE(ctx->vs_spirv_copy);
    if (ctx->fs_spirv_copy) SIT_FREE(ctx->fs_spirv_copy);
    _SituationFreeSpirvBlob(&ctx->vs_spirv);
    _SituationFreeSpirvBlob(&ctx->fs_spirv);
    SIT_FREE(ctx);
}

/* CAS abandon, retire pool job, detach slot. Returns true if this thread owns abandon. */
static bool _SituationVkAsyncCompileAbandon(
    _SituationVkAsyncShaderLoad* ctx, _SituationShaderSlot* slot, bool detach_slot) {
    if (!ctx) return false;
#if SIT_VK_SHADER_CACHE_PHASE2
    if (ctx->build_ticket && atomic_load(&ctx->build_ticket->waiter_count) > 1u) {
        return false; /* Do not abandon compilation if other shaders are waiting as followers */
    }
#endif
    int expected = atomic_load(&ctx->compile_done);
    if (expected != 0 && expected != -3) return false;
    if (!atomic_compare_exchange_strong(&ctx->compile_done, &expected, -2)) return false;
#if defined(SITUATION_ENABLE_THREADING)
    if (ctx->compile_job != 0 && sit_gs.thread_pool.is_active) {
        _SitThreadPoolRetireOrphanedJobMain(&sit_gs.thread_pool, ctx->compile_job);
        SituationWaitForJob(&sit_gs.thread_pool, ctx->compile_job);
        ctx->compile_job = 0;
    }
#endif
    if (detach_slot && slot) slot->vk_async_load = NULL;
    return true;
}

static _SitVkAsyncCompileProgressResult _SituationVkAsyncCompileProgress(
    _SituationVkAsyncShaderLoad* ctx, _SituationShaderSlot* slot, uint32_t mode_flags, uint64_t elapsed_ns) {
    if (!ctx) return SIT_VK_ASYNC_PROGRESS_FAILED;

    int done = atomic_load(&ctx->compile_done);
    if (done == 1) return SIT_VK_ASYNC_PROGRESS_SPIRV_READY;
    if (done == -1) return SIT_VK_ASYNC_PROGRESS_FAILED;
    if (done == -2) return SIT_VK_ASYNC_PROGRESS_ABANDONED;
    if (done != 0 && done != -3) return SIT_VK_ASYNC_PROGRESS_FAILED;

#if defined(SITUATION_ENABLE_THREADING)
    if (ctx->compile_job != 0 && sit_gs.thread_pool.is_active &&
        _SitJobHandleSettled(&sit_gs.thread_pool, ctx->compile_job)) {
        if (atomic_load(&ctx->compile_done) == 0) {
            ctx->compile_job = 0;
            _SituationVkAsyncCompileFreeCtx(ctx);
            if (slot) slot->vk_async_load = NULL;
            return (mode_flags & SIT_VK_ASYNC_PROGRESS_POLL)
                ? SIT_VK_ASYNC_PROGRESS_LOST
                : SIT_VK_ASYNC_PROGRESS_ABANDONED;
        }
        return SIT_VK_ASYNC_PROGRESS_IN_PROGRESS;
    }

    /* Phase B: starvation drive / unclaimed fast path.
     * If the job was submitted but has remained unclaimed in the pool for > UNCLAIMED_FAST_NS (100 ms),
     * we retire it from the pool and run it inline on the main thread. */
    if ((mode_flags & SIT_VK_ASYNC_PROGRESS_POLL) &&
        ctx->compile_job != 0 && sit_gs.thread_pool.is_active &&
        elapsed_ns > SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS) {
        
        uint32_t q_idx = (ctx->compile_job >> SIT_ID_QUEUE_SHIFT) & 1;
        uint32_t slot_idx = ctx->compile_job & SIT_ID_SLOT_MASK;
        SituationThreadPool* pool = &sit_gs.thread_pool;
        
        mtx_lock(&pool->queues[q_idx].lock);
        SituationJob* job = &pool->queues[q_idx].jobs[slot_idx & pool->queues[q_idx].mask];
        uint32_t expected_gen = (ctx->compile_job >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;
        
        bool unclaimed = (atomic_load(&job->generation) == (uint16_t)expected_gen) &&
                         (!atomic_load(&job->is_completed)) &&
                         (atomic_load(&job->dependency_count) == 0);
        mtx_unlock(&pool->queues[q_idx].lock);
        
        if (unclaimed) {
            _SitThreadPoolRetireOrphanedJobMain(pool, ctx->compile_job);
            ctx->compile_job = 0;
            _SituationVkAsyncCompileWorker(ctx, NULL);
            /* compile_done might be updated now. Re-read done. */
            done = atomic_load(&ctx->compile_done);
            if (done == 1) return SIT_VK_ASYNC_PROGRESS_SPIRV_READY;
            if (done == -1) return SIT_VK_ASYNC_PROGRESS_FAILED;
            if (done == -2) return SIT_VK_ASYNC_PROGRESS_ABANDONED;
            if (done != 0 && done != -3) return SIT_VK_ASYNC_PROGRESS_FAILED;
        }
    }
#endif

    if ((mode_flags & SIT_VK_ASYNC_PROGRESS_POLL) &&
        ctx->submit_time_ns != 0 &&
        elapsed_ns > SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS) {
        if (_SituationVkAsyncCompileAbandon(ctx, slot, true)) {
            return SIT_VK_ASYNC_PROGRESS_TIMEOUT;
        }
        return SIT_VK_ASYNC_PROGRESS_IN_PROGRESS;
    }

    if (mode_flags & (SIT_VK_ASYNC_PROGRESS_UNLOAD | SIT_VK_ASYNC_PROGRESS_SHUTDOWN)) {
        uint64_t abandon_ns = (mode_flags & SIT_VK_ASYNC_PROGRESS_SHUTDOWN)
            ? SITUATION_VULKAN_ASYNC_SHUTDOWN_NS
            : SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS;
        if (elapsed_ns > abandon_ns) {
            if (_SituationVkAsyncCompileAbandon(ctx, slot, true)) {
                return SIT_VK_ASYNC_PROGRESS_ABANDONED;
            }
            return SIT_VK_ASYNC_PROGRESS_IN_PROGRESS;
        }
        if ((mode_flags & SIT_VK_ASYNC_PROGRESS_SHUTDOWN) &&
            elapsed_ns > SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS) {
            if (_SituationVkAsyncCompileAbandon(ctx, slot, true)) {
                return SIT_VK_ASYNC_PROGRESS_ABANDONED;
            }
            return SIT_VK_ASYNC_PROGRESS_IN_PROGRESS;
        }
    }

    return SIT_VK_ASYNC_PROGRESS_IN_PROGRESS;
}

/* HARDENING: void by design — async teardown; poll path sets terminal error before free. */
static void _SituationVulkanFreeAsyncShaderLoad(_SituationShaderSlot* slot) {
    if (!slot || !slot->vk_async_load) return;
    _SituationVkAsyncShaderLoad* ctx = (_SituationVkAsyncShaderLoad*)slot->vk_async_load;
    uint64_t wait_start_ns = _SitGetMonotonicTimeNS();
    for (;;) {
        uint64_t elapsed_ns = _SitGetMonotonicTimeNS() - wait_start_ns;
        _SitVkAsyncCompileProgressResult pr = _SituationVkAsyncCompileProgress(
            ctx, slot, SIT_VK_ASYNC_PROGRESS_UNLOAD, elapsed_ns);
        if (pr == SIT_VK_ASYNC_PROGRESS_ABANDONED) {
            /* v2.4.345+: must release build ticket — early return here left phase-1 tickets
             * orphaned so the next BeginLoad with the same Layer-A key joined as a follower
             * and SituationPollShaderLoad spun IN_PROGRESS forever (async_shader_poll_after_unload_during_load). */
#if defined(SITUATION_ENABLE_THREADING)
            if (ctx->compile_job != 0 && sit_gs.thread_pool.is_active) {
                _SitThreadPoolRetireOrphanedJobMain(&sit_gs.thread_pool, ctx->compile_job);
                SituationWaitForJob(&sit_gs.thread_pool, ctx->compile_job);
                ctx->compile_job = 0;
            }
#endif
            _SituationVkAsyncCompileFreeCtx(ctx);
            slot->vk_async_load = NULL;
            return;
        }
        if (pr == SIT_VK_ASYNC_PROGRESS_IN_PROGRESS) {
            _SituationVkAsyncShaderCompilePump(ctx);
            SITUATION_SLEEP_MS(1);
            continue;
        }
        break;
    }
#if defined(SITUATION_ENABLE_THREADING)
    if (ctx->compile_job != 0 && sit_gs.thread_pool.is_active) {
        _SitThreadPoolRetireOrphanedJobMain(&sit_gs.thread_pool, ctx->compile_job);
        SituationWaitForJob(&sit_gs.thread_pool, ctx->compile_job);
        ctx->compile_job = 0;
    }
#endif
    _SituationVkAsyncCompileFreeCtx(ctx);
    slot->vk_async_load = NULL;
}

static void _SituationVkAsyncCompileWorker(void* payload, void* unused) {
    (void)unused;
    _SituationVkAsyncShaderLoad* ctx = (_SituationVkAsyncShaderLoad*)payload;
    if (!ctx) return;

    /* -3 = compiling on this thread; prevents double shaderc if pump inlines while a
     * worker also claimed the same job slot. */
    int expected = 0;
    if (!atomic_compare_exchange_strong(&ctx->compile_done, &expected, -3)) {
        /* Abandon path in FreeAsyncShaderLoad owns ctx cleanup — avoid double-free here. */
        if (atomic_load(&ctx->compile_done) == -2) {
            return;
        }
        return;
    }

    ctx->vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(ctx->vs_src, "async_vertex", shaderc_glsl_vertex_shader);
    ctx->fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(ctx->fs_src, "async_fragment", shaderc_glsl_fragment_shader);

    if (atomic_load(&ctx->compile_done) == -2) {
        return; /* unload/shutdown abandon — FreeAsyncShaderLoad frees ctx + ticket */
    }

    if (ctx->vs_spirv.data && ctx->fs_spirv.data) {
        atomic_store(&ctx->compile_done, 1);
    } else {
        atomic_store(&ctx->compile_done, -1);
    }
}

#if defined(SITUATION_ENABLE_THREADING)
static void _SituationVkAsyncShaderCompilePump(_SituationVkAsyncShaderLoad* ctx) {
    if (!ctx || atomic_load(&ctx->compile_done) != 0) return;
    /* compile_job != 0: SituationSubmitJobEx owns execution on a worker. Never inline
     * compile here — that sets compile_done while the job slot stays open and
     * SituationWaitForJob in FreeAsync spins forever (v2.4.234–235 hang). */
    if (ctx->compile_job != 0 && sit_gs.thread_pool.is_active) return;
    _SituationVkAsyncCompileWorker(ctx, NULL);
}
#else
static void _SituationVkAsyncShaderCompilePump(_SituationVkAsyncShaderLoad* ctx) {
    if (!ctx || atomic_load(&ctx->compile_done) != 0) return;
    _SituationVkAsyncCompileWorker(ctx, NULL);
}
#endif
#endif /* SITUATION_ENABLE_SHADER_COMPILER */

#if !defined(NDEBUG) && defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE && SIT_VK_SHADER_CACHE_PHASE2
static inline void _SitVkStatLegacySlotPipelineBuild(void) {
    sit_render.vk.shader_cache.stats.legacy_slot_pipeline_builds++;
}
#else
static inline void _SitVkStatLegacySlotPipelineBuild(void) { }
#endif

static SituationError _SituationVulkanBuildGraphicsPipelinesOnSlot(
    _SituationShaderSlot* slot,
    const void* vs_spirv, size_t vs_len,
    const void* fs_spirv, size_t fs_len,
    SituationSpirvLayoutProfile layout_profile) {
    if (!slot || !vs_spirv || !fs_spirv || vs_len == 0 || fs_len == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if ((vs_len & 3u) != 0 || (fs_len & 3u) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, "SPIR-V bytecode size must be a multiple of 4.");
        return SITUATION_ERROR_VULKAN_SPIRV_INVALID;
    }
    if (layout_profile > SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    slot->vk_spirv_layout_profile = layout_profile;
    slot->vk_owns_pipeline_layout = false;
    _SitVkPinLastSpirvOnSlot(slot, vs_spirv, vs_len, fs_spirv, fs_len);

    if (layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        _SitVkStatLegacySlotPipelineBuild();
        VkDescriptorSetLayout layouts[] = { sit_render.vk.dynamic_ubo_layout, sit_render.vk.text_sampler_layout };
        VkPushConstantRange push_constant_range = { .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS, .offset = 0, .size = 128 };
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };
        if (vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &slot->vk_pipeline_layout) != VK_SUCCESS) {
            return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
        }
        slot->vk_owns_pipeline_layout = true;
    } else if (layout_profile == SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO) {
        slot->vk_pipeline_layout = sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_TWO_SSBOS];
        if (slot->vk_pipeline_layout == VK_NULL_HANDLE) {
            return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
        }
    } else if (layout_profile == SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER) {
        slot->vk_pipeline_layout = sit_render.vk.graphics_spirv_layout_ubo_ssbo_sampler;
        if (slot->vk_pipeline_layout == VK_NULL_HANDLE) {
            return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
        }
    } else {
        slot->vk_pipeline_layout = sit_render.vk.graphics_spirv_layout_ubo_ssbo;
        if (slot->vk_pipeline_layout == VK_NULL_HANDLE) {
            return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
        }
    }

    VkVertexInputBindingDescription binding_desc_pbr = { .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_descs_pbr[4];
    attr_descs_pbr[0].binding = 0; attr_descs_pbr[0].location = SIT_ATTR_POSITION; attr_descs_pbr[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_pbr[0].offset = 0;
    attr_descs_pbr[1].binding = 0; attr_descs_pbr[1].location = SIT_ATTR_NORMAL; attr_descs_pbr[1].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_pbr[1].offset = 3 * sizeof(float);
    attr_descs_pbr[2].binding = 0; attr_descs_pbr[2].location = SIT_ATTR_TANGENT; attr_descs_pbr[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; attr_descs_pbr[2].offset = 6 * sizeof(float);
    attr_descs_pbr[3].binding = 0; attr_descs_pbr[3].location = SIT_ATTR_TEXCOORD_0; attr_descs_pbr[3].format = VK_FORMAT_R32G32_SFLOAT; attr_descs_pbr[3].offset = 10 * sizeof(float);

    slot->vk_pipeline = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_pbr, 4, attr_descs_pbr, 0u,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_back_ccw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_pbr, 4, attr_descs_pbr, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_back_cw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_pbr, 4, attr_descs_pbr, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);

    VkVertexInputBindingDescription binding_desc_legacy = { .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_descs_legacy[3];
    attr_descs_legacy[0].binding = 0; attr_descs_legacy[0].location = SIT_ATTR_POSITION; attr_descs_legacy[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_legacy[0].offset = 0;
    attr_descs_legacy[1].binding = 0; attr_descs_legacy[1].location = SIT_ATTR_NORMAL; attr_descs_legacy[1].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_legacy[1].offset = 3 * sizeof(float);
    attr_descs_legacy[2].binding = 0; attr_descs_legacy[2].location = SIT_ATTR_TEXCOORD_0; attr_descs_legacy[2].format = VK_FORMAT_R32G32_SFLOAT; attr_descs_legacy[2].offset = 6 * sizeof(float);

    slot->vk_pipeline_legacy = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_legacy, 3, attr_descs_legacy, 0u,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_legacy_back_ccw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_legacy, 3, attr_descs_legacy, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_legacy_back_cw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_legacy, 3, attr_descs_legacy, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);

    VkVertexInputBindingDescription binding_desc_simple = { .binding = 0, .stride = 3 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_descs_simple[1];
    attr_descs_simple[0].binding = 0; attr_descs_simple[0].location = SIT_ATTR_POSITION; attr_descs_simple[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_simple[0].offset = 0;

    slot->vk_pipeline_simple = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_simple, 1, attr_descs_simple, 0u,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_simple_back_ccw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_simple, 1, attr_descs_simple, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL);
    slot->vk_pipeline_simple_back_cw = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_simple, 1, attr_descs_simple, 0u,
        VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);

    if ((sit_render.enabled_features_mask & SIT_FEATURE_FILL_MODE_NON_SOLID) != 0u) {
        slot->vk_pipeline_line = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_pbr, 4, attr_descs_pbr, 0u,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE);
        slot->vk_pipeline_legacy_line = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_legacy, 3, attr_descs_legacy, 0u,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE);
        slot->vk_pipeline_simple_line = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv, vs_len, fs_spirv, fs_len, slot->vk_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_simple, 1, attr_descs_simple, 0u,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE);
    }

    if (slot->vk_pipeline == VK_NULL_HANDLE || slot->vk_pipeline_legacy == VK_NULL_HANDLE ||
        slot->vk_pipeline_back_ccw == VK_NULL_HANDLE || slot->vk_pipeline_back_cw == VK_NULL_HANDLE ||
        slot->vk_pipeline_legacy_back_ccw == VK_NULL_HANDLE || slot->vk_pipeline_legacy_back_cw == VK_NULL_HANDLE ||
        slot->vk_pipeline_simple == VK_NULL_HANDLE ||
        slot->vk_pipeline_simple_back_ccw == VK_NULL_HANDLE || slot->vk_pipeline_simple_back_cw == VK_NULL_HANDLE) {
        if (slot->vk_owns_pipeline_layout && slot->vk_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(sit_render.vk.device, slot->vk_pipeline_layout, NULL);
        }
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    return SITUATION_SUCCESS;
}

/* Phase 6 — shared Layer A fast path for sync + async GLSL mesh load.
 * Returns +1 if slot is ready (bundle or legacy fallback), 0 if Layer A miss,
 * -1 on failure (*out_err set). On 0, *out_layer_a_key is the pre-shaderc key. */
static int _SitVkTryMeshLoadFromLayerA(
    _SituationShaderSlot* slot,
    const char* vs_code,
    const char* fs_code,
    uint64_t* out_layer_a_key,
    SituationError* out_err)
{
#if !defined(SIT_VK_SHADER_CACHE_ENABLE) || !SIT_VK_SHADER_CACHE_ENABLE
    (void)slot; (void)vs_code; (void)fs_code; (void)out_layer_a_key; (void)out_err;
    return 0;
#else
    if (!slot || !vs_code || !fs_code || !out_layer_a_key) return 0;

    uint64_t vs_hash_src = _SitVkHashBytes64(vs_code, strlen(vs_code));
    uint64_t fs_hash_src = _SitVkHashBytes64(fs_code, strlen(fs_code));
    *out_layer_a_key = vs_hash_src ^ (fs_hash_src << 1) ^ _SitVkShadercOptionsFingerprint();

    uint32_t la_bucket = _SitVkCacheBucket(*out_layer_a_key);
    _SitVkShaderCache* c = &sit_render.vk.shader_cache;
    mtx_lock(&c->mutex);
    _SitVkSpirvBlobEntry* la = c->spirv_blob_cache[la_bucket];
    while (la) {
        if (la->layer_a_key == *out_layer_a_key) {
            atomic_fetch_add(&la->ref_count, 1u);
            break;
        }
        la = la->next;
    }
    mtx_unlock(&c->mutex);
    if (!la) return 0;

    uint64_t vs_hash = _SitVkHashBytes64(la->vs_data, la->vs_size);
    uint64_t fs_hash = _SitVkHashBytes64(la->fs_data, la->fs_size);
    _SitVkShaderCacheKey cache_key;
    _SitVkFillCacheKey(&cache_key, vs_hash, fs_hash, (uint8_t)SIT_SPIRV_LAYOUT_PROFILE_MESH);
    uint64_t ck = vs_hash ^ (fs_hash << 1) ^ cache_key.layout_profile;

    mtx_lock(&c->mutex);
    _SitVkPipelineBundle* hit_bundle = _SitVkShaderCacheFindAndRefBundleLocked(
        c, &cache_key, ck, (uint32_t)sit_render.vk.current_frame_index);
    atomic_fetch_sub(&la->ref_count, 1u);
    mtx_unlock(&c->mutex);

    if (hit_bundle) {
        _SitVkAttachBundleRef(slot, hit_bundle);
        _SitVkPinLastSpirvOnSlot(slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size);
        return 1;
    }

    _SitVkTryAttachBundle(slot,
        la->vs_data, la->vs_size, la->fs_data, la->fs_size,
        SIT_SPIRV_LAYOUT_PROFILE_MESH);
    if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
        _SitVkPinLastSpirvOnSlot(slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size);
        return 1;
    }

    SituationError pipe_err = _SituationVulkanBuildGraphicsPipelinesOnSlot(
        slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size,
        SIT_SPIRV_LAYOUT_PROFILE_MESH);
    if (pipe_err == SITUATION_SUCCESS) {
        _SitVkPinLastSpirvOnSlot(slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size);
        return 1;
    }
    if (out_err) *out_err = pipe_err;
    return -1;
#endif
}

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static SituationError _SituationVulkanBuildMeshPipelinesOnSlot(
    _SituationShaderSlot* slot, const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len) {
    return _SituationVulkanBuildGraphicsPipelinesOnSlot(
        slot, vs_spirv, vs_len, fs_spirv, fs_len, SIT_SPIRV_LAYOUT_PROFILE_MESH);
}

static SituationError _SituationPollVkAsyncShaderLoad(_SituationShaderSlot* slot) {
    if (!slot || !slot->vk_async_load) {
        return SITUATION_SUCCESS;
    }

    _SituationVkAsyncShaderLoad* ctx = (_SituationVkAsyncShaderLoad*)slot->vk_async_load;
    _SituationVkAsyncShaderCompilePump(ctx);

    uint64_t elapsed_ns = (ctx->submit_time_ns != 0)
        ? (_SitGetMonotonicTimeNS() - ctx->submit_time_ns) : 0;
    _SitVkAsyncCompileProgressResult pr = _SituationVkAsyncCompileProgress(
        ctx, slot, SIT_VK_ASYNC_PROGRESS_POLL, elapsed_ns);

    if (pr == SIT_VK_ASYNC_PROGRESS_IN_PROGRESS) {
        return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
    }
    if (pr == SIT_VK_ASYNC_PROGRESS_LOST) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_JOB_LOST,
            "Async shader compile job was retired by the job queue without executing.");
    }
    if (pr == SIT_VK_ASYNC_PROGRESS_TIMEOUT) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILE_TIMEOUT,
            "Async shader compile exceeded deadline; abandoned wedged compile job.");
    }
    if (pr == SIT_VK_ASYNC_PROGRESS_ABANDONED) {
        _SituationVkAsyncCompileFreeCtx(ctx);
        slot->vk_async_load = NULL;
        return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED,
            "Async shader compile was abandoned.");
    }

#if SIT_VK_SHADER_CACHE_PHASE2
    if (ctx->build_ticket && !ctx->build_ticket_leader) {
        if (atomic_load(&ctx->build_ticket->phase) < 2) {
            return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
        }
        _SitVkShaderCache* fc = &sit_render.vk.shader_cache;
        _SitVkSpirvBlobEntry* la = NULL;
        mtx_lock(&fc->mutex);
        uint32_t fb = _SitVkCacheBucket(ctx->layer_a_key);
        for (_SitVkSpirvBlobEntry* e = fc->spirv_blob_cache[fb]; e; e = e->next) {
            if (e->layer_a_key == ctx->layer_a_key) { la = e; break; }
        }
        mtx_unlock(&fc->mutex);
        if (!la) {
            _SitVkReleaseBuildTicket(fc, ctx->build_ticket);
            ctx->build_ticket = NULL;
            _SituationVulkanFreeAsyncShaderLoad(slot);
            return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED,
                "Build ticket follower: Layer A missing after leader compile.");
        }
        _SitVkTryAttachBundle(slot, la->vs_data, la->vs_size, la->fs_data, la->fs_size, ctx->layout_profile);
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            _SitVkReleaseBuildTicket(fc, ctx->build_ticket);
            ctx->build_ticket = NULL;
            _SituationVulkanFreeAsyncShaderLoad(slot);
            return SITUATION_SUCCESS;
        }
        _SitVkReleaseBuildTicket(fc, ctx->build_ticket);
        ctx->build_ticket = NULL;
        _SituationVulkanFreeAsyncShaderLoad(slot);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "Build ticket follower: bundle attach failed.");
    }
#endif

    int done = atomic_load(&ctx->compile_done);
    if (done < 0) {
        if (!ctx->vs_spirv.data && !ctx->vs_spirv_copy) {
            _SituationSetErrorFromCode(
                SITUATION_ERROR_SHADER_COMPILATION_FAILED,
                "Async Vulkan vertex GLSL to SPIR-V failed (shaderc).");
        } else if (!ctx->fs_spirv.data && !ctx->fs_spirv_copy) {
            _SituationSetErrorFromCode(
                SITUATION_ERROR_SHADER_COMPILATION_FAILED,
                "Async Vulkan fragment GLSL to SPIR-V failed (shaderc).");
        } else {
            _SituationSetErrorFromCode(
                SITUATION_ERROR_SHADER_COMPILATION_FAILED,
                "Async Vulkan shader compile failed.");
        }
        _SituationVulkanFreeAsyncShaderLoad(slot);
        return SituationGetLastErrorCode();
    }

    const void* vs = ctx->vs_spirv.data ? (const void*)ctx->vs_spirv.data : (const void*)ctx->vs_spirv_copy;
    size_t vs_len = ctx->vs_spirv.data ? ctx->vs_spirv.size : ctx->vs_spirv_len;
    const void* fs = ctx->fs_spirv.data ? (const void*)ctx->fs_spirv.data : (const void*)ctx->fs_spirv_copy;
    size_t fs_len = ctx->fs_spirv.data ? ctx->fs_spirv.size : ctx->fs_spirv_len;

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* [Shader Cache] Layer A insert on main thread — source strings are still alive on ctx.
     * Worker must not touch cache maps; this runs on the poll (main) thread only. */
    if (ctx->vs_src && ctx->fs_src && ctx->layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        uint64_t la_key = _SitVkHashBytes64(ctx->vs_src, strlen(ctx->vs_src))
                        ^ (_SitVkHashBytes64(ctx->fs_src, strlen(ctx->fs_src)) << 1)
                        ^ _SitVkShadercOptionsFingerprint();
        _SitVkInsertLayerA(la_key, vs, vs_len, fs, fs_len);
#if SIT_VK_SHADER_CACHE_PHASE2
        if (ctx->build_ticket && ctx->build_ticket_leader)
            atomic_store(&ctx->build_ticket->phase, 2);
#endif
    }
#endif

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* [Shader Cache] Layer C pre-check before full pipeline build. */
    if (ctx->layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        uint64_t vs_hash = _SitVkHashBytes64(vs, vs_len);
        uint64_t fs_hash = _SitVkHashBytes64(fs, fs_len);
        _SitVkShaderCacheKey cache_key;
        _SitVkFillCacheKey(&cache_key, vs_hash, fs_hash, (uint8_t)ctx->layout_profile);
        _SitVkShaderCache* c = &sit_render.vk.shader_cache;
        uint64_t ck = vs_hash ^ (fs_hash << 1) ^ cache_key.layout_profile;

        mtx_lock(&c->mutex);
        _SitVkPipelineBundle* hit_bundle = _SitVkShaderCacheFindAndRefBundleLocked(
            c, &cache_key, ck, (uint32_t)sit_render.vk.current_frame_index);
        mtx_unlock(&c->mutex);

        if (hit_bundle) {
            _SitVkAttachBundleRef(slot, hit_bundle);
#if SIT_VK_SHADER_CACHE_PHASE2
            if (ctx->build_ticket) {
                _SitVkReleaseBuildTicket(c, ctx->build_ticket);
                ctx->build_ticket = NULL;
            }
#endif
            _SituationVulkanFreeAsyncShaderLoad(slot);
            return SITUATION_SUCCESS;
        }
    }
#endif /* SIT_VK_SHADER_CACHE_ENABLE */

#if SIT_VK_SHADER_CACHE_PHASE2
    if (ctx->layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        _SitVkTryAttachBundle(slot, vs, vs_len, fs, fs_len, ctx->layout_profile);
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            if (ctx->build_ticket) {
                if (ctx->build_ticket_leader)
                    atomic_store(&ctx->build_ticket->phase, 3);
                _SitVkReleaseBuildTicket(&sit_render.vk.shader_cache, ctx->build_ticket);
                ctx->build_ticket = NULL;
            }
            _SituationVulkanFreeAsyncShaderLoad(slot);
            return SITUATION_SUCCESS;
        }
    }
#endif

    /* Phase 6A/6E: shared legacy fallback — only when bundle-only attach failed. */
    SituationError err = _SituationVulkanBuildGraphicsPipelinesOnSlot(
        slot, vs, vs_len, fs, fs_len, ctx->layout_profile);
#if SIT_VK_SHADER_CACHE_PHASE2
    if (ctx->layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH && ctx->build_ticket) {
        if (err == SITUATION_SUCCESS) {
            if (ctx->build_ticket_leader)
                atomic_store(&ctx->build_ticket->phase, 3);
            _SitVkReleaseBuildTicket(&sit_render.vk.shader_cache, ctx->build_ticket);
            ctx->build_ticket = NULL;
        } else if (ctx->build_ticket_leader) {
            atomic_store(&ctx->build_ticket->phase, 0);
        }
    }
#endif
    _SituationVulkanFreeAsyncShaderLoad(slot);
    if (err != SITUATION_SUCCESS) {
        return _SituationSetErrorFromCode(err, "Async Vulkan pipeline creation failed.");
    }
    return SITUATION_SUCCESS;
}
#endif /* SITUATION_ENABLE_SHADER_COMPILER */

static SituationError _SituationVulkanLoadShaderFromSpirvMemoryWithProfile(
    const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len,
    SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_spirv || !fs_spirv) return SITUATION_ERROR_INVALID_PARAM;
    if (vs_len == 0 || fs_len == 0) return SITUATION_ERROR_INVALID_PARAM;
    if ((vs_len & 3u) != 0 || (fs_len & 3u) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, "SPIR-V bytecode size must be a multiple of 4.");
        return SITUATION_ERROR_VULKAN_SPIRV_INVALID;
    }

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* [Shader Cache] Layer C pre-check; skip Layer A (no source to hash). */
    if (layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        uint64_t vs_hash = _SitVkHashBytes64(vs_spirv, vs_len);
        uint64_t fs_hash = _SitVkHashBytes64(fs_spirv, fs_len);
        _SitVkShaderCacheKey cache_key;
        _SitVkFillCacheKey(&cache_key, vs_hash, fs_hash, (uint8_t)layout_profile);
        _SitVkShaderCache* c = &sit_render.vk.shader_cache;
        uint64_t ck = vs_hash ^ (fs_hash << 1) ^ cache_key.layout_profile;

        mtx_lock(&c->mutex);
        _SitVkPipelineBundle* hit_bundle = _SitVkShaderCacheFindAndRefBundleLocked(
            c, &cache_key, ck, (uint32_t)sit_render.vk.current_frame_index);
        mtx_unlock(&c->mutex);

        if (hit_bundle) {
            _SitVkAttachBundleRef(slot, hit_bundle);
            *out_shader = handle;
            return SITUATION_SUCCESS;
        }
    }
#endif /* SIT_VK_SHADER_CACHE_ENABLE */

#if SIT_VK_SHADER_CACHE_PHASE2
    if (layout_profile == SIT_SPIRV_LAYOUT_PROFILE_MESH) {
        _SitVkTryAttachBundle(slot, vs_spirv, vs_len, fs_spirv, fs_len, layout_profile);
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            *out_shader = handle;
            return SITUATION_SUCCESS;
        }
    }
#endif

    SituationError err = _SituationVulkanBuildGraphicsPipelinesOnSlot(
        slot, vs_spirv, vs_len, fs_spirv, fs_len, layout_profile);
    if (err != SITUATION_SUCCESS) {
        _SitFreeShaderSlot(handle);
        return err;
    }

    /* [Shader Cache Phase 1] Miss: insert bundle for repeat SPIR-V loads. */
    _SitVkTryAttachBundle(slot, vs_spirv, vs_len, fs_spirv, fs_len, layout_profile);

    *out_shader = handle;
    return SITUATION_SUCCESS;
}

/**
 * @brief Load a graphics pipeline from in-memory SPIR-V (vertex + fragment), **Vulkan** backend.
 * @details Same triple-pipeline contract as `SituationLoadShaderFromMemory` / disk `SituationLoadShaderFromSpirv` (PBR, legacy, simple vertex layouts; set 0 = dynamic UBO, set 1 = sampler; 128-byte push constants). SPIR-V must be **Vulkan**-target bytecode. Caller buffers need only remain valid until this function returns. Does not record file paths (no hot-reload).
 */
SITAPI SituationError SituationLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader) {
    return _SituationVulkanLoadShaderFromSpirvMemoryWithProfile(
        vs_spirv, vs_len, fs_spirv, fs_len, SIT_SPIRV_LAYOUT_PROFILE_MESH, out_shader);
}

SITAPI SituationError SituationLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader) {
    return _SituationVulkanLoadShaderFromSpirvMemoryWithProfile(
        vs_spirv, vs_len, fs_spirv, fs_len, layout_profile, out_shader);
}

/**
 * @brief Load a graphics pipeline from precompiled SPIR-V files (`.spv`).
 * @details **Vulkan:** builds the same triple-pipeline layout as `SituationLoadShaderFromMemory` (PBR, legacy, position-only vertex formats; set 0 = dynamic UBO, set 1 = sampler; 128-byte push constants). SPIR-V must match that resource interface and be compiled for **Vulkan** (same family as `SituationLoadShaderFromMemory` / shaderc `shaderc_target_env_vulkan`, e.g. `glslc --target-env=vulkan1.3`). OpenGL-target `.spv` (`--target-env=opengl`, Demon Hunt skydome) is **not** valid here. Does **not** require `SITUATION_ENABLE_SHADER_COMPILER` — only disk I/O and `vkCreateShaderModule` / `vkCreateGraphicsPipelines`.
 */
SITAPI SituationError SituationLoadShaderFromSpirv(const char* vs_spv_path, const char* fs_spv_path, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_spv_path || !fs_spv_path) return SITUATION_ERROR_INVALID_PARAM;

    size_t vs_size = 0;
    size_t fs_size = 0;
    char* vs_data = _SituationReadSpirvFile(vs_spv_path, &vs_size);
    char* fs_data = _SituationReadSpirvFile(fs_spv_path, &fs_size);
    if (!vs_data || !fs_data) {
        if (vs_data) SIT_FREE(vs_data);
        if (fs_data) SIT_FREE(fs_data);
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationLoadShaderFromSpirvMemory(vs_data, vs_size, fs_data, fs_size, out_shader);
    SIT_FREE(vs_data);
    SIT_FREE(fs_data);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    _SituationShaderSlot* slot = _SitGetShaderSlot(*out_shader);
    if (slot) {
        slot->vs_path = _sit_strdup(vs_spv_path);
        slot->fs_path = _sit_strdup(fs_spv_path);
        slot->vs_mod_time = SituationGetFileModTime(vs_spv_path);
        slot->fs_mod_time = SituationGetFileModTime(fs_spv_path);
    }
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_VULKAN)
static VkShaderModule _SituationCreateVulkanShaderModule(const char* code, size_t code_size) {
    if (!code || code_size == 0) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_VULKAN_SPIRV_INVALID,
            "SPIR-V code pointer is NULL or size is zero.");
        return VK_NULL_HANDLE;
    }
    if ((code_size & 3u) != 0) {
        char detail[160];
        snprintf(detail, sizeof(detail),
                 "SPIR-V size %zu is not a multiple of 4 bytes", code_size);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, detail);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = (const uint32_t*)code;

    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(
        sit_render.vk.device, &create_info, NULL, &shader_module);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(
            error_detail, sizeof(error_detail),
            "shader SPIR-V (%zu bytes): vkCreateShaderModule VkResult 0x%x",
            code_size, (unsigned)result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED, error_detail);
        return VK_NULL_HANDLE;
    }

    return shader_module;
}

/**
 * @brief [INTERNAL] Creates a complete Vulkan graphics pipeline from pre-compiled SPIR-V shader files.
 * @details This is a core, high-level helper function for the Vulkan backend that orchestrates the entire process of creating a `VkPipeline`. It is a simplified entry point that assumes a standard vertex format and rendering state suitable for typical 3D rendering.
 *
 * @par Creation Process
 *   1.  **Load SPIR-V:** It reads the raw SPIR-V bytecode from the vertex and fragment shader files specified by `vs_path` and `fs_path`.
 *   2.  **Create Shader Modules:** It creates `VkShaderModule` objects from the loaded bytecode.
 *   3.  **Define Pipeline State:** It configures all the necessary creation info structs for a standard graphics pipeline, including:
 *       - `VkPipelineShaderStageCreateInfo`: Defines the vertex and fragment shader stages.
 *       - `VkPipelineVertexInputStateCreateInfo`: Defines the vertex data layout (this is currently hardcoded and assumes no specific vertex input, which is a simplification).
 *       - `VkPipelineInputAssemblyStateCreateInfo`: Sets the primitive topology to triangle lists.
 *       - `VkPipelineViewportStateCreateInfo`: Configures the viewport and scissor to be dynamic states.
 *       - `VkPipelineRasterizationStateCreateInfo`: Sets standard rasterization state (e.g., fill mode, back-face culling).
 *       - `VkPipelineMultisampleStateCreateInfo`: Disables multisampling.
 *       - `VkPipelineDepthStencilStateCreateInfo`: Enables depth testing and writing.
 *       - `VkPipelineColorBlendStateCreateInfo`: Configures standard alpha blending.
 *   4.  **Create Pipeline Layout:** It currently reuses a pre-existing, simple pipeline layout. A more advanced implementation would create or select a layout compatible with the specific shader's resource requirements.
 *   5.  **Create Graphics Pipeline:** It assembles all the state information into a `VkGraphicsPipelineCreateInfo` struct and calls `vkCreateGraphicsPipelines` to create the final `VkPipeline` object.
 *   6.  **Cleanup:** The temporary `VkShaderModule` objects are destroyed after the pipeline is successfully created.
 *
 * @param vs_path The file system path to the compiled vertex shader SPIR-V file (`.spv`).
 * @param fs_path The file system path to the compiled fragment shader SPIR-V file (`.spv`).
 *
 * @return A `SituationShader` handle populated with the created `vk_pipeline` and `vk_pipeline_layout`.
 * @return A zeroed (invalid) `SituationShader` handle if any step in the process fails (e.g., file not found, shader module creation fails, pipeline creation fails). A detailed error is set internally.
 *
 * @note This function is a simplified helper and makes several assumptions about the rendering state. For example, the vertex input state is hardcoded. A production-ready engine would have a more flexible system for defining pipeline state objects based on material or mesh properties.
 * @warning This function is for internal use only.
 *
 * @see SituationLoadShader(), _SituationReadSpirvFile(), _SituationVulkanCreateShaderModule()
 */
static SituationShader _SituationCreateVulkanPipeline(const char* vs_path, const char* fs_path) {
    SituationShader shader = {0};

    // 1. Load SPIR-V Bytecode
    size_t vs_size, fs_size;
    char* vs_code = _SituationReadSpirvFile(vs_path, &vs_size);
    char* fs_code = _SituationReadSpirvFile(fs_path, &fs_size);
    if (!vs_code || !fs_code) {
        if(vs_code) SIT_FREE(vs_code);
        if(fs_code) SIT_FREE(fs_code);
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_ACCESS, "Failed to read SPIR-V shader files");
        return shader;
    }

    // 2. Create Shader Modules
    VkShaderModule vs_module = _SituationCreateVulkanShaderModule(vs_code, vs_size);
    VkShaderModule fs_module = _SituationCreateVulkanShaderModule(fs_code, fs_size);
    SIT_FREE(vs_code);
    SIT_FREE(fs_code);

    if (vs_module == VK_NULL_HANDLE || fs_module == VK_NULL_HANDLE) {
        if(vs_module) vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        if(fs_module) vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create shader modules");
        return shader;
    }

    // 3. Define Shader Stages
    VkPipelineShaderStageCreateInfo vs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo fs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_stage_info, fs_stage_info};

    // 4. Define Vertex Input, Assembly, Viewport, Rasterization, etc.
    // This part is complex and depends heavily on the mesh format and desired state.
    // This is a simplified example for a standard 3D mesh.
    VkPipelineVertexInputStateCreateInfo vertex_input_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO }; // Assumes vertex data is bound elsewhere
    VkPipelineInputAssemblyStateCreateInfo input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE };
    VkPipelineViewportStateCreateInfo viewport_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling for user shaders
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE };
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &color_blend_attachment };
    VkDynamicState dynamic_states[] = { 
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 6, .pDynamicStates = dynamic_states };

    // 5. Create Pipeline Layout
    // This defines what uniforms/push constants the pipeline will use. A real engine has a complex system for this.
    // For now, we assume a simple, empty layout created during init.
    // VkPipelineLayoutCreateInfo pipeline_layout_info = ...
    // vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &shader.vk_pipeline_layout);
    // NOTE: This function is deprecated and incomplete - it tries to access fields that don't exist on the handle
    // shader.vk_pipeline_layout = sit_render.vk.quad_pipeline_layout; // REUSING A PRE-CREATED ONE for simplicity
    VkPipelineLayout vk_pipeline_layout = sit_render.vk.quad_pipeline_layout; // REUSING A PRE-CREATED ONE for simplicity

    // 6. Create the Graphics Pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = vk_pipeline_layout;
    pipeline_info.renderPass = sit_render.vk.main_window_render_pass; // This pipeline is compatible with the main render pass
    pipeline_info.subpass = 0;

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(sit_render.vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vk_pipeline);
    if (result != VK_SUCCESS) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "vkCreateGraphicsPipelines failed: VkResult = %d", (int)result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, err_msg);

        // Clean up shader modules (existing)
        if (vs_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        }
        if (fs_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);
        }
        // Clean up pipeline layout if it was created (NEW FIX)
        // NOTE: We're reusing a pre-created layout, so we don't destroy it here
        // if (vk_pipeline_layout != VK_NULL_HANDLE) {
        //     vkDestroyPipelineLayout(sit_render.vk.device, vk_pipeline_layout, NULL);
        // }
        // // // if (error_code) *error_code = SITUATION_ERROR_VULKAN_PIPELINE_FAILED;  // TODO: Fix error_code parameter  // TODO: Fix error_code parameter  // TODO: Fix error_code parameter

        return (SituationShader){0}; // Return invalid shader
    }
    
    // NOTE: This function is incomplete and doesn't properly store the pipeline in a slot
    // Use SituationLoadShader() or SituationLoadShaderFromMemory() instead
    return (SituationShader){0};
}
#endif /* SITUATION_USE_VULKAN */

static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob);

/**
 * @brief [INTERNAL] Callback for resolving `#include` directives in GLSL shaders.
 *
 * @details This function is invoked by the `shaderc` compiler whenever it encounters an `#include` statement
 *          in the shader source code. It attempts to load the requested file content from disk.
 *
 *          This enables the creation of modular "Uber Shaders" where common logic (math utilities,
 *          struct definitions) is stored in shared `.glslh` header files.
 *
 * @param user_data Optional user context (unused).
 * @param requested_source The path string inside the include directive (e.g., "common/math.glslh").
 * @param type The type of include (Standard vs Relative). Currently treated identically.
 * @param requesting_source The path of the file that contains the include directive (for relative path resolution).
 * @param include_depth The current nesting depth of includes (for recursion limits).
 *
 * @return A pointer to a `shaderc_include_result` struct containing the loaded source code or error info.
 *         The compiler will later pass this pointer to `_SituationShaderIncluderRelease`.
 */
static char* _SituationShaderIncluderTryLoadPath(const char* path, char** out_resolved_path) {
    if (!path || !path[0]) {
        return NULL;
    }
    char* text = SituationLoadFileText(path);
    if (text && out_resolved_path) {
        *out_resolved_path = _sit_strdup(path);
    }
    return text;
}

/** Mirrors `_SituationLoadCoreShaderFile` search order plus parent-dir relative includes. */
static char* _SituationShaderIncluderLoadFile(
    const char* requested_source,
    const char* requesting_source,
    char** out_resolved_path)
{
    if (!requested_source || !requested_source[0]) {
        return NULL;
    }
    if (out_resolved_path) {
        *out_resolved_path = NULL;
    }

    char* text = _SituationShaderIncluderTryLoadPath(requested_source, out_resolved_path);
    if (text) {
        return text;
    }

    if (requesting_source && requesting_source[0]) {
        const char* slash = strrchr(requesting_source, '/');
        const char* bslash = strrchr(requesting_source, '\\');
        const char* sep = slash;
        if (bslash && (!slash || bslash > slash)) {
            sep = bslash;
        }
        if (sep && sep > requesting_source) {
            char relative[512];
            size_t dir_len = (size_t)(sep - requesting_source);
            int n = snprintf(relative, sizeof(relative), "%.*s/%s",
                (int)dir_len, requesting_source, requested_source);
            if (n > 0 && (size_t)n < sizeof(relative)) {
                text = _SituationShaderIncluderTryLoadPath(relative, out_resolved_path);
                if (text) {
                    return text;
                }
            }
        }
    }

    const char* prefixes[] = { "../", "../../", "../../../", "../../../../" };
    char candidate[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        int n = snprintf(candidate, sizeof(candidate), "%s%s", prefixes[i], requested_source);
        if (n <= 0 || (size_t)n >= sizeof(candidate)) {
            continue;
        }
        text = _SituationShaderIncluderTryLoadPath(candidate, out_resolved_path);
        if (text) {
            return text;
        }
    }

    char* exe_base = SituationGetBasePath();
    if (exe_base) {
        char* joined = SituationJoinPath(exe_base, requested_source);
        if (joined) {
            text = SituationLoadFileText(joined);
            if (text) {
                if (out_resolved_path) {
                    *out_resolved_path = joined;
                } else {
                    SIT_FREE(joined);
                }
                SIT_FREE(exe_base);
                return text;
            }
            SIT_FREE(joined);
        }
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
            int n = snprintf(candidate, sizeof(candidate), "%s%s", prefixes[i], requested_source);
            if (n <= 0 || (size_t)n >= sizeof(candidate)) {
                continue;
            }
            joined = SituationJoinPath(exe_base, candidate);
            if (!joined) {
                continue;
            }
            text = SituationLoadFileText(joined);
            if (text) {
                if (out_resolved_path) {
                    *out_resolved_path = joined;
                } else {
                    SIT_FREE(joined);
                }
                SIT_FREE(exe_base);
                return text;
            }
            SIT_FREE(joined);
        }
        SIT_FREE(exe_base);
    }

    return NULL;
}

static shaderc_include_result* _SituationShaderIncluderResolve(
    void* user_data,
    const char* requested_source,
    int type,
    const char* requesting_source,
    size_t include_depth)
{
    (void)user_data; (void)type; (void)include_depth;

    _SitIncludeResult* container = (_SitIncludeResult*)SIT_CALLOC(1, sizeof(_SitIncludeResult));

    container->content = _SituationShaderIncluderLoadFile(
        requested_source, requesting_source, &container->full_path);

    if (container->content) {
        container->result.content = container->content;
        container->result.content_length = strlen(container->content);
        container->result.source_name = container->full_path;
        container->result.source_name_length = strlen(container->full_path);
    } else {
        // Error: Provide an error message as the content
        const char* err_msg = "Could not open included file.";
        container->result.content = err_msg;
        container->result.content_length = strlen(err_msg);
        container->result.source_name = "";
        container->result.source_name_length = 0;
        // Empty path signals failure to shaderc? No, usually content is error msg.
        // But standard behavior is usually just failing to load.
    }

    return &container->result;
}

/**
 * @brief [INTERNAL] Callback for freeing memory allocated during shader inclusion.
 *
 * @details This function is called by `shaderc` once it has finished processing an included file.
 *          It is responsible for freeing the `content` buffer (loaded from disk) and the
 *          `shaderc_include_result` container structure itself.
 *
 * @param user_data Optional user context (unused).
 * @param include_result The pointer returned by `_SituationShaderIncluderResolve`.
 */
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result) {
    (void)user_data;
    _SitIncludeResult* container = (_SitIncludeResult*)include_result;
    if (container) {
        if (container->content && container->content != container->result.content) {
             // Handle error message case if strictly needed, but usually we just free content
        }
        // If content was loaded via SituationLoadFileText (SIT_MALLOC), free it.
        // If it was a static error string, we shouldn't free it.
        // Simpler logic:
        if (container->result.source_name_length > 0) { // Was successful load
             SIT_FREE(container->content);
        }
        SIT_FREE(container->full_path);
        SIT_FREE(container);
    }
}

/**
 * @brief [INTERNAL] Compiles a GLSL source string into a SPIR-V binary blob using shaderc.
 *
 * @details This is the first and most crucial stage of the unified shader pipeline. It takes standard, human-readable GLSL code and transforms it into the SPIR-V intermediate representation.
 *          This SPIR-V bytecode can then be consumed by both the Vulkan backend and the OpenGL backend (if `GL_ARB_gl_spirv` is supported), ensuring shader consistency across different graphics APIs and potentially improving load times.
 *
 * @param glsl_source A null-terminated C-string containing the GLSL shader code to be compiled.
 * @param source_name A descriptive name for the shader (e.g., "scene.vert", "compute_filter.comp").
 *                    This name is used in error messages generated by shaderc to help identify the problematic shader.
 * @param shader_kind The type of shader being compiled (e.g., `shaderc_vertex_shader`, `shaderc_fragment_shader`, `shaderc_compute_shader`). This tells shaderc which specific compilation rules and validation checks to apply.
 *
 * @return A `_SituationSpirvBlob` struct.
 *         - On **success**, the struct is populated:
 *           - `internal_result` points to a valid `shaderc_compilation_result_t` object containing the compiled SPIR-V data.
 *           - `data` points to the raw SPIR-V bytecode within the `internal_result`.
 *           - `size` is the size of the SPIR-V bytecode in bytes.
 *         - On **failure**, the struct is zero-initialized (`{0}`), and the library's global error state (`sit_gs.last_error_msg`, `sit_gs.last_error_code`) is updated with a specific error code and a descriptive message (either from shaderc or an internal error).
 *
 * @note This function requires the `SITUATION_ENABLE_SHADER_COMPILER` define to be set, as it directly depends on the `shaderc` library.
 * @warning The returned `_SituationSpirvBlob` contains an `internal_result` object (`shaderc_compilation_result_t*`) that **must** be freed later by calling `_SituationFreeSpirvBlob` to prevent memory leaks within the shaderc library.
 *          Failing to do so will result in leaked resources.
 * @warning The pointers `blob.data` and `blob.size` are only valid as long as the `blob.internal_result` object exists and has not been released.
 *
 * @see _SituationFreeSpirvBlob(), _SituationCreateVulkanShaderModule(),
 *      SituationCreateComputePipelineFromMemory()
 */
static _SituationSpirvBlob _SituationShadercCompileGLSLtoSPIRVWithMacros(
    const char* glsl_source,
    const char* source_name,
    shaderc_shader_kind shader_kind,
    const _SituationShadercMacro* macros,
    size_t macro_count,
    shaderc_target_env target_env,
    uint32_t target_env_version,
    bool gl_auto_bindings)
{
    // --- 1. Input Validation ---
    // Perform basic checks on input parameters to prevent crashes or undefined behavior
    // within this function or the shaderc library.
    _SituationSpirvBlob blob = {0}; // Initialize return struct to zero/NULL

    if (!glsl_source) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCompileGLSLtoSPIRV: GLSL source code pointer is NULL." );
        return blob; // Return zero-initialized struct
    }
    // Check for empty string (strlen(glsl_source) == 0) could also be done,
    // but shaderc might handle it gracefully or provide its own error.
    // Let's assume non-NULL is sufficient for now.

    if (!source_name) {
        // Provide a default name for error reporting if none is given.
        // This prevents a potential crash if shaderc internally uses source_name.
        source_name = "<unnamed_shader>";
        // Alternatively, return an error:
        // _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCompileGLSLtoSPIRV: source_name is NULL.");
        // return blob;
    }

    // --- 2. Initialize shaderc Compiler and Options ---
    // These are the core objects needed to configure and perform the compilation.
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compile_options_t options = shaderc_compile_options_initialize();

    // Enable #include support
    shaderc_compile_options_set_include_callbacks(
        options,
        _SituationShaderIncluderResolve,
        _SituationShaderIncluderRelease,
        NULL // user_data
    );

    // Check if initialization was successful.
    if (!compiler) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Failed to initialize shaderc compiler." );
        // Clean up any potentially partially initialized object.
        if (options) {
            shaderc_compile_options_release(options);
        }
        return blob; // Return zero-initialized struct
    }
    if (!options) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Failed to initialize shaderc compile options." );
        // Clean up the successfully initialized compiler.
        shaderc_compiler_release(compiler);
        return blob; // Return zero-initialized struct
    }

    // --- 3. Configure Compilation Options ---
    shaderc_compile_options_set_target_env(options, target_env, target_env_version);
    if (gl_auto_bindings) {
        shaderc_compile_options_set_auto_bind_uniforms(options, true);
        shaderc_compile_options_set_auto_map_locations(options, true);
    }

    if (macros) {
        for (size_t i = 0; i < macro_count; ++i) {
            if (!macros[i].name) continue;
            const char* val = macros[i].value;
            size_t val_len = 0;
            if (val && val[0]) {
                val_len = strlen(val);
            } else {
                val = NULL;
            }
            shaderc_compile_options_add_macro_definition(
                options, macros[i].name, strlen(macros[i].name), val, val_len);
        }
    }

    // Enable performance optimizations for the generated SPIR-V.
    // This can reduce shader size and potentially improve runtime performance.
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);

    // In debug builds (when NDEBUG is NOT defined), generate additional debug information.
    // This information is useful for debugging tools like RenderDoc or Nsight Graphics.
#ifndef NDEBUG
    shaderc_compile_options_set_generate_debug_info(options);
#endif

    // --- 4. Perform the Compilation ---
    // This is the core operation where shaderc processes the GLSL source.
    // strlen(glsl_source) is used to determine the length of the input string.
    // shaderc makes a copy of the source internally, so the input string can be freed
    // after this call returns.
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] About to call shaderc_compile_into_spv for '%s' (%zu bytes)\n", source_name, strlen(glsl_source)); fflush(stderr);
#endif
    blob.internal_result = shaderc_compile_into_spv(
        compiler,               // The initialized shaderc compiler instance
        glsl_source,            // The GLSL source code string
        strlen(glsl_source),   // The length of the GLSL source code
        shader_kind,            // The type of shader (vertex, fragment, compute, etc.)
        source_name,            // The name for error reporting
        "main",                 // The entry point function name within the shader
        options                 // The configured compilation options
    );
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] shaderc_compile_into_spv returned, checking status...\n"); fflush(stderr);
#endif
    if (blob.internal_result) {
        shaderc_compilation_status status = shaderc_result_get_compilation_status(blob.internal_result);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Situation] Compilation status: %d\n", (int)status);
#endif
        if (status != shaderc_compilation_status_success) {
            const char* err = shaderc_result_get_error_message(blob.internal_result);
            fprintf(stderr, "[Situation] Compilation FAILED: %s\n", err ? err : "<no message>");
        } else {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
            fprintf(stderr, "[Situation] Compilation SUCCESS\n");
#endif
        }
        fflush(stderr);
    } else {
        fprintf(stderr, "[Situation] ERROR: blob.internal_result is NULL!\n"); fflush(stderr);
        _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED, "shaderc_compile_into_spv returned NULL result object");
    }

    // --- 5. Release Temporary Resources ---
    // The compiler and options objects are no longer needed after the compilation call.
    // Release them immediately to free their associated resources.
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);

    // Check if the compilation call itself produced a result object.
    // A NULL internal_result here would indicate a fundamental failure in the
    // shaderc_compile_into_spv call itself, perhaps due to an internal error in shaderc.
    if (!blob.internal_result) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: shaderc_compile_into_spv returned NULL result object." );
        // No need to call shaderc_result_release as internal_result is NULL.
        return (_SituationSpirvBlob){0}; // Explicitly return zeroed struct
    }

    // --- 6. Check Compilation Status ---
    // Even if internal_result is not NULL, the compilation might have failed.
    // shaderc reports the status separately.
    shaderc_compilation_status status = shaderc_result_get_compilation_status(blob.internal_result);

    if (status != shaderc_compilation_status_success) {
        // Compilation failed. Retrieve the detailed error message from shaderc.
        const char* error_message = shaderc_result_get_error_message(blob.internal_result);
        // Determine the most appropriate SituationError code based on the shaderc status.
        SituationError sit_error_code = SITUATION_ERROR_SHADER_COMPILATION_FAILED;
        // Map specific shaderc errors to potentially more descriptive Situation errors if needed.
        // For example:
        // if (status == shaderc_compilation_status_invalid_stage) {
        //     sit_error_code = SITUATION_ERROR_INVALID_PARAM; // Or a new specific error?
        // } else if (status == shaderc_compilation_status_compilation_error) {
        //     sit_error_code = SITUATION_ERROR_SHADER_COMPILATION_FAILED; // Default
        // }
        // For now, use the general compilation failed error.

        char full_error_msg[512]; // Buffer for a more detailed error message
        snprintf(
            full_error_msg,
            sizeof(full_error_msg),
            "_SituationVulkanCompileGLSLtoSPIRV: Shader compilation failed for '%s' (Status: %d). Details: %s",
            source_name,
            (int)status,
            (error_message && strlen(error_message) > 0) ? error_message : "<no details from shaderc>"
        );

        _SituationSetErrorFromCode(sit_error_code, full_error_msg);

        // Clean up the shaderc result object associated with the failed compilation.
        shaderc_result_release(blob.internal_result);
        blob.internal_result = NULL; // Defensive nulling
        // Return a zero-initialized struct to indicate failure.
        return (_SituationSpirvBlob){0};
    }

    // --- 7. Extract Compiled Data (On Success) ---
    // If we reach here, the compilation was successful.
    // Extract the pointer to the compiled SPIR-V bytecode and its size from the result object.
    blob.data = (const uint8_t*)shaderc_result_get_bytes(blob.internal_result);
    blob.size = shaderc_result_get_length(blob.internal_result);

    // Perform a final sanity check: ensure data is not NULL and size is not zero for a successful compilation.
    // While unlikely if status is success, it's a good defensive measure.
    if (!blob.data || blob.size == 0) {
         // This is unexpected for a successful compilation.
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Successful compilation yielded NULL data or zero size." );
        // Clean up the result object.
        shaderc_result_release(blob.internal_result);
        blob.internal_result = NULL;
        return (_SituationSpirvBlob){0};
    }

    // --- 8. Return Successful Result ---
    // The blob struct is now fully populated with the successful compilation result.
    // The caller is responsible for calling _SituationFreeSpirvBlob later.
    return blob;
}

static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRVWithMacros(
    const char* glsl_source,
    const char* source_name,
    shaderc_shader_kind shader_kind,
    const _SituationShadercMacro* macros,
    size_t macro_count)
{
    return _SituationShadercCompileGLSLtoSPIRVWithMacros(
        glsl_source, source_name, shader_kind, macros, macro_count,
        shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1, false);
}

static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(
    const char* glsl_source,
    const char* source_name,
    shaderc_shader_kind shader_kind)
{
    return _SituationVulkanCompileGLSLtoSPIRVWithMacros(glsl_source, source_name, shader_kind, NULL, 0);
}

static _SituationSpirvBlob _SituationVulkanCompileCoreShaderFile(
    const char* relative_path,
    const char* source_name,
    shaderc_shader_kind shader_kind,
    const _SituationShadercMacro* extra_macros,
    size_t extra_macro_count)
{
    char* src = NULL;
    _SituationSpirvBlob blob = {0};

    if (_SituationLoadCoreShaderFile(relative_path, &src) != SITUATION_SUCCESS) {
        return blob;
    }

    _SituationShadercMacro macros[8];
    size_t macro_count = 0;
    macros[macro_count++] = (_SituationShadercMacro){ "SITUATION_USE_VULKAN", "1" };
    if (extra_macros) {
        for (size_t i = 0; i < extra_macro_count && macro_count < sizeof(macros) / sizeof(macros[0]); ++i) {
            macros[macro_count++] = extra_macros[i];
        }
    }

    blob = _SituationVulkanCompileGLSLtoSPIRVWithMacros(src, source_name, shader_kind, macros, macro_count);
    SIT_FREE(src);
    return blob;
}

/**
 * @brief [INTERNAL] Releases the memory held by a shaderc compilation result object.
 *
 * @details This helper function is responsible for cleaning up the resources associated with a `_SituationSpirvBlob`, specifically the `shaderc_compilation_result_t` object stored in its `internal_result` member.
 *          This object holds the compiled SPIR-V bytecode and any associated metadata (like error messages) generated by the shaderc library.
 *
 * @param blob A pointer to the `_SituationSpirvBlob` struct whose `internal_result` member should be freed.
 *
 * @note It is safe to call this function on a `_SituationSpirvBlob` that has not been successfully initialized by `_SituationVulkanCompileGLSLtoSPIRV` (e.g., if `internal_result` is `NULL`). In such cases, the function will simply do nothing.
 * @note This function only frees the `internal_result`. It does not modify the `data` or `size` members of the `blob` struct itself. After calling this function, the `blob` struct should be considered invalid or re-initialized before reuse.
 * @warning The `blob` pointer itself is not freed by this function. If the `_SituationSpirvBlob` struct was allocated on the heap, the caller is still responsible for freeing that memory.
 *
 * @see _SituationVulkanCompileGLSLtoSPIRV(), _SituationSpirvBlob
 */
static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob) {
    // --- 1. Input Validation ---
    // Check if the blob pointer itself is valid.
    if (!blob) {
        // Calling free on a NULL pointer is safe, but explicitly checking
        // prevents potential misuse and can aid debugging.
        return; // Silently return, consistent with freeing NULL pointers.
    }

    // --- 2. Release Shaderc Resource ---
    // Check if the internal shaderc result object exists before attempting to release it.
    if (blob->internal_result) {
        // This is the actual call to the shaderc library to release the compilation result.
        shaderc_result_release(blob->internal_result);
        // Set the pointer to NULL to indicate it's no longer valid.
        // This prevents accidental double-free if the function were called again.
        blob->internal_result = NULL;
    }

    // Note: The `data` and `size` members of the `_SituationSpirvBlob` struct
    // are not modified here. They point to memory managed by the
    // `shaderc_compilation_result_t` object, which is freed by `shaderc_result_release`.
    // After this call, `blob->data` should be considered a dangling pointer
    // relative to its original source and should not be used.
}
#endif

#if defined(SITUATION_USE_OPENGL)

/**
 * @brief [INTERNAL] Compiles a single GLSL shader stage from a source string.
 * @details This is a low-level helper function that takes a string of GLSL code and a shader type (e.g., vertex, fragment, compute) and uses the OpenGL driver to compile it into a shader object. It performs comprehensive error checking and reporting.
 *
 * @par Compilation Process
 *   1.  Creates a new shader object of the specified `type` (`glCreateShader`).
 *   2.  Associates the `source` string with the shader object (`glShaderSource`).
 *   3.  Attempts to compile the shader (`glCompileShader`).
 *   4.  Checks the `GL_COMPILE_STATUS`. If compilation fails, it retrieves the detailed error log from the driver.
 *
 * The retrieved error log is formatted with a prefix indicating the shader type and is set as the library's last error message, providing invaluable feedback for debugging shader code.
 *
 * @param source A null-terminated C string containing the GLSL source code to compile.
 * @param type The type of shader to create (e.g., `GL_VERTEX_SHADER`, `GL_FRAGMENT_SHADER`, `GL_COMPUTE_SHADER`).
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The `GLuint` ID of the compiled shader object on success.
 * @return `0` on failure. On failure, a detailed error message is set, and the invalid shader object is deleted.
 *
 * @note This function is for internal use by the higher-level shader program creation functions.
 * @warning The returned shader object is an intermediate resource. It should be attached to a program and then deleted with `glDeleteShader` to prevent resource leaks.
 *
 * @see _SituationCreateGLShaderProgram(), _SituationCreateGLShaderProgramFromSource()
 */
/** Injection point for virtual-bindless fallback: after the last #extension in the file, or after #version + leading #directives. */
static const char* _SituationGLSLVirtualBindlessInjectionPoint(const char* source) {
    if (!source) return NULL;

    const char* after_last_extension = NULL;
    for (const char* scan = source; (scan = strstr(scan, "#extension")) != NULL; ) {
        const char* line_end = strchr(scan, '\n');
        if (!line_end) {
            after_last_extension = source + strlen(source);
            break;
        }
        after_last_extension = line_end + 1;
        scan = after_last_extension;
    }
    if (after_last_extension) {
        return after_last_extension;
    }

    const char* version_pos = strstr(source, "#version");
    if (!version_pos) {
        return source;
    }
    const char* p = strchr(version_pos, '\n');
    if (!p) {
        return source + strlen(source);
    }
    p++;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (strncmp(p, "#define", 7) == 0 || strncmp(p, "#if", 3) == 0) {
            const char* line_end = strchr(p, '\n');
            if (!line_end) {
                return source + strlen(source);
            }
            p = line_end + 1;
            continue;
        }
        break;
    }
    return p;
}

static GLuint _SituationCompileGLShaderEx(const char* source, GLenum type, SituationError* error_code, bool wait_for_compile) {
    if (!source) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    GLuint shader = glCreateShader(type);

    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        // [Phase 5] Virtual Bindless Injection — must land after every #extension (incl. #if-nested ones).
        const char* injection_point = _SituationGLSLVirtualBindlessInjectionPoint(source);
        if (injection_point && injection_point > source) {
            const char* injection =
                "#define SITUATION_VIRTUAL_BINDLESS 1\n"
                "uniform sampler2D _sit_virtual_textures[32];\n"
                "uniform int _sit_texture_slot_id;\n"
                "#define global_textures _sit_virtual_textures\n"
                "#define nonuniformEXT(x) _sit_texture_slot_id\n";

            const char* sources[3] = { source, injection, injection_point };
            GLint lengths[3] = {
                (GLint)(injection_point - source),
                (GLint)strlen(injection),
                (GLint)strlen(injection_point)
            };
            glShaderSource(shader, 3, sources, lengths);
        } else {
            glShaderSource(shader, 1, &source, NULL);
        }
    } else {
        glShaderSource(shader, 1, &source, NULL);
    }

    glCompileShader(shader);

    if (!wait_for_compile) {
        if (error_code) *error_code = SITUATION_SUCCESS;
        return shader;
    }

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            // Determine the prefix for the error message.
            const char* type_name = "Unknown Shader";
            if (type == GL_VERTEX_SHADER) type_name = "Vertex Shader";
            else if (type == GL_FRAGMENT_SHADER) type_name = "Fragment Shader";
            else if (type == GL_COMPUTE_SHADER) type_name = "Compute Shader";

            // Calculate the total size needed for the final error string: "PREFIX: LOG\0"
            size_t prefix_len = strlen(type_name) + 2; // For ": "
            size_t total_buffer_size = prefix_len + log_length;

            // Allocate a single buffer for the entire message.
            char* final_error_message = (char*)SIT_MALLOC(total_buffer_size);

            if (final_error_message) {
                // Write the prefix into the buffer.
                strcpy(final_error_message, type_name);
                strcat(final_error_message, ": ");

                // Get a pointer to where the log should start.
                char* log_part = final_error_message + prefix_len;

                // Read the GL info log directly into the end of our buffer.
                glGetShaderInfoLog(shader, log_length, NULL, log_part);

                // Set the final, combined error message.
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, final_error_message);
                
                // Log to debug file
                SIT_DEBUG_LOG("[SHADER_ERROR] %s", final_error_message);

                // Free the single allocated buffer.
                SIT_FREE(final_error_message);
            } else {
                // If allocation fails, set a memory error.
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for shader compilation log.");
            }
        } else {
            // No log available, provide a generic error.
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, "An unknown shader compilation error occurred with no log.");
        }

        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
        glDeleteShader(shader);
        return 0;
    }

    if (error_code) *error_code = SITUATION_SUCCESS;
    return shader;
}

static GLuint _SituationCompileGLShader(const char* source, GLenum type, SituationError* error_code) {
    return _SituationCompileGLShaderEx(source, type, error_code, true);
}

/** Returns 1 when compile finished (success or failure). On failure deletes shader and sets error_code. */
static int _SituationPollGLShaderCompile(GLuint shader, GLenum type, SituationError* error_code) {
    if (!shader) {
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 1;
    }
    GLint completion = GL_FALSE;
    glGetShaderiv(shader, GL_COMPLETION_STATUS_KHR, &completion);
    if (completion != GL_TRUE) {
        return 0;
    }
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) {
        if (error_code) *error_code = SITUATION_SUCCESS;
        return 1;
    }
    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        const char* type_name = (type == GL_VERTEX_SHADER) ? "Vertex Shader" : "Fragment Shader";
        size_t prefix_len = strlen(type_name) + 2;
        size_t total_buffer_size = prefix_len + (size_t)log_length;
        char* final_error_message = (char*)SIT_MALLOC(total_buffer_size);
        if (final_error_message) {
            strcpy(final_error_message, type_name);
            strcat(final_error_message, ": ");
            glGetShaderInfoLog(shader, log_length, NULL, final_error_message + prefix_len);
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, final_error_message);
            SIT_FREE(final_error_message);
        }
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, "Async shader compilation failed");
    }
    if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
    return 1;
}

static void _SituationGLFreeSpirvAsyncCopies(_SituationShaderSlot* slot) {
    if (!slot) return;
    if (slot->gl_spirv_vs_copy) { SIT_FREE(slot->gl_spirv_vs_copy); slot->gl_spirv_vs_copy = NULL; }
    if (slot->gl_spirv_fs_copy) { SIT_FREE(slot->gl_spirv_fs_copy); slot->gl_spirv_fs_copy = NULL; }
    slot->gl_spirv_substage = 0;
}

static SituationError _SituationGLAsyncLoadFail(_SituationShaderSlot* slot) {
    SituationError err = SituationGetLastErrorCode();
    if (err == SITUATION_SUCCESS || err == SITUATION_ERROR_UNKNOWN_ERROR) {
        err = SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED;
    }
    if (!slot) {
        return err;
    }
    if (slot->gl_async_vs_shader) { glDeleteShader(slot->gl_async_vs_shader); slot->gl_async_vs_shader = 0; }
    if (slot->gl_async_fs_shader) { glDeleteShader(slot->gl_async_fs_shader); slot->gl_async_fs_shader = 0; }
    if (slot->gl_pending_program_id) { glDeleteProgram(slot->gl_pending_program_id); slot->gl_pending_program_id = 0; }
    _SituationGLFreeSpirvAsyncCopies(slot);
    slot->gl_spirv_vs_len = 0;
    slot->gl_spirv_fs_len = 0;
    slot->gl_pending_link_spirv = false;
    slot->gl_is_linking = false;
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_IDLE;
    return err;
}

static int _SituationGLSpecializeSpirvShader(
    GLuint shader, GLenum type, size_t blob_bytes, SituationError fail_code, SituationError* out_err) {
    const char* stage =
        (type == GL_VERTEX_SHADER) ? "vertex" :
        (type == GL_FRAGMENT_SHADER) ? "fragment" : "shader";

    glSpecializeShader(shader, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char* infoLog = _SituationDupGLInfoLog(shader, 0);
        SituationError spirv_err = _SituationSetGLErrorFromSpirvStage(
            fail_code, stage, blob_bytes, infoLog ? infoLog : "");
        if (out_err) {
            *out_err = spirv_err;
        }
        if (infoLog) {
            SIT_FREE(infoLog);
        }
        return 0;
    }
    if (out_err) {
        *out_err = SITUATION_SUCCESS;
    }
    return 1;
}

static int _SituationFinalizeGLPendingProgramLink(_SituationShaderSlot* slot) {
    if (!slot || !slot->gl_is_linking || !slot->gl_pending_program_id) {
        return 0;
    }

    GLuint program = slot->gl_pending_program_id;
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char* infoLog = _SituationDupGLInfoLog(program, 1);
        SituationError code = slot->gl_pending_link_spirv
            ? SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED
            : SITUATION_ERROR_OPENGL_SHADER_LINK;
        if (slot->gl_pending_link_spirv) {
            size_t blob_bytes = slot->gl_spirv_vs_len + slot->gl_spirv_fs_len;
            _SituationSetGLErrorFromSpirvStage(
                code, "graphics program", blob_bytes, infoLog ? infoLog : "");
        } else {
            _SituationSetErrorFromCode(code, (infoLog && infoLog[0]) ? infoLog : "(no driver log)");
        }
        if (infoLog) {
            SIT_FREE(infoLog);
        }
        glDeleteProgram(program);
        slot->gl_pending_program_id = 0;
        slot->gl_is_linking = false;
        slot->gl_pending_link_spirv = false;
        slot->gl_spirv_vs_len = 0;
        slot->gl_spirv_fs_len = 0;
        return 0;
    }

    if (slot->gl_pending_link_spirv) {
        _SituationBindGLProgramStorageBlocks(program);
        _SituationBindGLProgramUniformBlocks(program);
        if (slot->uniform_map) {
            _sit_uniform_map_destroy(slot->uniform_map);
            slot->uniform_map = NULL;
        }
    } else {
        if (slot->uniform_map) {
            _sit_uniform_map_destroy(slot->uniform_map);
        }
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) {
            glDeleteProgram(program);
            slot->gl_pending_program_id = 0;
            slot->gl_is_linking = false;
            return 0;
        }
        GLint count = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
        for (GLint j = 0; j < count; j++) {
            char name[256];
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(program, (GLuint)j, sizeof(name), &length, &size, &type, name);
            GLint location = glGetUniformLocation(program, name);
            if (location != -1) {
                SituationError set_err = _sit_uniform_map_set(slot->uniform_map, name, location);
                if (set_err != SITUATION_SUCCESS) {
                    glDeleteProgram(program);
                    slot->gl_pending_program_id = 0;
                    slot->gl_is_linking = false;
                    return 0;
                }
            }
        }
    }

    if (slot->gl_program_id) {
        glDeleteProgram(slot->gl_program_id);
    }
    slot->gl_program_id = program;
    slot->gl_pending_program_id = 0;
    slot->gl_is_linking = false;
    slot->gl_pending_link_spirv = false;
    slot->gl_spirv_vs_len = 0;
    slot->gl_spirv_fs_len = 0;
    return 1;
}

static SituationError _SituationPollGLPendingProgramLink(_SituationShaderSlot* slot) {
    if (!slot || !slot->gl_is_linking || !slot->gl_pending_program_id) {
        return SITUATION_SUCCESS;
    }

    int ready = 0;
    if (sit_render.gl.parallel_shader_compile_available) {
        GLint status = GL_FALSE;
        glGetProgramiv(slot->gl_pending_program_id, GL_COMPLETION_STATUS_KHR, &status);
        ready = (status == GL_TRUE);
        if (!ready) {
            /* Some drivers never set COMPLETION for SPIR-V links; fall back to LINK_STATUS. */
            GLint link_ok = GL_FALSE;
            glGetProgramiv(slot->gl_pending_program_id, GL_LINK_STATUS, &link_ok);
            if (link_ok == GL_TRUE) {
                ready = 1;
            } else {
                GLint log_len = 0;
                glGetProgramiv(slot->gl_pending_program_id, GL_INFO_LOG_LENGTH, &log_len);
                if (log_len > 1) {
                    ready = 1;
                }
            }
        }
    } else {
        ready = 1;
    }

    if (!ready) {
        return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
    }

    if (!_SituationFinalizeGLPendingProgramLink(slot)) {
        SituationError err = SituationGetLastErrorCode();
        if (err == SITUATION_SUCCESS || err == SITUATION_ERROR_UNKNOWN_ERROR) {
            err = slot->gl_pending_link_spirv
                ? SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED
                : SITUATION_ERROR_OPENGL_SHADER_LINK;
        }
        return err;
    }
    return SITUATION_SUCCESS;
}

static SituationError _SituationPollGLAsyncSpirvShaderLoad(_SituationShaderSlot* slot) {
    if (!slot || slot->gl_async_load_stage != SIT_GL_ASYNC_STAGE_SPIRV) {
        return SITUATION_SUCCESS;
    }
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "GL_ARB_gl_spirv required for async SPIR-V load.");
        return _SituationGLAsyncLoadFail(slot);
    }

    SituationError err = SITUATION_SUCCESS;

    if (slot->gl_spirv_substage == 0) {
        if (!_SituationGLSpecializeSpirvShader(
                slot->gl_async_vs_shader, GL_VERTEX_SHADER, slot->gl_spirv_vs_len,
                SITUATION_ERROR_OPENGL_SPIRV_VS_SPECIALIZE_FAILED, &err)) {
            return _SituationGLAsyncLoadFail(slot);
        }
        slot->gl_spirv_substage = 1;
        return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
    }

    if (slot->gl_spirv_substage == 1) {
        if (!_SituationGLSpecializeSpirvShader(
                slot->gl_async_fs_shader, GL_FRAGMENT_SHADER, slot->gl_spirv_fs_len,
                SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED, &err)) {
            return _SituationGLAsyncLoadFail(slot);
        }
        slot->gl_spirv_substage = 2;
        return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
    }

    if (slot->gl_spirv_substage != 2) {
        return SITUATION_SUCCESS;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateProgram failed during SPIR-V async link.");
        return _SituationGLAsyncLoadFail(slot);
    }
    glAttachShader(program, slot->gl_async_vs_shader);
    glAttachShader(program, slot->gl_async_fs_shader);
    glLinkProgram(program);
    glDeleteShader(slot->gl_async_vs_shader);
    glDeleteShader(slot->gl_async_fs_shader);
    slot->gl_async_vs_shader = 0;
    slot->gl_async_fs_shader = 0;
    _SituationGLFreeSpirvAsyncCopies(slot);
    slot->gl_pending_program_id = program;
    slot->gl_pending_link_spirv = true;
    slot->gl_is_linking = true;
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_IDLE;
    return _SituationPollGLPendingProgramLink(slot);
}

static SituationError _SituationBeginGLSpirvShaderLoadAsync(
    const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader) {
    if (!out_shader) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(out_shader, 0, sizeof(SituationShader));

    SituationError v_err = _SituationValidateSpirvBinary(vs_spirv, vs_len, "vertex");
    if (v_err != SITUATION_SUCCESS) {
        return v_err;
    }
    SituationError f_err = _SituationValidateSpirvBinary(fs_spirv, fs_len, "fragment");
    if (f_err != SITUATION_SUCCESS) {
        return f_err;
    }
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(
            SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,
            "GL_ARB_gl_spirv is required for SituationBeginLoadShaderFromSpirvMemory on OpenGL.");
        return SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE;
    }

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    slot->gl_spirv_vs_copy = (uint8_t*)SIT_MALLOC(vs_len);
    slot->gl_spirv_fs_copy = (uint8_t*)SIT_MALLOC(fs_len);
    if (!slot->gl_spirv_vs_copy || !slot->gl_spirv_fs_copy) {
        _SituationGLAsyncLoadFail(slot);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    memcpy(slot->gl_spirv_vs_copy, vs_spirv, vs_len);
    memcpy(slot->gl_spirv_fs_copy, fs_spirv, fs_len);
    slot->gl_spirv_vs_len = vs_len;
    slot->gl_spirv_fs_len = fs_len;
    slot->gl_spirv_substage = 0;

    _SituationMakeGLContextCurrentForHostThread();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderBinary(1, &vs, GL_SHADER_BINARY_FORMAT_SPIR_V, slot->gl_spirv_vs_copy, (GLsizei)vs_len);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderBinary(1, &fs, GL_SHADER_BINARY_FORMAT_SPIR_V, slot->gl_spirv_fs_copy, (GLsizei)fs_len);
    SIT_CHECK_GL_ERROR();

    slot->gl_async_vs_shader = vs;
    slot->gl_async_fs_shader = fs;
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_SPIRV;
    *out_shader = handle;
    return SITUATION_SUCCESS;
}

static SituationError _SituationPollGLAsyncShaderLoad(_SituationShaderSlot* slot) {
    if (!slot) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (slot->gl_async_load_stage == SIT_GL_ASYNC_STAGE_SPIRV) {
        return _SituationPollGLAsyncSpirvShaderLoad(slot);
    }
    if (slot->gl_async_load_stage != SIT_GL_ASYNC_STAGE_COMPILE) {
        return SITUATION_SUCCESS;
    }

    SituationError vs_err = SITUATION_SUCCESS;
    SituationError fs_err = SITUATION_SUCCESS;

    if (slot->gl_async_vs_shader) {
        int vs_done = _SituationPollGLShaderCompile(slot->gl_async_vs_shader, GL_VERTEX_SHADER, &vs_err);
        if (!vs_done) {
            return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
        }
        if (vs_err != SITUATION_SUCCESS) {
            return _SituationGLAsyncLoadFail(slot);
        }
    }
    if (slot->gl_async_fs_shader) {
        int fs_done = _SituationPollGLShaderCompile(slot->gl_async_fs_shader, GL_FRAGMENT_SHADER, &fs_err);
        if (!fs_done) {
            return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
        }
        if (fs_err != SITUATION_SUCCESS) {
            return _SituationGLAsyncLoadFail(slot);
        }
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateProgram failed during async GLSL link.");
        return _SituationGLAsyncLoadFail(slot);
    }
    glAttachShader(program, slot->gl_async_vs_shader);
    glAttachShader(program, slot->gl_async_fs_shader);
    glLinkProgram(program);
    glDeleteShader(slot->gl_async_vs_shader);
    glDeleteShader(slot->gl_async_fs_shader);
    slot->gl_async_vs_shader = 0;
    slot->gl_async_fs_shader = 0;
    slot->gl_pending_program_id = program;
    slot->gl_is_linking = true;
    slot->gl_pending_link_spirv = false;
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_IDLE;
    return SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS;
}

/**
 * @brief [INTERNAL] Creates a standard two-stage (vertex + fragment) OpenGL shader program from GLSL source.
 * @details This is the primary internal helper for creating graphics pipelines on the OpenGL backend. It orchestrates the process of compiling individual vertex and fragment shader sources and linking them together into a complete, usable `glProgram`.
 *
 * @par Creation Process
 *   1.  Calls `_SituationCompileGLShader` to compile the vertex shader source (`vs_src`).
 *   2.  If successful, it calls `_SituationCompileGLShader` to compile the fragment shader source (`fs_src`).
 *   3.  If both shaders compile successfully, it creates a new program object (`glCreateProgram`), attaches both shaders, and links them (`glLinkProgram`).
 *   4.  After linking, the individual shader objects are detached and deleted, as they are no longer needed.
 *   5.  Finally, it checks the link status and reports any errors.
 *
 * @param vs_src A null-terminated C string containing the vertex shader source code.
 * @param fs_src A null-terminated C string containing the fragment shader source code.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful compilation and linking.
 * @return `0` on failure. On failure, a detailed error message (from either the compiler or linker) is set via `_SituationSetErrorFromCode`, and all intermediate resources are cleaned up.
 *
 * @note This function is for internal use by high-level `SituationLoadShader*` functions.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram` when it is no longer needed.
 *
 * @see SituationLoadShaderFromMemory(), _SituationCompileGLShader()
 */
static GLuint _SituationCreateGLShaderProgramAsync(const char* vs_src, const char* fs_src, SituationError* error_code) {
    if (!vs_src || !fs_src) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    SituationError local_err = SITUATION_SUCCESS;
    GLuint vs = _SituationCompileGLShader(vs_src, GL_VERTEX_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint fs = _SituationCompileGLShader(fs_src, GL_FRAGMENT_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        glDeleteShader(vs);
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create shader program");
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // [Async] Defer status check.
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates and links a complete OpenGL shader program from vertex and fragment GLSL source strings.
 *
 * @details This is a low-level helper function that performs the full OpenGL shader creation pipeline:
 *            1. Creates vertex and fragment shader objects
 *            2. Attaches the provided source strings (`vs_src` and `fs_src`)
 *            3. Compiles both shaders individually
 *            4. Checks compilation status and logs errors/info logs on failure
 *            5. Creates and links the program object
 *            6. Checks link status and logs errors/info logs on failure
 *            7. Cleans up intermediate shader objects on success or failure
 *
 *          Intended for use during shader creation, hot-reload, internal quad/compute pipelines,
 *          or when SPIR-V binary path is unavailable (e.g. no GL_ARB_gl_spirv extension).
 *
 *          On success, returns a valid `GLuint` program name that must be deleted later
 *          with `glDeleteProgram` (typically via graveyard/deferred cleanup in render thread).
 *
 * @param vs_src Null-terminated string containing the vertex shader GLSL source code.
 *               Must remain valid for the duration of the call.
 * @param fs_src Null-terminated string containing the fragment shader GLSL source code.
 *               Must remain valid for the duration of the call.
 * @param error_code Pointer to a `SituationError` variable that receives the detailed error code
 *                   on failure. On success, set to `SITUATION_SUCCESS`.
 *                   May be NULL if the caller does not need the error detail.
 *
 * @return A valid OpenGL program object name (`GLuint`) on success,
 *         0 on failure (compilation or linking error).
 *         On failure, `*error_code` is set to an appropriate value:
 *           - SITUATION_ERROR_SHADER_COMPILATION_FAILED (vertex or fragment compile error)
 *           - SITUATION_ERROR_SHADER_LINK_FAILED (program link error)
 *           - SITUATION_ERROR_INVALID_PARAM (null source strings)
 *           - SITUATION_ERROR_GL_ERROR (underlying GL call failed)
 *
 * Thread safety invariants:
 *   - Must be called from a thread that has an active OpenGL context (typically render thread
 *     or main thread during init/hot-reload)
 *   - No internal locking  -  caller must ensure no concurrent GL calls on the same context
 *   - Safe during hot-reload if old programs are deleted first
 *
 * @note In debug builds, full GLSL compile/link info logs are printed to stderr on failure.
 *       In release builds, only high-level errors are logged.
 *       The function does **not** validate GLSL syntax beyond what the driver reports.
 *       Shader objects are always detached and deleted  -  caller only needs to manage the returned program.
 *
 * @see _SituationCreateGLShaderProgramFromSpirv, _SituationCreateGLComputeProgramFromSpirv,
 *      glCreateShader, glShaderSource, glCompileShader, glCreateProgram, glLinkProgram,
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED, SITUATION_ERROR_SHADER_LINK_FAILED
 */
static GLuint _SituationCreateGLShaderProgram(const char* vs_src, const char* fs_src, SituationError* error_code) {
    if (!vs_src || !fs_src) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    SituationError local_err = SITUATION_SUCCESS;
    GLuint vs = _SituationCompileGLShader(vs_src, GL_VERTEX_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint fs = _SituationCompileGLShader(fs_src, GL_FRAGMENT_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        glDeleteShader(vs);
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create shader program");
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    // [Phase 5] Virtual Bindless Sampler Setup
    if (success && !SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        glUseProgram(program);
        GLint loc = glGetUniformLocation(program, "_sit_virtual_textures");
        if (loc >= 0) {
            // Set bindings 0-31
            GLint bindings[32];
            for (int i = 0; i < 32; i++) bindings[i] = i;
            glUniform1iv(loc, 32, bindings);
        }
        glUseProgram(0);
    }

    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        // Dynamically allocate a buffer large enough for the full link error log.
        if (log_length > 0) {
            char* infoLog = (char*)SIT_MALLOC(log_length);
            if (infoLog) {
                glGetProgramInfoLog(program, log_length, NULL, infoLog);
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
                SIT_FREE(infoLog);
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for shader link log.");
            }
        } else {
            // No log, provide a generic message.
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, "An unknown linking error occurred.");
        }
        glDeleteProgram(program); // Add this line
        glDeleteShader(vs);       // Ensure shaders are deleted
        glDeleteShader(fs);       // Ensure shaders are deleted
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        return 0;
    }

    _SituationBindGLProgramStorageBlocks(program);

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates a single-stage (compute) OpenGL shader program from GLSL source.
 * @details This helper function is the traditional GLSL-based creation path for compute shaders. It is called by the `_SituationCreateGLComputeProgram` dispatcher when the source type is GLSL or when SPIR-V is not supported.
 *
 * @par Creation Process
 *   1.  Calls `_SituationCompileGLShader` to compile the compute shader source (`cs_src`) with the type `GL_COMPUTE_SHADER`.
 *   2.  If compilation is successful, it creates a new program object, attaches the compute shader, and links the program.
 *   3.  The individual shader object is deleted after linking.
 *   4.  The link status is checked to ensure a valid executable program was created.
 *
 * @param cs_src A null-terminated C string containing the compute shader source code.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful compilation and linking.
 * @return `0` on failure. On failure, a detailed error message is set, and all intermediate resources are cleaned up.
 *
 * @note This function is for internal use by `_SituationCreateGLComputeProgram` only. It specifically handles the single-stage linking process required for compute programs.
 *
 * @see _SituationCreateGLComputeProgram(), _SituationCompileGLShader()
 */
static GLuint _SituationCreateGLShaderProgramFromSource(const char* cs_src, SituationError* error_code) {
    if (!cs_src) {
        // This check is now primarily for internal consistency
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        // _SituationCreateGLComputeProgram already set the error msg, avoid duplication or override it carefully
        // _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationCreateGLShaderProgramFromSource: Compute shader source cannot be NULL");
        return 0;
    }

    // 1. Compile the compute shader
    SituationError local_err = SITUATION_SUCCESS;
    GLuint cs = _SituationCompileGLShader(cs_src, GL_COMPUTE_SHADER, &local_err); // Assume this function exists and handles glGetShaderiv(GL_COMPILE_STATUS, ...)
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0; // Error message already set by _SituationCompileGLShader
    }

    // 2. Create a program and attach the shader
    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationCreateGLShaderProgramFromSource: Failed to create shader program object");
        glDeleteShader(cs); // Clean up the successfully created shader
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, cs);
    glLinkProgram(program);

    // 3. Shader is linked, we no longer need the individual shader object
    glDeleteShader(cs);
    cs = 0; // Good practice

    // 4. Check for linking errors
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    // [Phase 5] Virtual Bindless Sampler Setup
    if (success && !SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        glUseProgram(program);
        GLint loc = glGetUniformLocation(program, "_sit_virtual_textures");
        if (loc >= 0) {
            // Set bindings 0-31
            GLint bindings[32];
            for (int i = 0; i < 32; i++) bindings[i] = i;
            glUniform1iv(loc, 32, bindings);
        }
        glUseProgram(0);
    }

    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        // Handle potential glGetProgramiv failure? Unlikely, but glGetError could check.
        if (log_length > 0) {
            char* infoLog = (char*)SIT_MALLOC((size_t)log_length);
            if (infoLog) {
                glGetProgramInfoLog(program, log_length, NULL, infoLog);
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
                SIT_FREE(infoLog);
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationCreateGLShaderProgramFromSource: Failed to allocate memory for shader link log.");
            }
        } else {
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, "_SituationCreateGLShaderProgramFromSource: An unknown linking error occurred (no log available).");
        }
        glDeleteProgram(program); // Clean up the unsuccessfully linked program
        program = 0;
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        return 0;
    }

    // Success path for this helper
    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates a single-stage OpenGL compute program, dispatching to the optimal creation path.
 * @details This function acts as a high-level dispatcher for creating OpenGL compute shaders.
 *          It intelligently selects the best method - either using modern, pre-compiled SPIR-V bytecode or falling back to traditional GLSL source compilation - based on the type of data provided and the capabilities of the current OpenGL driver.
 *
 * @par Dispatch Logic
 *   - If `source_type` is `SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV` and the `GL_ARB_gl_spirv` extension is available, it calls `_SituationCreateGLComputeProgramFromSpirv` for the fastest and most consistent creation path.
 *   - Otherwise, if `source_type` is `SITUATION_GL_SHADER_SOURCE_TYPE_GLSL`, it calls `_SituationCreateGLShaderProgramFromSource` to perform traditional compilation and linking.
 *   - If SPIR-V data is provided but the driver does not support it, the function will fail and report an error.
 *
 * @param source_data A pointer to the shader data. This must be a `const struct _SituationSpirvBlob*` for SPIR-V or a `const char*` for GLSL.
 * @param source_type An enum (`SituationGLShaderSourceType`) specifying whether `source_data` points to GLSL or SPIR-V.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful creation and linking.
 * @return `0` on failure. On failure, a detailed error message is set by one of the internal helper functions.
 *
 * @note This is the sole entry point for all internal OpenGL compute shader creation and is called by `SituationCreateComputePipelineFromMemory`.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram`.
 *
 * @see SituationCreateComputePipelineFromMemory(), _SituationCreateGLComputeProgramFromSpirv(), _SituationCreateGLShaderProgramFromSource()
 */
static GLuint _SituationCreateGLComputeProgram(const void* source_data, SituationGLShaderSourceType source_type, SituationError* error_code) {
    if (error_code) *error_code = SITUATION_SUCCESS;

    if (!source_data) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationCreateGLComputeProgram: Shader source data cannot be NULL.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // If we have SPIR-V data and the driver supports it, use the modern path.
    if (source_type == SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV && GLAD_GL_ARB_gl_spirv) {
        return _SituationCreateGLComputeProgramFromSpirv((const struct _SituationSpirvBlob*)source_data, error_code);
    }
#endif

    // Fallback to the traditional GLSL source path in all other cases.
    if (source_type == SITUATION_GL_SHADER_SOURCE_TYPE_GLSL) {
        return _SituationCreateGLShaderProgramFromSource((const char*)source_data, error_code);
    }

    // If we get here, it means we were given SPIR-V but couldn't use it (either compiler is off or driver lacks support).
    _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Received SPIR-V data, but cannot process it (GL_ARB_gl_spirv unavailable or shader compiler disabled).");
    if (error_code) *error_code = SITUATION_ERROR_OPENGL_UNSUPPORTED;
    return 0;
}
#endif


 /**
 * @brief [core] Creates a compute pipeline directly from GLSL source code provided as a C string in memory.
 *
 * @details This function compiles the provided GLSL compute shader source code into SPIR-V bytecode (if the shader compiler is enabled) and then creates the corresponding backend-specific compute pipeline object (e.g., OpenGL program, Vulkan pipeline).
 *          The resulting `SituationComputePipeline` handle can be used with `SituationCmdBindComputePipeline` and `SituationCmdDispatch` to execute compute work on the GPU.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Compiles the GLSL source into an OpenGL Compute Program. If `SITUATION_ENABLE_SHADER_COMPILER` and `GL_ARB_gl_spirv` are available, it may compile to SPIR-V first for consistency with Vulkan.
 * - **Vulkan:** This backend **requires** `SITUATION_ENABLE_SHADER_COMPILER`. The function uses `shaderc` to compile the GLSL source into a SPIR-V binary blob, which is then used to create a `VkPipeline`.
 *
 * @param compute_shader_source A null-terminated string containing the GLSL compute shader source code. Must not be NULL.
 *
 * @return A `SituationComputePipeline` handle.
 *         - On **success**: The handle's `.id` member will be non-zero, and it can be used for binding and dispatching. The caller is responsible for destroying it using `SituationDestroyComputePipeline()` to prevent resource leaks.;
 *         - On **failure**: The handle will be in an invalid state (`.id` == 0). A detailed error message can be retrieved using `SituationGetLastErrorMsg()`.
 *           Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *
 * @note The caller is **responsible** for destroying the returned pipeline using `SituationDestroyComputePipeline()` to prevent GPU and CPU memory leaks.;
 * @note This function requires the library to be initialized (`SituationInit()` must have been called successfully).
 * @note This function requires the `SITUATION_ENABLE_SHADER_COMPILER` define to be set during compilation for the Vulkan backend to work with GLSL source. For OpenGL, it depends on the internal handling of GLSL vs SPIR-V (as discussed in `_SituationCreateGLComputeProgram`).
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @par Resource Tracking and Potential Leaks:
 * If this function succeeds (returns a handle with `.id != 0`), the underlying GPU resources are valid.
 * However, an internal CPU memory allocation for resource tracking might fail. In this rare case:
 * - A warning will be printed to `stderr` (e.g., "WARNING: Potential leak of Vulkan compute pipeline handle...").
 * - The valid GPU resource handle is still returned.
 * - It is the caller's **absolute responsibility** to call `SituationDestroyComputePipeline()` on the returned handle to prevent a GPU resource leak, as the library's automatic shutdown cleanup will not track this specific resource.;
 *
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 * @see SituationCreateComputePipeline()
 * @see SituationDestroyComputePipeline();
 * @see SituationCmdBindComputePipeline()
 * @see SituationCmdDispatch()
 */
SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline) {
    if (!SituationIsInitialized()) return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCreateComputePipelineFromMemory: Library not initialized.");
    if (!compute_shader_source) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipelineFromMemory: Compute shader source cannot be NULL.");
    if (!out_pipeline) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipelineFromMemory: out_pipeline cannot be NULL.");

    SituationComputePipeline handle;
    _SituationComputePipelineSlot* slot = _SitAllocComputePipelineSlot(&handle);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    slot->layout_type = layout_type;

#if defined(SITUATION_USE_OPENGL)
    // OpenGL Compute Creation
    SituationError err;
    _SituationMakeGLContextCurrentForHostThread();
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // SPIR-V path
    _SituationSpirvBlob cs_spirv = _SituationOpenGLCompileGLSLtoSPIRV(compute_shader_source, "compute_shader", shaderc_compute_shader);
    if (!cs_spirv.data) {
        _SituationFreeSpirvBlob(&cs_spirv);
        _SituationReleaseHostGLContextIfInFrame();
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED;
    }
    slot->gl_program_id = _SituationCreateGLComputeProgramFromSpirv(&cs_spirv, &err);
    _SituationFreeSpirvBlob(&cs_spirv);
#else
    // GLSL path
    slot->gl_program_id = _SituationCreateGLComputeProgram(compute_shader_source, SITUATION_GL_SHADER_SOURCE_TYPE_GLSL, &err);
#endif

    if (err != SITUATION_SUCCESS) {
        _SituationReleaseHostGLContextIfInFrame();
        _SitFreeComputePipelineSlot(handle);
        return err;
    }
    _SituationReleaseHostGLContextIfInFrame();

    /* Vulkan TWO_SSBOS uses two descriptor sets (binding 0 each). OpenGL maps API set_index to
     * glBindBufferBase(SSBO, set_index, ...), i.e. SSBO binding points 0 and 1. SPIR-V from the same
     * GLSL can still reflect both blocks at binding 0 unless we assign block bindings explicitly. */
    if (layout_type == SIT_COMPUTE_LAYOUT_TWO_SSBOS && slot->gl_program_id != 0) {
        GLuint prog = slot->gl_program_id;
        GLuint r_in = glGetProgramResourceIndex(prog, GL_SHADER_STORAGE_BLOCK, "InBuffer");
        GLuint r_out = glGetProgramResourceIndex(prog, GL_SHADER_STORAGE_BLOCK, "OutBuffer");
        if (r_in != GL_INVALID_INDEX) glShaderStorageBlockBinding(prog, r_in, 0);
        if (r_out != GL_INVALID_INDEX) glShaderStorageBlockBinding(prog, r_out, 1);
    }

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan Compute Creation
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    _SituationSpirvBlob cs_spirv = _SituationVulkanCompileGLSLtoSPIRV(compute_shader_source, "compute_shader", shaderc_compute_shader);
    if (!cs_spirv.data) {
        _SituationFreeSpirvBlob(&cs_spirv);
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    VkPipelineLayout layout = sit_render.vk.compute_layouts[layout_type];
    if (layout == VK_NULL_HANDLE) layout = sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_ONE_SSBO]; // Fallback

    // INLINED LOGIC:
    VkShaderModule shaderModule = _SituationVulkanCreateShaderModule(cs_spirv.data, cs_spirv.size);
    _SituationFreeSpirvBlob(&cs_spirv); // Free the blob immediately after creating the module

    if (shaderModule == VK_NULL_HANDLE) {
         _SitFreeComputePipelineSlot(handle);
         return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module = shaderModule;
    computePipelineCreateInfo.stage.pName = "main";
    computePipelineCreateInfo.layout = layout;

    VkPipeline vk_pipeline;
    if (vkCreateComputePipelines(sit_render.vk.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, &vk_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(sit_render.vk.device, shaderModule, NULL);
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    slot->vk_pipeline = vk_pipeline;
    slot->vk_pipeline_layout = layout;
    slot->shader_module = shaderModule; // Store module to destroy later

#else
    _SitFreeComputePipelineSlot(handle);
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Vulkan compute requires shader compiler.");
#endif // SITUATION_ENABLE_SHADER_COMPILER

#endif // SITUATION_USE_VULKAN

    *out_pipeline = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Creates a compute pipeline by loading GLSL source code from a file.
 *
 * @details This function is a convenience wrapper around `SituationCreateComputePipelineFromMemory`. It loads the GLSL compute shader source code from the specified file path into memory and then uses that string to create the compute pipeline using the standard process.
 *
 * @param compute_shader_path The file system path to the GLSL compute shader source file (e.g., "shaders/compute_filter.comp"). This path must be valid and accessible.
 *
 * @return A `SituationComputePipeline` handle.
 *         - On **success**, the handle's `id` member will be non-zero, and it can be used with functions like `SituationCmdBindComputePipeline` and `SituationDestroyComputePipeline`.;
 *         - On **failure** (e.g., file not found, read error, compilation error, pipeline creation failure), the handle will be zero-initialized (`{0}`). Use `SituationGetLastErrorMsg()` to retrieve a detailed error message.
 *
 * @note This function requires the library to be successfully initialized  (`SituationInit` must have been called).
 * @note This function requires `SITUATION_ENABLE_SHADER_COMPILER` to be defined if runtime compilation of GLSL to SPIR-V is needed (which is the standard process for Vulkan and often for OpenGL).
 * @note The caller is responsible for eventually destroying the returned pipeline using `SituationDestroyComputePipeline` to prevent GPU and CPU memory leaks.;
 *
 * @see SituationCreateComputePipelineFromMemory(), SituationDestroyComputePipeline(), SituationLoadFileText();
 */
SITAPI SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline) {
    if (!SituationIsInitialized()) return _SituationSetErrorFromCode( SITUATION_ERROR_NOT_INITIALIZED, "SituationCreateComputePipeline: Library not initialized." );
    if (!compute_shader_path) return _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipeline: compute_shader_path cannot be NULL." );

    char* source = SituationLoadFileText(compute_shader_path);
    if (!source) {
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationCreateComputePipelineFromMemory(source, layout_type, out_pipeline);

    if (err == SITUATION_SUCCESS) {
        _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*out_pipeline);
        if (slot) {
            slot->source_path = _sit_strdup(compute_shader_path);
            slot->mod_time = SituationGetFileModTime(compute_shader_path);
        }
    }

    SIT_FREE(source);
    return err;
}


// --- Updated/Added Documentation Block for SituationDestroyComputePipeline ---;
/**
 * @brief Destroys a compute pipeline and frees all associated resources.
 *
 * @details This function cleans up the resources acquired during the creation of a `SituationComputePipeline`. This includes backend-specific objects
 *          (e.g., OpenGL program, Vulkan pipeline/layout) and removing the pipeline from the library's internal resource tracking list.
 *          It is crucial to call this function for every successfully created `SituationComputePipeline` to prevent memory leaks of both GPU resources and CPU-side tracking structures.
 *
 * @param pipeline A pointer to the `SituationComputePipeline` handle to be destroyed. The handle's `id` member must be non-zero.
 *                 The contents of the struct pointed to by `pipeline` will be zeroed upon successful destruction.
 *
 * @note It is safe to call this function on an already destroyed or invalid pipeline (where `pipeline->id` is 0); it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 * @warning This function must only be called before `SituationShutdown`.
 * @warning After calling this function, the `SituationComputePipeline` handle pointed to by `pipeline` becomes invalid and must not be used again.
 *
 * @see SituationCreateComputePipeline(), SituationCreateComputePipelineFromMemory()
 */
SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline) {
    if (!pipeline) return;
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*pipeline);
    if (!slot) return;

#if defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE) {
        if (slot->vk_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline, NULL);
            slot->vk_pipeline = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyPipeline(slot->vk_pipeline, VK_NULL_HANDLE);
    }
    if (slot->shader_module != VK_NULL_HANDLE && sit_render.vk.device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(sit_render.vk.device, slot->shader_module, NULL);
        slot->shader_module = VK_NULL_HANDLE;
    }

#elif defined(SITUATION_USE_OPENGL)
    if (glIsProgram(slot->gl_program_id)) {
        glDeleteProgram(slot->gl_program_id);
    }
#endif

    _SitFreeComputePipelineSlot(*pipeline);
    memset(pipeline, 0, sizeof(SituationComputePipeline));
}

#if defined(SITUATION_USE_VULKAN)
static VkShaderModule _SituationVulkanCreateShaderModuleEx(
    const void* code, size_t code_size, const char* stage_label, SituationError fail_code) {
    if (!code || code_size == 0) {
        char detail[160];
        snprintf(detail, sizeof(detail), "%s: null or empty SPIR-V blob", stage_label);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, detail);
        return VK_NULL_HANDLE;
    }
    if ((code_size & 3u) != 0) {
        char detail[160];
        snprintf(detail, sizeof(detail), "%s: SPIR-V size %zu is not a multiple of 4", stage_label, code_size);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, detail);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = (const uint32_t*)code;

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(sit_render.vk.device, &create_info, NULL, &shader_module);
    if (result != VK_SUCCESS) {
        char detail[SITUATION_MAX_ERROR_MSG_LEN];
        snprintf(
            detail, sizeof(detail),
            "%s SPIR-V (%zu bytes): vkCreateShaderModule VkResult 0x%x",
            stage_label, code_size, (unsigned)result);
        _SituationSetErrorFromCode(fail_code, detail);
        return VK_NULL_HANDLE;
    }

    return shader_module;
}

static VkShaderModule _SituationVulkanCreateShaderModule(const void* code, size_t code_size) {
    return _SituationVulkanCreateShaderModuleEx(
        code, code_size, "shader", SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED);
}

/**
 * @brief [INTERNAL] Creates a complete Vulkan graphics pipeline from SPIR-V bytecode.
 * @details This is a generic and powerful helper function that encapsulates the complexity of Vulkan pipeline creation. It takes compiled SPIR-V, a pre-created pipeline layout, and vertex format descriptions to build a complete, ready-to-use VkPipeline object.
 *
 * @param vs_data A pointer to the raw SPIR-V bytecode for the vertex shader.
 * @param vs_size The size of the vertex shader bytecode in bytes.
 * @param fs_data A pointer to the raw SPIR-V bytecode for the fragment shader.
 * @param fs_size The size of the fragment shader bytecode in bytes.
 * @param pipelineLayout The pre-created VkPipelineLayout that defines the descriptor sets and push constants this pipeline will use.
 * @param topology The primitive topology (e.g., triangles, lines, points, triangle strip).
 * @param vertexBindingCount The number of vertex buffer bindings.
 * @param pVertexBindingDescriptions A pointer to an array of vertex binding descriptions.
 * @param vertexAttributeCount The number of vertex attributes.
 * @param pVertexAttributeDescriptions A pointer to an array of vertex attribute descriptions.
 * @param pipeline_flags Bit **SIT_VK_PIPELINE_BLEND_OPAQUE** — disable color blending (opaque writes). Used by built-in quad draws so results match OpenGL solid fills under alpha blending.
 * @return A valid VkPipeline handle on success, or VK_NULL_HANDLE on failure.
 */
static void _SitVulkanFillGraphicsDynamicStates(VkDynamicState* states, uint32_t* out_count) {
    uint32_t n = 0;
    states[n++] = VK_DYNAMIC_STATE_VIEWPORT;
    states[n++] = VK_DYNAMIC_STATE_SCISSOR;
    states[n++] = VK_DYNAMIC_STATE_LINE_WIDTH;
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        states[n++] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
        states[n++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
        states[n++] = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
        states[n++] = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
    }
    if (sit_render.vk.extended_dynamic_state3_polygon_mode_enabled && sit_render.vk.pfn_cmd_set_polygon_mode_ext) {
        states[n++] = VK_DYNAMIC_STATE_POLYGON_MODE_EXT;
    }
    if (sit_render.vk.depth_bias_dynamic_enabled) {
        states[n++] = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE;
        states[n++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    }
    if (sit_render.vk.extended_dynamic_state3_color_write_enabled &&
        sit_render.vk.pfn_cmd_set_color_write_mask_ext) {
        states[n++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
    }
    if (sit_render.vk.pfn_cmd_set_stencil_test_enable) {
        states[n++] = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
    }
    if (sit_render.vk.pfn_cmd_set_stencil_op) {
        states[n++] = VK_DYNAMIC_STATE_STENCIL_OP;
        states[n++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
        states[n++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
        states[n++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }
    *out_count = n;
}

static VkPipeline _SituationVulkanCreateGraphicsPipeline(
    const void* vs_data, size_t vs_size,
    const void* fs_data, size_t fs_size,
    VkPipelineLayout pipelineLayout,
    VkPrimitiveTopology topology,
    uint32_t vertexBindingCount,
    const VkVertexInputBindingDescription* pVertexBindingDescriptions,
    uint32_t vertexAttributeCount,
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,
    uint32_t pipeline_flags,
    VkCullModeFlags cull_mode,
    VkFrontFace front_face,
    VkPolygonMode polygon_mode)
{
    return _SituationVulkanCreateGraphicsPipelineEx(
        vs_data, vs_size, fs_data, fs_size, pipelineLayout, topology,
        vertexBindingCount, pVertexBindingDescriptions, vertexAttributeCount, pVertexAttributeDescriptions,
        pipeline_flags, cull_mode, front_face, polygon_mode,
        VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT);
}

static VkPipeline _SituationVulkanCreateGraphicsPipelineEx(
    const void* vs_data, size_t vs_size,
    const void* fs_data, size_t fs_size,
    VkPipelineLayout pipelineLayout,
    VkPrimitiveTopology topology,
    uint32_t vertexBindingCount,
    const VkVertexInputBindingDescription* pVertexBindingDescriptions,
    uint32_t vertexAttributeCount,
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,
    uint32_t pipeline_flags,
    VkCullModeFlags cull_mode,
    VkFrontFace front_face,
    VkPolygonMode polygon_mode,
    VkFormat dynamic_color_format,
    VkFormat dynamic_depth_format,
    VkSampleCountFlagBits rasterization_samples)
{
    if (rasterization_samples == 0) {
        rasterization_samples = VK_SAMPLE_COUNT_1_BIT;
    }
    VkShaderModule vs_module = _SituationVulkanCreateShaderModuleEx(
        vs_data, vs_size, "vertex", SITUATION_ERROR_VULKAN_SPIRV_VS_MODULE_FAILED);
    if (vs_module == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    VkShaderModule fs_module = _SituationVulkanCreateShaderModuleEx(
        fs_data, fs_size, "fragment", SITUATION_ERROR_VULKAN_SPIRV_FS_MODULE_FAILED);
    if (fs_module == VK_NULL_HANDLE) {
        vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo vs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo fs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_stage_info, fs_stage_info};

    // 2. Define the pipeline's fixed-function states.
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = vertexBindingCount;
    vertex_input_info.pVertexBindingDescriptions = pVertexBindingDescriptions;
    vertex_input_info.vertexAttributeDescriptionCount = vertexAttributeCount;
    vertex_input_info.pVertexAttributeDescriptions = pVertexAttributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = topology, .primitiveRestartEnable = VK_FALSE };
    VkPipelineViewportStateCreateInfo viewport_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = polygon_mode;
    rasterizer.lineWidth = 1.0f;
    // Disable backface culling for all 2D rendering (text, quads, VD compositing)
    // The quad vertices produce counter-clockwise triangles under top-left-origin ortho projection
    // which would be culled with BACK_BIT + CLOCKWISE front face.
    rasterizer.cullMode = cull_mode;
    rasterizer.frontFace = front_face;
    rasterizer.depthBiasEnable = VK_FALSE;
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = rasterization_samples;
    multisampling.sampleShadingEnable = (rasterization_samples != VK_SAMPLE_COUNT_1_BIT &&
                                         sit_render.vk.dynamic_ms_sample_shading_enable) ? VK_TRUE : VK_FALSE;
    multisampling.minSampleShading = sit_render.vk.dynamic_ms_min_sample_shading;
    multisampling.alphaToCoverageEnable = (rasterization_samples != VK_SAMPLE_COUNT_1_BIT &&
                                           sit_render.vk.dynamic_ms_alpha_to_coverage_enable) ? VK_TRUE : VK_FALSE;

    /* TRIANGLE_LIST: depth write off (text passes transparent fragments without occluding).
       TRIANGLE_STRIP: 2D quad strips — depth write off; use <= so fragments at z matching cleared depth pass.
       SIT_VK_PIPELINE_NO_DEPTH: internal 2D compositors (VD) — depth off entirely. */
    VkBool32 depth_test_enable = VK_TRUE;
    VkBool32 enableDepthWrite = VK_TRUE;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS;
    if (pipeline_flags & SIT_VK_PIPELINE_NO_DEPTH) {
        depth_test_enable = VK_FALSE;
        enableDepthWrite = VK_FALSE;
    } else if (dynamic_color_format != VK_FORMAT_UNDEFINED && dynamic_depth_format == VK_FORMAT_UNDEFINED) {
        depth_test_enable = VK_FALSE;
        enableDepthWrite = VK_FALSE;
    } else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        enableDepthWrite = VK_FALSE;
    } else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
        enableDepthWrite = VK_FALSE;
        depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    }
    VkPipelineDepthStencilStateCreateInfo depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = depth_test_enable, .depthWriteEnable = enableDepthWrite, .depthCompareOp = depth_compare, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = (pipeline_flags & SIT_VK_PIPELINE_BLEND_OPAQUE) ? VK_FALSE : VK_TRUE;
    if (pipeline_flags & SIT_VK_PIPELINE_BLEND_ADDITIVE) {
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    } else if (pipeline_flags & SIT_VK_PIPELINE_BLEND_MULTIPLY) {
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    } else if (pipeline_flags & SIT_VK_PIPELINE_BLEND_SCREEN) {
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    } else {
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &color_blend_attachment };

    VkDynamicState dynamic_states[16];
    uint32_t dynamic_state_count = 0;
    _SitVulkanFillGraphicsDynamicStates(dynamic_states, &dynamic_state_count);
    VkPipelineDynamicStateCreateInfo dynamic_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = dynamic_state_count, .pDynamicStates = dynamic_states };

    // 3. Assemble the pipeline create info struct.
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipelineLayout;
    VkPipelineRenderingCreateInfo rendering_info = {};
    if (dynamic_color_format != VK_FORMAT_UNDEFINED) {
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &dynamic_color_format;
        rendering_info.depthAttachmentFormat = dynamic_depth_format;
        pipeline_info.pNext = &rendering_info;
        pipeline_info.renderPass = VK_NULL_HANDLE;
        pipeline_info.subpass = 0;
    } else {
        pipeline_info.renderPass = sit_render.vk.main_window_render_pass;
        pipeline_info.subpass = 0;
    }
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    // 4. Create the final graphics pipeline object.
    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(sit_render.vk.device, _SitVkPipelineCacheHandle(), 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "vkCreateGraphicsPipelines failed.");
        pipeline = VK_NULL_HANDLE; // Ensure we return NULL on failure
    }

    // 5. Clean up the temporary shader modules, as they are now baked into the pipeline.
    vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
    vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);

    return pipeline;
}

#endif // SITUATION_USE_VULKAN

/**
 * @brief [High-Level] Loads, compiles, and creates a graphics shader pipeline from GLSL source files.
 * @details This is the recommended high-level function for loading shaders from disk. It acts as a convenience wrapper, performing a multi-step process:
 *          1. Reads the vertex and fragment shader source code from the specified files using `SituationLoadFileText`.
 *          2. Passes the in-memory source code to `SituationLoadShaderFromMemory` for compilation and GPU resource creation.
 *          3. Cleans up the temporary memory buffers used to hold the source code.
 *
 * @param vs_path The file system path to the vertex shader GLSL source file (e.g., "shaders/pbr.vert").
 * @param fs_path The file system path to the fragment shader GLSL source file (e.g., "shaders/pbr.frag").
 *
 * @return A `SituationShader` handle.
 *         - On success, the `id` member of the returned struct will be non-zero, and the shader is ready for use.
 *         - On failure (e.g., file not found, compilation error), an invalid handle (`id == 0`) is returned.
 *           Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *
 * @note The caller is **responsible** for destroying the returned shader using `SituationUnloadShader()` to prevent GPU memory leaks.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationLoadShaderFromMemory()
 * @see SituationUnloadShader()
 */
SITAPI SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    char* vs_source = SituationLoadFileText(vs_path);
    if (!vs_source) return SITUATION_ERROR_FILE_NOT_FOUND;

    char* fs_source = SituationLoadFileText(fs_path);
    if (!fs_source) {
        SIT_FREE(vs_source);
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationLoadShaderFromMemory(vs_source, fs_source, out_shader);

    // Store paths for hot-reload
    if (err == SITUATION_SUCCESS) {
        _SituationShaderSlot* slot = _SitGetShaderSlot(*out_shader);
        if (slot) {
            slot->vs_path = _sit_strdup(vs_path);
            slot->fs_path = _sit_strdup(fs_path);
            slot->vs_mod_time = SituationGetFileModTime(vs_path);
            slot->fs_mod_time = SituationGetFileModTime(fs_path);
        }
    }

    SIT_FREE(vs_source);
    SIT_FREE(fs_source);
    return err;
}


/**
 * @brief [Core] Creates a graphics shader pipeline from GLSL source code provided as C strings.
 * @details This is the core function for creating graphics pipelines. It takes in-memory GLSL source code for vertex and fragment shaders, orchestrates the backend-specific compilation and linking process, and returns a handle to the final,
 * ready-to-use GPU pipeline object. It also registers the new resource with the internal resource manager for leak detection at shutdown.
 *
 * @par Backend-Specific Compilation
 * - **OpenGL:** The GLSL source strings are passed directly to the OpenGL driver for compilation (`glCompileShader`) and linking (`glLinkProgram`) into a shader program object.
 *     If `SITUATION_ENABLE_SHADER_COMPILER` is defined and the `GL_ARB_gl_spirv` extension is available, the source may first be compiled to SPIR-V for consistency with Vulkan.
 * - **Vulkan:** This backend **requires** `SITUATION_ENABLE_SHADER_COMPILER`. The function uses `shaderc` to compile both the vertex and fragment GLSL sources into separate SPIR-V binary blobs. These blobs are then used to construct a complete `VkPipeline` object, including its `VkPipelineLayout`.
 *
 * @param vs_code The null-terminated string containing the vertex shader source code.
 * @param fs_code The null-terminated string containing the fragment shader source code.
 *
 * @return A `SituationShader` handle.
 *         - On success, the `id` member will be non-zero, and the handle is ready for use with `SituationCmdBindPipeline`.
 *         - On failure (e.g., a syntax error in the shader code, a linking error, or a resource allocation failure), an invalid handle (`id == 0`) is returned. Use `SituationGetLastErrorMsg()` to retrieve the detailed error log from the compiler/linker.
 *
 * @note The caller is **responsible** for destroying the returned shader using `SituationUnloadShader()` to prevent GPU memory leaks.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationLoadShader(), SituationUnloadShader(), SituationCmdBindPipeline()
 */
SITAPI SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER) && defined(SITUATION_USE_VULKAN)
#if SIT_VK_SHADER_CACHE_PHASE2 && defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    _SitVkShaderBuildTicket* sync_ticket = NULL;
    bool sync_ticket_leader = false;
#endif
    // --- PATH A: Runtime Compilation (Shaderc) ---
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    uint64_t _layer_a_key = 0;
    {
        SituationError la_err = SITUATION_SUCCESS;
        int la_fast = _SitVkTryMeshLoadFromLayerA(slot, vs_code, fs_code, &_layer_a_key, &la_err);
        if (la_fast == 1) {
            *out_shader = handle;
            return SITUATION_SUCCESS;
        }
        if (la_fast < 0) {
            _SitFreeShaderSlot(handle);
            return la_err;
        }
    }
#if SIT_VK_SHADER_CACHE_PHASE2
    sync_ticket = _SitVkAcquireBuildTicket(&sit_render.vk.shader_cache, _layer_a_key, &sync_ticket_leader);
    if (sync_ticket && !sync_ticket_leader) {
        return _SitVkSyncLoadFromBuildTicketFollower(slot, &sit_render.vk.shader_cache, sync_ticket,
            _layer_a_key, handle, out_shader);
    }
#endif
#endif /* SIT_VK_SHADER_CACHE_ENABLE */

    _SituationSpirvBlob vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(vs_code, "vertex_shader", shaderc_glsl_vertex_shader);
    if (!vs_spirv.data) {
#if SIT_VK_SHADER_CACHE_PHASE2
        _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, sync_ticket, sync_ticket_leader, 0);
#endif
        _SituationFreeSpirvBlob(&vs_spirv);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    _SituationSpirvBlob fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(fs_code, "fragment_shader", shaderc_glsl_fragment_shader);
    if (!fs_spirv.data) {
#if SIT_VK_SHADER_CACHE_PHASE2
        _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, sync_ticket, sync_ticket_leader, 0);
#endif
        _SituationFreeSpirvBlob(&vs_spirv); // Clean up VS
        _SituationFreeSpirvBlob(&fs_spirv);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* [Shader Cache] Layer A insert: shaderc just produced fresh SPIR-V — populate the blob
     * cache so the next call with the same GLSL source skips shaderc entirely.
     * Idempotent: concurrent inserts of the same key are safe (insert checks for duplicate). */
    _SitVkInsertLayerA(_layer_a_key,
        vs_spirv.data, vs_spirv.size,
        fs_spirv.data, fs_spirv.size);
#if SIT_VK_SHADER_CACHE_PHASE2
    if (sync_ticket && sync_ticket_leader)
        atomic_store(&sync_ticket->phase, 2);
#endif
#endif

    #if defined(SITUATION_USE_VULKAN)
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
        /* [Shader Cache] Layer C check on compiled SPIR-V (Layer A miss / first load).
         * HIT: skip _SituationVulkanBuildGraphicsPipelinesOnSlot entirely. */
        {
            uint64_t vs_hash = _SitVkHashBytes64(vs_spirv.data, vs_spirv.size);
            uint64_t fs_hash = _SitVkHashBytes64(fs_spirv.data, fs_spirv.size);
            _SitVkShaderCacheKey cache_key;
            _SitVkFillCacheKey(&cache_key, vs_hash, fs_hash, (uint8_t)SIT_SPIRV_LAYOUT_PROFILE_MESH);
            _SitVkShaderCache* c = &sit_render.vk.shader_cache;
            uint64_t ck = vs_hash ^ (fs_hash << 1) ^ cache_key.layout_profile;

            mtx_lock(&c->mutex);
            _SitVkPipelineBundle* hit_bundle = _SitVkShaderCacheFindAndRefBundleLocked(
                c, &cache_key, ck, (uint32_t)sit_render.vk.current_frame_index);
            mtx_unlock(&c->mutex);

            if (hit_bundle) {
                _SitVkPinLastSpirvOnSlot(slot, vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size);
                _SituationFreeSpirvBlob(&vs_spirv);
                _SituationFreeSpirvBlob(&fs_spirv);
                _SitVkAttachBundleRef(slot, hit_bundle);
#if SIT_VK_SHADER_CACHE_PHASE2
                _SitVkFinishSyncBuildTicket(c, sync_ticket, sync_ticket_leader, 3);
#endif
                *out_shader = handle;
                return SITUATION_SUCCESS;
            }
        }
#endif /* SIT_VK_SHADER_CACHE_ENABLE */

#if SIT_VK_SHADER_CACHE_PHASE2
        /* Phase 2 first-load: bundle-only (1 pipeline + lazy variants) — skip legacy 12-pipeline build. */
        _SitVkTryAttachBundle(slot,
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            SIT_SPIRV_LAYOUT_PROFILE_MESH);
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            _SituationFreeSpirvBlob(&vs_spirv);
            _SituationFreeSpirvBlob(&fs_spirv);
            _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, sync_ticket, sync_ticket_leader, 3);
            *out_shader = handle;
            return SITUATION_SUCCESS;
        }
#endif

        /* Phase 6A: shared legacy fallback — only when bundle-only attach failed. */
        {
            SituationError pipe_err = _SituationVulkanBuildGraphicsPipelinesOnSlot(
                slot, vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size,
                SIT_SPIRV_LAYOUT_PROFILE_MESH);
            if (pipe_err != SITUATION_SUCCESS) {
#if SIT_VK_SHADER_CACHE_PHASE2
                _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, sync_ticket, sync_ticket_leader, 0);
#endif
                _SitFreeShaderSlot(handle);
                _SituationFreeSpirvBlob(&vs_spirv);
                _SituationFreeSpirvBlob(&fs_spirv);
                return pipe_err;
            }
#if SIT_VK_SHADER_CACHE_PHASE2
            _SitVkFinishSyncBuildTicket(&sit_render.vk.shader_cache, sync_ticket, sync_ticket_leader, 3);
#endif
        }
    #elif defined(SITUATION_USE_OPENGL)
        SituationError err;
        SituationSpirvBinary vs_bin = { vs_spirv.data, vs_spirv.size };
        SituationSpirvBinary fs_bin = { fs_spirv.data, fs_spirv.size };
        slot->gl_program_id = _SituationCreateGLShaderProgramFromSpirv(&vs_bin, &fs_bin, &err);
        if(err != SITUATION_SUCCESS) {
            _SitFreeShaderSlot(handle);
            _SituationFreeSpirvBlob(&vs_spirv);
            _SituationFreeSpirvBlob(&fs_spirv);
            return err;
        }
    #endif

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);
#else
    // --- PATH B: Legacy OpenGL GLSL Source Pipeline ---
    #if defined(SITUATION_USE_OPENGL)
#if SIT_GL_SHADER_CACHE_ENABLE
        {
            SituationError err = _SitGLLoadShaderProgramCached(slot, vs_code, fs_code);
            if (err != SITUATION_SUCCESS) {
                _SitFreeShaderSlot(handle);
                return err;
            }
        }
#else
        SituationError err;
        slot->gl_program_id = _SituationCreateGLShaderProgram(vs_code, fs_code, &err);
        if (err == SITUATION_SUCCESS) {
            slot->uniform_map = _sit_uniform_map_create();
            if (!slot->uniform_map) {
                _SitFreeShaderSlot(handle);
                return SITUATION_ERROR_MEMORY_ALLOCATION;
            }
        } else {
            _SitFreeShaderSlot(handle);
            return err;
        }
#endif
    #elif defined(SITUATION_USE_VULKAN)
        _SitFreeShaderSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Loading GLSL from memory requires the shader compiler to be enabled for the Vulkan backend.");
    #endif
#endif

    *out_shader = handle;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationBeginLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader) {
#if defined(SITUATION_USE_OPENGL)
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_code || !fs_code) return SITUATION_ERROR_INVALID_PARAM;

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    _SituationMakeGLContextCurrentForHostThread();
    SituationError err = SITUATION_SUCCESS;
    slot->gl_async_vs_shader = _SituationCompileGLShaderEx(vs_code, GL_VERTEX_SHADER, &err, false);
    if (err != SITUATION_SUCCESS || !slot->gl_async_vs_shader) {
        _SitFreeShaderSlot(handle);
        _SituationReleaseHostGLContextForRenderThread();
        return err != SITUATION_SUCCESS ? err : SITUATION_ERROR_OPENGL_SHADER_COMPILE;
    }
    slot->gl_async_fs_shader = _SituationCompileGLShaderEx(fs_code, GL_FRAGMENT_SHADER, &err, false);
    if (err != SITUATION_SUCCESS || !slot->gl_async_fs_shader) {
        if (slot->gl_async_vs_shader) glDeleteShader(slot->gl_async_vs_shader);
        _SitFreeShaderSlot(handle);
        _SituationReleaseHostGLContextForRenderThread();
        return err != SITUATION_SUCCESS ? err : SITUATION_ERROR_OPENGL_SHADER_COMPILE;
    }
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_COMPILE;
    *out_shader = handle;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER)
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_code || !fs_code) return SITUATION_ERROR_INVALID_PARAM;

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    {
        uint64_t layer_a_key = 0;
        SituationError la_err = SITUATION_SUCCESS;
        int la_fast = _SitVkTryMeshLoadFromLayerA(slot, vs_code, fs_code, &layer_a_key, &la_err);
        if (la_fast == 1) {
            *out_shader = handle;
            return SITUATION_SUCCESS;
        }
        if (la_fast < 0) {
            _SitFreeShaderSlot(handle);
            return la_err;
        }
    }
#endif

    _SituationVkAsyncShaderLoad* ctx = (_SituationVkAsyncShaderLoad*)SIT_MALLOC(sizeof(_SituationVkAsyncShaderLoad));
    if (!ctx) {
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    memset(ctx, 0, sizeof(*ctx));
    atomic_init(&ctx->compile_done, 0);
    ctx->layout_profile = SIT_SPIRV_LAYOUT_PROFILE_MESH;
    ctx->vs_src = _sit_strdup(vs_code);
    ctx->fs_src = _sit_strdup(fs_code);
    if (!ctx->vs_src || !ctx->fs_src) {
        SIT_FREE(ctx->vs_src);
        SIT_FREE(ctx->fs_src);
        SIT_FREE(ctx);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    slot->vk_async_load = ctx;

#if SIT_VK_SHADER_CACHE_PHASE2
    ctx->layer_a_key = _SitVkHashBytes64(vs_code, strlen(vs_code))
                     ^ (_SitVkHashBytes64(fs_code, strlen(fs_code)) << 1)
                     ^ _SitVkShadercOptionsFingerprint();
    bool ticket_leader = false;
    ctx->build_ticket = _SitVkAcquireBuildTicket(&sit_render.vk.shader_cache, ctx->layer_a_key, &ticket_leader);
    ctx->build_ticket_leader = ticket_leader;
    if (ctx->build_ticket && !ticket_leader) {
        *out_shader = handle;
        return SITUATION_SUCCESS;
    }
#endif

    if (sit_gs.thread_pool.is_active) {
        /* CPU-bound shaderc compile: high-priority worker queue (not I/O thread).
         * POINTER_ONLY + data_size 0: never SOO-copy ctx; RUN_IF_FULL avoids a stuck compile_done=0. */
        SituationJobId compile_job = SituationSubmitJobEx(
            &sit_gs.thread_pool, _SituationVkAsyncCompileWorker, ctx, 0,
            SIT_SUBMIT_BLOCK_IF_FULL | SIT_SUBMIT_POINTER_ONLY | SIT_SUBMIT_HIGH_PRIORITY
                | SIT_SUBMIT_RUN_IF_FULL);
        /* Safe to write after submit: the worker never reads these fields, and the
         * polling thread is this thread (poll happens on the main thread later). */
        ctx->compile_job = compile_job;
        ctx->submit_time_ns = _SitGetMonotonicTimeNS();
        if (compile_job == 0 && atomic_load(&ctx->compile_done) == 0) {
            _SituationVkAsyncCompileWorker(ctx, NULL);
        }
    } else {
        _SituationVkAsyncCompileWorker(ctx, NULL);
    }

    *out_shader = handle;
    return SITUATION_SUCCESS;
#else
    return SituationLoadShaderFromMemory(vs_code, fs_code, out_shader);
#endif
}

SITAPI SituationError SituationBeginLoadShaderFromSpirvMemoryEx(
    const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len,
    SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader) {
#if defined(SITUATION_USE_VULKAN)
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!vs_spirv || !fs_spirv || vs_len == 0 || fs_len == 0) return SITUATION_ERROR_INVALID_PARAM;
    if ((vs_len & 3u) != 0 || (fs_len & 3u) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SPIRV_INVALID, "SPIR-V bytecode size must be a multiple of 4.");
        return SITUATION_ERROR_VULKAN_SPIRV_INVALID;
    }
    if (layout_profile > SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex);
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    _SituationVkAsyncShaderLoad* ctx = (_SituationVkAsyncShaderLoad*)SIT_MALLOC(sizeof(_SituationVkAsyncShaderLoad));
    if (!ctx) {
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    memset(ctx, 0, sizeof(*ctx));
    atomic_init(&ctx->compile_done, 1);
    ctx->layout_profile = layout_profile;
    ctx->vs_spirv_copy = (uint8_t*)SIT_MALLOC(vs_len);
    ctx->fs_spirv_copy = (uint8_t*)SIT_MALLOC(fs_len);
    if (!ctx->vs_spirv_copy || !ctx->fs_spirv_copy) {
        SIT_FREE(ctx->vs_spirv_copy);
        SIT_FREE(ctx->fs_spirv_copy);
        SIT_FREE(ctx);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    memcpy(ctx->vs_spirv_copy, vs_spirv, vs_len);
    memcpy(ctx->fs_spirv_copy, fs_spirv, fs_len);
    ctx->vs_spirv_len = vs_len;
    ctx->fs_spirv_len = fs_len;
    slot->vk_async_load = ctx;
    *out_shader = handle;
    return SITUATION_SUCCESS;
#else
    return SituationLoadShaderFromSpirvMemoryEx(vs_spirv, vs_len, fs_spirv, fs_len, layout_profile, out_shader);
#endif
#elif defined(SITUATION_USE_OPENGL)
    (void)layout_profile;
    if (!out_shader) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!vs_spirv || !fs_spirv || vs_len == 0 || fs_len == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    return _SituationBeginGLSpirvShaderLoadAsync(vs_spirv, vs_len, fs_spirv, fs_len, out_shader);
#else
    (void)layout_profile;
    (void)vs_spirv;
    (void)vs_len;
    (void)fs_spirv;
    (void)fs_len;
    (void)out_shader;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationBeginLoadShaderFromSpirvMemory(
    const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader) {
    return SituationBeginLoadShaderFromSpirvMemoryEx(
        vs_spirv, vs_len, fs_spirv, fs_len, SIT_SPIRV_LAYOUT_PROFILE_MESH, out_shader);
}

SITAPI SituationError SituationPollShaderLoad(SituationShader shader) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || !slot->is_active) return SITUATION_ERROR_RESOURCE_INVALID;

    SIT_PROF_ZONE_CTX(sit_prof_poll, "PollShaderLoad");
#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    SituationError poll_err = _SituationPollGLAsyncShaderLoad(slot);
    if (poll_err != SITUATION_SUCCESS) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, poll_err);
    }
    poll_err = _SituationPollGLPendingProgramLink(slot);
    if (poll_err != SITUATION_SUCCESS) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, poll_err);
    }

    if (slot->gl_async_load_stage != SIT_GL_ASYNC_STAGE_IDLE || slot->gl_is_linking) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS);
    }
    if (slot->gl_async_vs_shader || slot->gl_async_fs_shader) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS);
    }
    if (slot->gl_program_id != 0) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_SUCCESS);
    }
    SituationError last = SituationGetLastErrorCode();
    if (last != SITUATION_SUCCESS && last != SITUATION_ERROR_UNKNOWN_ERROR) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, last);
    }
    SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_OPENGL_SHADER_LINK);
#elif defined(SITUATION_USE_VULKAN)
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    SituationError poll_err = _SituationPollVkAsyncShaderLoad(slot);
    if (poll_err != SITUATION_SUCCESS) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, poll_err);
    }
#endif
    if (slot->vk_async_load) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS);
    }
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_SUCCESS);
    }
#endif
    if (slot->vk_pipeline != VK_NULL_HANDLE) {
        SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_SUCCESS);
    }
    {
        SituationError last = SituationGetLastErrorCode();
        if (last != SITUATION_SUCCESS && last != SITUATION_ERROR_UNKNOWN_ERROR) {
            SIT_PROF_RETURN_CTX(sit_prof_poll, last);
        }
    }
    SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED);
#else
    SIT_PROF_RETURN_CTX(sit_prof_poll, SITUATION_ERROR_RESOURCE_INVALID);
#endif
}


/**
 * @brief Destroys a graphics shader pipeline and frees all of its associated GPU and CPU resources.
 * @details This is the only correct way to release a shader created with `SituationLoadShader` or `SituationLoadShaderFromMemory`. It handles the full cleanup process:
 *          1. Removes the shader from the internal resource tracking list to prevent false leak warnings at shutdown.
 *          2. Destroys all backend-specific GPU objects (OpenGL program, Vulkan pipeline and layout).
 *          3. Frees any associated CPU-side resources (like the OpenGL uniform location cache).
 *          4. Invalidates the user's handle by zeroing it out to prevent accidental use of stale data.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Destroys the internal uniform location cache (`_sit_uniform_map_destroy`), then deletes the shader program object (`glDeleteProgram`).
 * - **Vulkan:** Waits for the GPU to become idle to ensure the pipeline is not in use, then destroys both the `VkPipeline` and its associated `VkPipelineLayout`.
 *
 * @param[in,out] shader A pointer to the `SituationShader` handle to be destroyed. The contents of the struct will be zeroed out, invalidating the handle for future use.
 *
 * @note It is safe to call this function on a NULL pointer or an already-unloaded shader handle (where `shader->id` is 0); it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationUnloadShader(SituationShader* shader) {
    if (!shader) return;

    _SituationShaderSlot* slot = _SitGetShaderSlot(*shader);
    if (!slot) return;

    // --- Backend-Specific Destruction ---
#if defined(SITUATION_USE_OPENGL)
    if (slot->uniform_map) {
        _sit_uniform_map_destroy(slot->uniform_map);
        slot->uniform_map = NULL;
    }
    if (slot->gl_async_vs_shader) { glDeleteShader(slot->gl_async_vs_shader); slot->gl_async_vs_shader = 0; }
    if (slot->gl_async_fs_shader) { glDeleteShader(slot->gl_async_fs_shader); slot->gl_async_fs_shader = 0; }
    if (slot->gl_pending_program_id) { glDeleteProgram(slot->gl_pending_program_id); slot->gl_pending_program_id = 0; }
    _SituationGLFreeSpirvAsyncCopies(slot);
    slot->gl_is_linking = false;
    slot->gl_pending_link_spirv = false;
    slot->gl_async_load_stage = SIT_GL_ASYNC_STAGE_IDLE;
#if SIT_GL_SHADER_CACHE_ENABLE
    if (slot->gl_program_cache_ref.entry) {
        _SitGLProgramCacheRelease(&slot->gl_program_cache_ref,
            (uint32_t)sit_render.current_frame_index);
        slot->gl_program_id = 0;
    } else if (glIsProgram(slot->gl_program_id)) {
        glDeleteProgram(slot->gl_program_id);
        slot->gl_program_id = 0;
    }
#else
    if (glIsProgram(slot->gl_program_id)) {
        glDeleteProgram(slot->gl_program_id);
        slot->gl_program_id = 0;
    }
#endif
    SIT_CHECK_GL_ERROR();

#elif defined(SITUATION_USE_VULKAN)
    {
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
        _SituationVulkanFreeAsyncShaderLoad(slot);
#endif
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
        /* [Shader Cache Phase 1] Release cached bundle ref before per-slot pipeline destroy.
         * Slot vk_pipeline_* may be VK_NULL_HANDLE on repeat-load (cache-hit) path;
         * the existing defer-destroy loop is safe with NULL handles (guards are in place).
         * CRITICAL SAFETY: use _SitVkDerefBundle for live bundles — never read
         * vk_bundle_ref.bundle directly outside _SitVkAttachBundleRef or the two
         * intentional stale-ref NULL-clears (see _SitVkAttachBundleRef comment). */
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            _SitVkShaderCacheReleaseBundle(&sit_render.vk.shader_cache, &slot->vk_bundle_ref);
        } else if (slot->vk_bundle_ref.bundle) {
            /* Stale/evicted ref — generation mismatch or DESTROYED; clear without decrement. */
            slot->vk_bundle_ref.bundle     = NULL;
            slot->vk_bundle_ref.generation = 0;
        }
        slot->vk_bound_pipeline_cache = VK_NULL_HANDLE;
#endif
        _SitVkDestroyVDDynamicPipelinesOnSlot(slot);
        if (slot->vk_owned_last_vs_spirv) {
            SIT_FREE(slot->vk_owned_last_vs_spirv);
            slot->vk_owned_last_vs_spirv = NULL;
        }
        if (slot->vk_owned_last_fs_spirv) {
            SIT_FREE(slot->vk_owned_last_fs_spirv);
            slot->vk_owned_last_fs_spirv = NULL;
        }
        slot->vk_last_vs_spirv = NULL;
        slot->vk_last_fs_spirv = NULL;
        slot->vk_last_vs_size = 0;
        slot->vk_last_fs_size = 0;
        VkPipelineLayout layout_to_destroy = VK_NULL_HANDLE;
        if (slot->vk_owns_pipeline_layout) {
            layout_to_destroy = slot->vk_pipeline_layout;
        }
        if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE) {
            if (slot->vk_pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline, NULL);
                slot->vk_pipeline = VK_NULL_HANDLE;
            }
            if (layout_to_destroy != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(sit_render.vk.device, layout_to_destroy, NULL);
                slot->vk_pipeline_layout = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_legacy, NULL);
                slot->vk_pipeline_legacy = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_simple != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_simple, NULL);
                slot->vk_pipeline_simple = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_back_ccw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_back_ccw, NULL);
                slot->vk_pipeline_back_ccw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_back_cw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_back_cw, NULL);
                slot->vk_pipeline_back_cw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_legacy_back_ccw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_legacy_back_ccw, NULL);
                slot->vk_pipeline_legacy_back_ccw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_legacy_back_cw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_legacy_back_cw, NULL);
                slot->vk_pipeline_legacy_back_cw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_simple_back_ccw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_simple_back_ccw, NULL);
                slot->vk_pipeline_simple_back_ccw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_simple_back_cw != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_simple_back_cw, NULL);
                slot->vk_pipeline_simple_back_cw = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_line != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_line, NULL);
                slot->vk_pipeline_line = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_legacy_line != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_legacy_line, NULL);
                slot->vk_pipeline_legacy_line = VK_NULL_HANDLE;
            }
            if (slot->vk_pipeline_simple_line != VK_NULL_HANDLE) {
                vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_simple_line, NULL);
                slot->vk_pipeline_simple_line = VK_NULL_HANDLE;
            }
        } else {
            _SituationDeferDestroyPipeline(slot->vk_pipeline, layout_to_destroy);
            _SitVkDestroyVDDynamicPipelinesOnSlot(slot);
            if (slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_simple != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_simple, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_back_ccw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_back_ccw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_back_cw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_back_cw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_legacy_back_ccw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_ccw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_legacy_back_cw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_cw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_simple_back_ccw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_ccw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_simple_back_cw != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_cw, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_line != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_line, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_legacy_line != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_line, VK_NULL_HANDLE);
            }
            if (slot->vk_pipeline_simple_line != VK_NULL_HANDLE) {
                _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_line, VK_NULL_HANDLE);
            }
        }
        slot->vk_pipeline_layout = VK_NULL_HANDLE;
        slot->vk_owns_pipeline_layout = false;
    }
#endif

    _SitFreeShaderSlot(*shader);
    memset(shader, 0, sizeof(SituationShader));
}

#if defined(SITUATION_USE_OPENGL)
static size_t _sit_uniform_scalar_payload_bytes(SituationUniformType type) {
    switch (type) {
        case SIT_UNIFORM_FLOAT:
            return sizeof(GLfloat);
        case SIT_UNIFORM_VEC2:
            return 2u * sizeof(GLfloat);
        case SIT_UNIFORM_VEC3:
            return 3u * sizeof(GLfloat);
        case SIT_UNIFORM_VEC4:
            return 4u * sizeof(GLfloat);
        case SIT_UNIFORM_INT:
            return sizeof(GLint);
        case SIT_UNIFORM_IVEC2:
            return 2u * sizeof(GLint);
        case SIT_UNIFORM_IVEC3:
            return 3u * sizeof(GLint);
        case SIT_UNIFORM_IVEC4:
            return 4u * sizeof(GLint);
        case SIT_UNIFORM_MAT4:
            return 16u * sizeof(GLfloat);
        default:
            return 0;
    }
}

static SituationError _SitGLDeferProgramUniform(SituationGLSoftCommandBuffer* buf, GLuint prog, GLint loc, SituationUniformType type,
                                                int elem_count, const void* data, size_t payload_bytes) {
    if (!buf || !data || payload_bytes == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (elem_count < 1) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    size_t off_before = buf->data_cursor;
    void* blob = NULL;
    SIT_GL_SOFT_DATA_PUSH(buf, data, payload_bytes, blob);
    if (!blob) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_UNIFORM, p);
    if (!p) {
        buf->data_cursor = off_before;
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    p->args.set_uniform.shader_id = (uint64_t)prog;
    p->args.set_uniform.location = loc;
    p->args.set_uniform.type = (int)type;
    p->args.set_uniform.elem_count = elem_count;
    p->args.set_uniform.data_offset = off_before;
    return SITUATION_SUCCESS;
}
#endif /* SITUATION_USE_OPENGL */


/**
 * @brief Sets the value of a standalone uniform variable within a graphics shader program.
 * @details This function provides a convenient way to pass data to shaders, primarily for the OpenGL backend where standalone uniforms are common. It automatically caches uniform locations for high performance on subsequent calls.
 *
 * @par Backend-Specific Behavior & Usage
 * - **OpenGL:** This is the primary, high-performance method for setting per-draw-call data that is not part of a larger UBO.
 *      On the first call for a given `uniform_name`, it queries the uniform's location using `glGetUniformLocation` and caches it in an internal hash map. Subsequent calls for the same uniform are extremely fast as they use the cached location, avoiding repeated string lookups.
 *      It uses the appropriate `glUniform*` function based on the provided `type`.
 *
 * - **Vulkan:** This function is **not recommended** for the Vulkan backend and will return `SITUATION_ERROR_NOT_IMPLEMENTED`.
 *      Vulkan's architecture is optimized for passing data via Uniform Buffer Objects (UBOs) for per-frame data and **Push Constants** for small, high-frequency per-draw data. For Vulkan, you should use `SituationCmdBindUniformBuffer` and `SituationCmdSetPushConstant` instead.
 *
 * @param shader The `SituationShader` handle whose uniform you want to set.
 * @param uniform_name The null-terminated string name of the uniform variable in the GLSL code (e.g., "u_modelMatrix").
 * @param data A pointer to the data to be sent to the uniform (e.g., a `mat4`, `vec4`, `float`).
 * @param type An enum `SituationUniformType` that specifies the data type of the uniform. This determines which underlying API function is called.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_NOT_INITIALIZED` if the library is not initialized.
 * @return `SITUATION_ERROR_INVALID_PARAM` if any of the input parameters are invalid.
 * @return `SITUATION_ERROR_NOT_IMPLEMENTED` if called on the Vulkan backend.
 * @return `SITUATION_ERROR_OPENGL_GENERAL` if an OpenGL error occurs.
 *
 * @note In OpenGL, if the specified `uniform_name` does not exist in the shader or is optimized out by the compiler, this function will silently do nothing and return `SITUATION_SUCCESS`. This is standard behavior for `glGetUniformLocation`.
 *
 * @note **Render thread (`SITUATION_ENABLE_RENDER_THREAD`, `render_thread_count > 0`):** the main thread does not
 *       have the window GL context current during the frame. Standalone `glProgramUniform` calls would not apply
 *       reliably. While `SituationAcquireFrameCommandBuffer` is active, this function **records** `SIT_OP_SET_UNIFORM`
 *       into the main soft buffer so uploads run on the render thread during `_SituationGLExecuteCommands`.
 *
 * @note **OpenGL (any build):** while `SituationAcquireFrameCommandBuffer` is active (`sit_render.in_frame`), uploads
 *       are also **deferred** into the soft buffer (unless the GL render-thread path already handled the call) so they
 *       execute with the rest of the frame’s GL commands—avoiding ordering issues with `SituationCmdBeginRenderPass`
 *       viewport and user draws.
 *
 * @see SituationCmdSetPushConstant(), SituationCmdBindUniformBuffer(), SituationSetShaderUniform1iv()
 */
#if defined(SITUATION_USE_OPENGL)
static SituationError _SituationSetShaderUniformLocationImpl(_SituationShaderSlot* slot, GLint location, const void* data, SituationUniformType type);
#endif

SITAPI SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || !uniform_name || !data) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

#if defined(SITUATION_USE_OPENGL)
    if (!slot->uniform_map) {
        // Map should have been created on load. If missing, create now (lazy).
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // 1. Get Location (Cached)
    GLint location = _sit_uniform_map_get(slot->uniform_map, uniform_name);
    if (location == -1) {
        // Not in cache. Query driver.
        // We must bind to query? No, glGetUniformLocation takes program ID.
        // BUT we should avoid binding just to query if possible, but here we can query directly.
        // Optimization: track current program to avoid redundant state check?
        // Let's just query.

        // Safety: Ensure program exists
        if (!glIsProgram(slot->gl_program_id)) return SITUATION_ERROR_RESOURCE_INVALID;

        _SituationMakeGLContextCurrentForHostThread();
        location = glGetUniformLocation(slot->gl_program_id, uniform_name);
        _SituationReleaseHostGLContextIfInFrame();

        // Cache result (even -1, to avoid re-querying invalid uniforms?)
        // Map stores -1? Yes.
        if (location != -1) {
            SituationError set_err = _sit_uniform_map_set(slot->uniform_map, uniform_name, location);
            if (set_err != SITUATION_SUCCESS) {
                return set_err;
            }
        } else {
             // Optional: Cache miss to avoid spamming driver for typo'd uniforms?
             // For now, don't cache failures to save memory/logic.
             return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }

    return _SituationSetShaderUniformLocationImpl(slot, location, data, type);
#else
    (void)shader;
    (void)uniform_name;
    (void)data;
    (void)type;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationSetShaderUniformLocation(SituationShader shader, int location, const void* data, SituationUniformType type) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || !data) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    return _SituationSetShaderUniformLocationImpl(slot, location, data, type);
#else
    (void)location;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

#if defined(SITUATION_USE_OPENGL)
static SituationError _SituationSetShaderUniformLocationImpl(_SituationShaderSlot* slot, GLint location, const void* data, SituationUniformType type) {
    if (!slot || !data || location < 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    GLuint prog = slot->gl_program_id;
    if (!glIsProgram(prog)) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        if (!sit_render.in_frame) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER,
                                            "SituationSetShaderUniform: render thread owns GL; call only between SituationAcquireFrameCommandBuffer and SituationEndFrame.");
        }
        size_t payload = _sit_uniform_scalar_payload_bytes(type);
        if (payload == 0) {
            return SITUATION_ERROR_INVALID_PARAM;
        }
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, type, 1, data, payload);
    }
#endif

    if (sit_render.in_frame) {
        size_t payload = _sit_uniform_scalar_payload_bytes(type);
        if (payload == 0) {
            return SITUATION_ERROR_INVALID_PARAM;
        }
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, type, 1, data, payload);
    }

    switch (type) {
        case SIT_UNIFORM_FLOAT:
            glProgramUniform1fv(prog, location, 1, (const GLfloat*)data);
            break;
        case SIT_UNIFORM_VEC2:
            glProgramUniform2fv(prog, location, 1, (const GLfloat*)data);
            break;
        case SIT_UNIFORM_VEC3:
            glProgramUniform3fv(prog, location, 1, (const GLfloat*)data);
            break;
        case SIT_UNIFORM_VEC4:
            glProgramUniform4fv(prog, location, 1, (const GLfloat*)data);
            break;
        case SIT_UNIFORM_INT:
            glProgramUniform1iv(prog, location, 1, (const GLint*)data);
            break;
        case SIT_UNIFORM_IVEC2:
            glProgramUniform2iv(prog, location, 1, (const GLint*)data);
            break;
        case SIT_UNIFORM_IVEC3:
            glProgramUniform3iv(prog, location, 1, (const GLint*)data);
            break;
        case SIT_UNIFORM_IVEC4:
            glProgramUniform4iv(prog, location, 1, (const GLint*)data);
            break;
        case SIT_UNIFORM_MAT4:
            glProgramUniformMatrix4fv(prog, location, 1, GL_FALSE, (const GLfloat*)data);
            break;
        default:
            return SITUATION_ERROR_INVALID_PARAM;
    }
    return SITUATION_SUCCESS;
}
#endif /* SITUATION_USE_OPENGL */

SITAPI SituationError SituationSetShaderUniform1iv(SituationShader shader, const char* uniform_name, int count, const int* values) {
#if !defined(SITUATION_USE_OPENGL)
    (void)shader;
    (void)uniform_name;
    (void)count;
    (void)values;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#else
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (count < 1 || !values || !uniform_name) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    if (!slot->uniform_map) {
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }

    GLint location = _sit_uniform_map_get(slot->uniform_map, uniform_name);
    if (location == -1) {
        if (!glIsProgram(slot->gl_program_id)) {
            return SITUATION_ERROR_RESOURCE_INVALID;
        }
        location = glGetUniformLocation(slot->gl_program_id, uniform_name);
        if (location == -1) {
            const char* bracket = strchr(uniform_name, '[');
            if (bracket && bracket != uniform_name) {
                char alt[96];
                size_t base_len = (size_t)(bracket - uniform_name);
                if (base_len < sizeof(alt)) {
                    memcpy(alt, uniform_name, base_len);
                    alt[base_len] = '\0';
                    location = glGetUniformLocation(slot->gl_program_id, alt);
                }
            }
        }
        if (location != -1) {
            SituationError set_err = _sit_uniform_map_set(slot->uniform_map, uniform_name, location);
            if (set_err != SITUATION_SUCCESS) {
                return set_err;
            }
        } else {
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }

    GLuint prog = slot->gl_program_id;
    size_t payload = (size_t)count * sizeof(GLint);

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        if (!sit_render.in_frame) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER,
                                            "SituationSetShaderUniform1iv: render thread owns GL; call only between SituationAcquireFrameCommandBuffer and SituationEndFrame.");
        }
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_INT, count, values, payload);
    }
#endif

    if (sit_render.in_frame) {
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_INT, count, values, payload);
    }

    glProgramUniform1iv(prog, location, count, values);
    return SITUATION_SUCCESS;
#endif
}

SITAPI SituationError SituationSetShaderUniform1fv(SituationShader shader, const char* uniform_name, int count, const float* values) {
#if !defined(SITUATION_USE_OPENGL)
    (void)shader;
    (void)uniform_name;
    (void)count;
    (void)values;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#else
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (count < 1 || !values || !uniform_name) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) return SITUATION_ERROR_INVALID_PARAM;

    if (!slot->uniform_map) {
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    GLint location = _sit_uniform_map_get(slot->uniform_map, uniform_name);
    if (location == -1) {
        if (!glIsProgram(slot->gl_program_id)) return SITUATION_ERROR_RESOURCE_INVALID;
        location = glGetUniformLocation(slot->gl_program_id, uniform_name);
        if (location == -1) {
            const char* bracket = strchr(uniform_name, '[');
            if (bracket && bracket != uniform_name) {
                char alt[96];
                size_t base_len = (size_t)(bracket - uniform_name);
                if (base_len < sizeof(alt)) {
                    memcpy(alt, uniform_name, base_len);
                    alt[base_len] = '\0';
                    location = glGetUniformLocation(slot->gl_program_id, alt);
                }
            }
        }
        if (location != -1) {
            SituationError set_err = _sit_uniform_map_set(slot->uniform_map, uniform_name, location);
            if (set_err != SITUATION_SUCCESS) {
                return set_err;
            }
        } else {
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }

    GLuint prog = slot->gl_program_id;
    size_t payload = (size_t)count * sizeof(GLfloat);

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        if (!sit_render.in_frame) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER,
                                            "SituationSetShaderUniform1fv: render thread owns GL; call only between SituationAcquireFrameCommandBuffer and SituationEndFrame.");
        }
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_FLOAT, count, values, payload);
    }
#endif

    if (sit_render.in_frame) {
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_FLOAT, count, values, payload);
    }

    glProgramUniform1fv(prog, location, count, values);
    return SITUATION_SUCCESS;
#endif
}

SITAPI SituationError SituationSetShaderUniformMatrix4fv(SituationShader shader, const char* uniform_name, int count, const mat4* matrices) {
#if !defined(SITUATION_USE_OPENGL)
    (void)shader;
    (void)uniform_name;
    (void)count;
    (void)matrices;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#else
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (count < 1 || !matrices || !uniform_name) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) return SITUATION_ERROR_INVALID_PARAM;

    if (!slot->uniform_map) {
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    GLint location = _sit_uniform_map_get(slot->uniform_map, uniform_name);
    if (location == -1) {
        if (!glIsProgram(slot->gl_program_id)) return SITUATION_ERROR_RESOURCE_INVALID;
        location = glGetUniformLocation(slot->gl_program_id, uniform_name);
        if (location == -1) {
            const char* bracket = strchr(uniform_name, '[');
            if (bracket && bracket != uniform_name) {
                char alt[96];
                size_t base_len = (size_t)(bracket - uniform_name);
                if (base_len < sizeof(alt)) {
                    memcpy(alt, uniform_name, base_len);
                    alt[base_len] = '\0';
                    location = glGetUniformLocation(slot->gl_program_id, alt);
                }
            }
        }
        if (location != -1) {
            SituationError set_err = _sit_uniform_map_set(slot->uniform_map, uniform_name, location);
            if (set_err != SITUATION_SUCCESS) {
                return set_err;
            }
        } else {
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }

    GLuint prog = slot->gl_program_id;
    size_t payload = (size_t)count * sizeof(mat4);

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        if (!sit_render.in_frame) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER,
                                            "SituationSetShaderUniformMatrix4fv: render thread owns GL; call only between SituationAcquireFrameCommandBuffer and SituationEndFrame.");
        }
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_MAT4, count, matrices, payload);
    }
#endif

    if (sit_render.in_frame) {
        SituationGLSoftCommandBuffer* gbuf = &sit_render.gl.soft_buffers[sit_render.current_frame_index];
        return _SitGLDeferProgramUniform(gbuf, prog, location, SIT_UNIFORM_MAT4, count, matrices, payload);
    }

    glProgramUniformMatrix4fv(prog, location, count, GL_FALSE, (const GLfloat*)matrices);
    return SITUATION_SUCCESS;
#endif
}

SITAPI SituationError SituationValidateShaderUniforms(SituationShader shader, const SituationUniformExpectation* table, int table_count, char* error_buf, size_t error_buf_size) {
#if !defined(SITUATION_USE_OPENGL)
    (void)shader;
    (void)table;
    (void)table_count;
    if (error_buf && error_buf_size > 0) error_buf[0] = '\0';
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#else
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (error_buf && error_buf_size > 0) error_buf[0] = '\0';
    if (!table || table_count <= 0) return SITUATION_ERROR_INVALID_PARAM;
    
    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || !glIsProgram(slot->gl_program_id)) return SITUATION_ERROR_INVALID_PARAM;
    
    GLuint prog = slot->gl_program_id;
    GLint active_uniforms = 0;
    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &active_uniforms);
    
    for (int i = 0; i < table_count; i++) {
        const SituationUniformExpectation* exp = &table[i];
        GLint loc = glGetUniformLocation(prog, exp->name);
        
        char expected_name[256];
        strncpy(expected_name, exp->name, sizeof(expected_name)-1);
        expected_name[sizeof(expected_name)-1] = '\0';
        char* exp_bracket = strchr(expected_name, '[');
        if (exp_bracket) *exp_bracket = '\0';
        
        bool found_match = false;
        for (GLuint u = 0; u < (GLuint)active_uniforms; u++) {
            char name[256];
            GLsizei length;
            GLint size;
            GLenum type;
            glGetActiveUniform(prog, u, sizeof(name), &length, &size, &type, name);
            
            char* bracket = strchr(name, '[');
            if (bracket) *bracket = '\0';
            
            if (strcmp(name, expected_name) == 0) {
                found_match = true;
                bool type_ok = true;
                if (exp->type == SIT_UNIFORM_FLOAT && type != GL_FLOAT) type_ok = false;
                else if (exp->type == SIT_UNIFORM_VEC2 && type != GL_FLOAT_VEC2) type_ok = false;
                else if (exp->type == SIT_UNIFORM_VEC3 && type != GL_FLOAT_VEC3) type_ok = false;
                else if (exp->type == SIT_UNIFORM_VEC4 && type != GL_FLOAT_VEC4) type_ok = false;
                else if (exp->type == SIT_UNIFORM_INT && (type != GL_INT && type != GL_SAMPLER_2D && type != GL_SAMPLER_CUBE && type != GL_SAMPLER_2D_SHADOW)) type_ok = false;
                else if (exp->type == SIT_UNIFORM_MAT4 && type != GL_FLOAT_MAT4) type_ok = false;
                
                if (!type_ok) {
                    if (error_buf && error_buf_size > 0) {
                        snprintf(error_buf, error_buf_size, "Type mismatch for %s", exp->name);
                    }
                    return SITUATION_ERROR_INVALID_PARAM;
                }
                
                if (exp->array_length > 0 && size < exp->array_length) {
                    if (error_buf && error_buf_size > 0) {
                        snprintf(error_buf, error_buf_size, "Array size mismatch for %s (expected %d, got %d)", exp->name, exp->array_length, size);
                    }
                    return SITUATION_ERROR_INVALID_PARAM;
                }
                break;
            }
        }
        
        if (!found_match && loc == -1) {
            if (error_buf && error_buf_size > 0) {
                snprintf(error_buf, error_buf_size, "Missing uniform: %s", exp->name);
            }
            return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }
    return SITUATION_SUCCESS;
#endif
}

SITAPI SituationError SituationQueryShaderStorageBlocks(SituationShader shader, SituationShaderStorageBlockInfo* out_blocks, int capacity, int* out_count) {
#if !defined(SITUATION_USE_OPENGL)
    (void)shader;
    (void)out_blocks;
    (void)capacity;
    if (out_count) *out_count = 0;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#else
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || slot->gl_program_id == 0) return SITUATION_ERROR_INVALID_PARAM;

    _SituationMakeGLContextCurrentForHostThread();
    GLuint program = slot->gl_program_id;

    GLint ssbo_count = 0;
    glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &ssbo_count);
    if (out_count) {
        *out_count = (int)ssbo_count;
    }

    if (!out_blocks || capacity <= 0) {
        return SITUATION_SUCCESS; /* count-only query */
    }

    int fill = ssbo_count < capacity ? ssbo_count : capacity;
    for (int i = 0; i < fill; i++) {
        SituationShaderStorageBlockInfo* info = &out_blocks[i];
        memset(info, 0, sizeof(*info));
        info->block_index = (uint32_t)i;

        /* Query the assigned binding point (post _SituationBindGLProgramStorageBlocks). */
        GLenum binding_prop = GL_BUFFER_BINDING;
        GLint binding_val = -1;
        glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, (GLuint)i,
                               1, &binding_prop, 1, NULL, &binding_val);
        info->binding_point = (binding_val >= 0) ? (uint32_t)binding_val : (uint32_t)i;

        /* Query block name (best-effort; may be empty for anonymous SPIR-V blocks). */
        GLsizei name_len = 0;
        glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, (GLuint)i,
                                 (GLsizei)(sizeof(info->name) - 1), &name_len, info->name);
        info->name[name_len] = '\0';
    }

    return SITUATION_SUCCESS;
#endif
}


/**
 * @brief [DEPRECATED] Inserts a coarse-grained memory barrier.
 * @details This function provides a simple, but less optimal, way to synchronize memory.
 *          It is recommended to use `SituationCmdPipelineBarrierEx` or `SituationCmdBufferBarrier` for new synchronization code.
 *
 * @param cmd The command buffer to record the barrier into. (Ignored in OpenGL).
 * @param barrier_bits A bitmask of `SITUATION_BARRIER_*_BIT` flags specifying the types of memory access to synchronize.
 *
 * @deprecated Use SituationCmdPipelineBarrierEx() or SituationCmdBufferBarrier() for more precise synchronization.
 */
SITAPI void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits) {
    if (!SituationIsInitialized() || barrier_bits == 0) {
        return;
    }

#if defined(SITUATION_USE_OPENGL)
    (void)cmd;
    // The previous mapping logic for OpenGL is still valid for this coarse barrier.
    GLbitfield gl_barrier_bits = 0;
    if (barrier_bits & SITUATION_BARRIER_VERTEX_ATTRIB_ARRAY_BIT)  gl_barrier_bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_INDEX_BUFFER_BIT)         gl_barrier_bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_UNIFORM_BUFFER_BIT)       gl_barrier_bits |= GL_UNIFORM_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_TEXTURE_FETCH_BIT)        gl_barrier_bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BIT)  gl_barrier_bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_COMMAND_BIT)              gl_barrier_bits |= GL_COMMAND_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_SHADER_STORAGE_BIT)       gl_barrier_bits |= GL_SHADER_STORAGE_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_ALL_BARRIER_BITS)         gl_barrier_bits = GL_ALL_BARRIER_BITS;

    if (gl_barrier_bits != 0) {
        glMemoryBarrier(gl_barrier_bits);
    }
#elif defined(SITUATION_USE_VULKAN)
    // For this deprecated function, we issue a very broad, "sledgehammer" barrier.
    // It's not optimal but guarantees correctness for simple use cases.
    VkMemoryBarrier memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(
        (VkCommandBuffer)cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Wait for ALL previous stages
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Unblock ALL subsequent stages
        0,
        1, &memory_barrier,
        0, NULL,
        0, NULL
    );
#endif
}

// ============================================================================
// Hot Reloading Implementation
// ============================================================================
/**
 * @section Hot-Reloading Overview
 * These functions allow applications to reload assets (Shaders, Textures, Models) from disk at runtime
 * without restarting the application. This is intended primarily for development, tooling, and
 * "creative coding" workflows.
 *
 * @note **Performance (Vulkan):** Reloading is asynchronous and safe. Resources are defer-destroyed.
 * @warning **Performance (OpenGL):** Reloading may cause a brief stall (`glFinish`).
 *
 * @note **Path Tracking:** Hot-reloading only works for assets loaded from files using the high-level
 * `SituationLoad...` functions. Assets created from raw memory pointers cannot be hot-reloaded as
 * they have no associated file path.
 */

 /**
 * @brief Reloads a graphics shader pipeline from its original source files.
 *
 * @details This function looks up the original file paths used to create the shader, waits for the GPU to become idle,
 *          destroys the existing pipeline resources, and attempts to compile and link a new pipeline from disk.
 *
 *          If successful, the `shader` handle is updated in-place with the new ID.
 *          If failure occurs (e.g., compilation error in the new code), the old shader is destroyed, and the handle
 *          becomes invalid (ID = 0). The application should check the return value and handle invalidation gracefully.
 *
 * @param[in,out] shader A pointer to the `SituationShader` handle to reload.
 *                       On success, this struct is updated with the new resource IDs.
 *                       On failure, this struct is zeroed out.
 *
 * @return `true` if the shader was successfully recompiled and linked.
 * @return `false` if the shader could not be reloaded (e.g., file not found, GLSL syntax error).
 *         Check `SituationGetLastErrorMsg()` for compiler errors.
 */
SITAPI SituationError SituationReloadShader(SituationShader* shader) {
    if (!SituationIsInitialized() || !shader) return SITUATION_ERROR_INVALID_PARAM;
    _SituationShaderSlot* slot = _SitGetShaderSlot(*shader);
    if (!slot || !slot->vs_path || !slot->fs_path) return SITUATION_ERROR_INVALID_PARAM;

    // Reload
    char* vs = SituationLoadFileText(slot->vs_path);
    char* fs = SituationLoadFileText(slot->fs_path);
    if (!vs || !fs) { SIT_FREE(vs); SIT_FREE(fs); return SITUATION_ERROR_FILE_NOT_FOUND; }

#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER) \
    && defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* [Shader Cache Phase 3] In-place bundle swap.
     * Compile GLSL → SPIR-V, hash it, and compare against the current bundle's content_hash.
     * If the SPIR-V didn't change (content_hash match), skip all GPU work — just refresh mod times. */
    {
        _SituationSpirvBlob vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(vs, "reload_vs", shaderc_glsl_vertex_shader);
        _SituationSpirvBlob fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(fs, "reload_fs", shaderc_glsl_fragment_shader);
        SIT_FREE(vs); SIT_FREE(fs);

        if (!vs_spirv.data || !fs_spirv.data) {
            _SituationFreeSpirvBlob(&vs_spirv);
            _SituationFreeSpirvBlob(&fs_spirv);
            return _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED,
                "SituationReloadShader: GLSL compilation failed.");
        }

        uint64_t new_vs_hash = _SitVkHashBytes64(vs_spirv.data, vs_spirv.size);
        uint64_t new_fs_hash = _SitVkHashBytes64(fs_spirv.data, fs_spirv.size);
        uint64_t new_content_hash = new_vs_hash ^ new_fs_hash;

        /* Check current bundle content_hash for O(1) no-op detection. */
        _SitVkPipelineBundle* cur_bundle = _SitVkDerefBundle(&slot->vk_bundle_ref);
        if (cur_bundle && cur_bundle->content_hash == new_content_hash) {
            /* SPIR-V unchanged — no GPU work needed. Refresh mod times and Layer A. */
            _SitVkInsertLayerA(
                _SitVkHashBytes64(vs_spirv.data, vs_spirv.size) ^ /* reuse new hashes */
                    (_SitVkHashBytes64(fs_spirv.data, fs_spirv.size) << 1) ^
                    _SitVkShadercOptionsFingerprint(),
                vs_spirv.data, vs_spirv.size,
                fs_spirv.data, fs_spirv.size);
            _SituationFreeSpirvBlob(&vs_spirv);
            _SituationFreeSpirvBlob(&fs_spirv);
            slot->vs_mod_time = SituationGetFileModTime(slot->vs_path);
            slot->fs_mod_time = SituationGetFileModTime(slot->fs_path);
            return SITUATION_SUCCESS;
        }

        /* SPIR-V changed — acquire new bundle (reuses modules if same hash seen before). */
        uint64_t la_key = new_vs_hash ^ (new_fs_hash << 1) ^ _SitVkShadercOptionsFingerprint();
        _SitVkInsertLayerA(la_key, vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size);

        _SitVkShaderCacheKey new_key;
        _SitVkFillCacheKey(&new_key, new_vs_hash, new_fs_hash, SIT_SPIRV_LAYOUT_PROFILE_MESH);

        _SitVkModulePairEntry* modules = _SitVkShaderCacheAcquireModules(
            &sit_render.vk.shader_cache,
            new_vs_hash, new_fs_hash,
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size);
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);

        if (!modules) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED,
                "SituationReloadShader: module acquire failed.");
        }

        _SitVkPipelineBundle* new_bundle = _SitVkShaderCacheAcquireBundle(
            &sit_render.vk.shader_cache, &new_key, modules,
            (uint32_t)sit_render.vk.current_frame_index);
        if (!new_bundle) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
                "SituationReloadShader: bundle acquire failed.");
        }

        /* Release old bundle ref; defer-destroy any slot-owned pipelines from Phase 1 legacy path.
         * THREADING ASSUMPTION: SituationReloadShader must be called from the main thread between
         * frames — i.e. no command buffer is currently recording against this slot's pipeline.
         * After this release, slot->vk_pipeline_layout briefly holds the old bundle's layout
         * (vk_owns_pipeline_layout = false, so it is not destroyed here). It will be overwritten
         * by _SitVkAttachBundleRef below before any subsequent SituationCmdBindPipeline call,
         * which is safe only because no frame recording is in flight at reload time. */
        if (_SitVkDerefBundle(&slot->vk_bundle_ref)) {
            _SitVkShaderCacheReleaseBundle(&sit_render.vk.shader_cache, &slot->vk_bundle_ref);
        } else if (slot->vk_bundle_ref.bundle) {
            /* Stale/evicted ref — generation mismatch or DESTROYED; clear without decrement. */
            slot->vk_bundle_ref.bundle = NULL;
            slot->vk_bundle_ref.generation = 0;
        }

        /* Defer-destroy any slot-owned pipeline handles (first-load path leftovers). */
        if (slot->vk_pipeline != VK_NULL_HANDLE) {
            VkPipelineLayout lo = slot->vk_owns_pipeline_layout ? slot->vk_pipeline_layout : VK_NULL_HANDLE;
            _SituationDeferDestroyPipeline(slot->vk_pipeline, lo);
            slot->vk_pipeline = VK_NULL_HANDLE;
            slot->vk_pipeline_layout = VK_NULL_HANDLE;
            slot->vk_owns_pipeline_layout = false;
        }
        if (slot->vk_pipeline_legacy != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy, VK_NULL_HANDLE); slot->vk_pipeline_legacy = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_simple != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_simple, VK_NULL_HANDLE); slot->vk_pipeline_simple = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_back_ccw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_back_ccw, VK_NULL_HANDLE); slot->vk_pipeline_back_ccw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_back_cw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_back_cw, VK_NULL_HANDLE); slot->vk_pipeline_back_cw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_legacy_back_ccw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_ccw, VK_NULL_HANDLE); slot->vk_pipeline_legacy_back_ccw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_legacy_back_cw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_cw, VK_NULL_HANDLE); slot->vk_pipeline_legacy_back_cw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_simple_back_ccw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_ccw, VK_NULL_HANDLE); slot->vk_pipeline_simple_back_ccw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_simple_back_cw != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_cw, VK_NULL_HANDLE); slot->vk_pipeline_simple_back_cw = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_line != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_line, VK_NULL_HANDLE); slot->vk_pipeline_line = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_legacy_line != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_line, VK_NULL_HANDLE); slot->vk_pipeline_legacy_line = VK_NULL_HANDLE; }
        if (slot->vk_pipeline_simple_line != VK_NULL_HANDLE) { _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_line, VK_NULL_HANDLE); slot->vk_pipeline_simple_line = VK_NULL_HANDLE; }

        _SitVkAttachBundleRef(slot, new_bundle);
        slot->vs_mod_time = SituationGetFileModTime(slot->vs_path);
        slot->fs_mod_time = SituationGetFileModTime(slot->fs_path);
        return SITUATION_SUCCESS;
    }
#else
    /* Non-cache path (OpenGL or Vulkan without shader cache): swap via temp slot. */
    SituationShader new_handle;
    SituationError err = SituationLoadShaderFromMemory(vs, fs, &new_handle);
    SIT_FREE(vs); SIT_FREE(fs);

    if (err == SITUATION_SUCCESS) {
        _SituationShaderSlot* new_slot = _SitGetShaderSlot(new_handle);
        if (new_slot) {
            #if defined(SITUATION_USE_OPENGL)
            if (glIsProgram(slot->gl_program_id)) glDeleteProgram(slot->gl_program_id);
            slot->gl_program_id = new_slot->gl_program_id;
            if (slot->uniform_map) _sit_uniform_map_destroy(slot->uniform_map);
            slot->uniform_map = new_slot->uniform_map;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyPipeline(slot->vk_pipeline, slot->vk_pipeline_layout);
            if (slot->vk_pipeline_legacy) _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy, VK_NULL_HANDLE);
            if (slot->vk_pipeline_simple) _SituationDeferDestroyPipeline(slot->vk_pipeline_simple, VK_NULL_HANDLE);
            if (slot->vk_pipeline_back_ccw) _SituationDeferDestroyPipeline(slot->vk_pipeline_back_ccw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_back_cw) _SituationDeferDestroyPipeline(slot->vk_pipeline_back_cw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_legacy_back_ccw) _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_ccw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_legacy_back_cw) _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_back_cw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_simple_back_ccw) _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_ccw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_simple_back_cw) _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_back_cw, VK_NULL_HANDLE);
            if (slot->vk_pipeline_line) _SituationDeferDestroyPipeline(slot->vk_pipeline_line, VK_NULL_HANDLE);
            if (slot->vk_pipeline_legacy_line) _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy_line, VK_NULL_HANDLE);
            if (slot->vk_pipeline_simple_line) _SituationDeferDestroyPipeline(slot->vk_pipeline_simple_line, VK_NULL_HANDLE);
            slot->vk_pipeline = new_slot->vk_pipeline;
            slot->vk_pipeline_legacy = new_slot->vk_pipeline_legacy;
            slot->vk_pipeline_simple = new_slot->vk_pipeline_simple;
            slot->vk_pipeline_back_ccw = new_slot->vk_pipeline_back_ccw;
            slot->vk_pipeline_back_cw = new_slot->vk_pipeline_back_cw;
            slot->vk_pipeline_legacy_back_ccw = new_slot->vk_pipeline_legacy_back_ccw;
            slot->vk_pipeline_legacy_back_cw = new_slot->vk_pipeline_legacy_back_cw;
            slot->vk_pipeline_simple_back_ccw = new_slot->vk_pipeline_simple_back_ccw;
            slot->vk_pipeline_simple_back_cw = new_slot->vk_pipeline_simple_back_cw;
            slot->vk_pipeline_line = new_slot->vk_pipeline_line;
            slot->vk_pipeline_legacy_line = new_slot->vk_pipeline_legacy_line;
            slot->vk_pipeline_simple_line = new_slot->vk_pipeline_simple_line;
            slot->vk_pipeline_layout = new_slot->vk_pipeline_layout;
            #endif

            slot->vs_mod_time = SituationGetFileModTime(slot->vs_path);
            slot->fs_mod_time = SituationGetFileModTime(slot->fs_path);

            new_slot->is_active = false; // Recycle new slot
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,
            "SituationReloadShader: new slot not accessible after successful load (registry defect)");
    }
    return err;
#endif
}

#endif // SITUATION_IMPL_RENDERER_SHADER_H
