/***************************************************************************************************
*
*   sit/aud/graph_serialization_impl.h - Audio Graph Serialization Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of JSON serialization for audio graphs.
*   
*   JSON Format:
*   {
*     "version": "2.4.0",
*     "sample_rate": 48000,
*     "nodes": [
*       {
*         "id": 1,
*         "type": "Reverb",
*         "active": true,
*         "controls": {
*           "room_size": 0.8,
*           "damp": 0.5
*         }
*       }
*     ],
*     "patches": [
*       {
*         "src_node": 1,
*         "src_port": 0,
*         "dst_node": 2,
*         "dst_port": 0,
*         "type": "audio"
*       }
*     ]
*   }
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_SERIALIZATION_IMPL_H
#define SITUATION_NODE_GRAPH_SERIALIZATION_IMPL_H

#include "node_graph_serialization.h"
#include "node_graph_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Memory macros (avoid full situation_api.h dependency)
#ifndef SIT_MALLOC
#define SIT_MALLOC(sz) malloc(sz)
#endif
#ifndef SIT_CALLOC
#define SIT_CALLOC(n, sz) calloc(n, sz)
#endif
#ifndef SIT_REALLOC
#define SIT_REALLOC(p, sz) realloc(p, sz)
#endif
#ifndef SIT_FREE
#define SIT_FREE(p) free(p)
#endif

#define SITUATION_SERIALIZATION_VERSION "2.4.0"
#define SITUATION_JSON_BUFFER_SIZE 65536  // 64KB initial buffer

// ================================================================================================
// HELPER STRUCTURES
// ================================================================================================

/**
 * @brief Dynamic string buffer for JSON writing.
 */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} SituationJSONBuffer;

// ================================================================================================
// JSON BUFFER OPERATIONS
// ================================================================================================

static SituationJSONBuffer* _SituationCreateJSONBuffer(void) {
    SituationJSONBuffer* buf = (SituationJSONBuffer*)SIT_CALLOC(1, sizeof(SituationJSONBuffer));
    if (!buf) return NULL;
    
    buf->capacity = SITUATION_JSON_BUFFER_SIZE;
    buf->data = (char*)SIT_MALLOC(buf->capacity);
    if (!buf->data) {
        SIT_FREE(buf);
        return NULL;
    }
    
    buf->data[0] = '\0';
    buf->size = 0;
    return buf;
}

static void _SituationDestroyJSONBuffer(SituationJSONBuffer* buf) {
    if (!buf) return;
    if (buf->data) SIT_FREE(buf->data);
    SIT_FREE(buf);
}

static bool _SituationAppendToBuffer(SituationJSONBuffer* buf, const char* str) {
    if (!buf || !str) return false;
    
    size_t len = strlen(str);
    size_t needed = buf->size + len + 1;
    
    // Resize if needed
    if (needed > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < needed) new_capacity *= 2;
        
        char* new_data = (char*)SIT_REALLOC(buf->data, new_capacity);
        if (!new_data) return false;
        
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    
    // Append string
    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    
    return true;
}

static bool _SituationAppendFormatted(SituationJSONBuffer* buf, const char* format, ...) {
    char temp[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    return _SituationAppendToBuffer(buf, temp);
}

// ================================================================================================
// JSON ESCAPING
// ================================================================================================

static void _SituationEscapeJSONString(const char* input, char* output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 2; i++) {
        switch (input[i]) {
            case '"':  output[j++] = '\\'; output[j++] = '"'; break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n'; break;
            case '\r': output[j++] = '\\'; output[j++] = 'r'; break;
            case '\t': output[j++] = '\\'; output[j++] = 't'; break;
            default:   output[j++] = input[i]; break;
        }
    }
    output[j] = '\0';
}

// ================================================================================================
// SERIALIZATION IMPLEMENTATION
// ================================================================================================

static bool _SituationSerializeNode(
    SituationJSONBuffer* buf,
    const SituationNode* node,
    bool is_last
) {
    if (!buf || !node || !node->metadata) return false;
    
    char escaped_name[256];
    _SituationEscapeJSONString(node->metadata->name, escaped_name, sizeof(escaped_name));
    
    uint16_t node_id = _SituationGetHandleIndex(node->handle);
    
    // Start node object
    if (!_SituationAppendToBuffer(buf, "    {\n")) return false;
    if (!_SituationAppendFormatted(buf, "      \"id\": %u,\n", node_id)) return false;
    if (!_SituationAppendFormatted(buf, "      \"type\": \"%s\",\n", escaped_name)) return false;
    if (!_SituationAppendFormatted(buf, "      \"active\": %s", node->is_active ? "true" : "false")) return false;
    
    // Serialize controls if any
    if (node->metadata->num_controls > 0 && node->control_values) {
        if (!_SituationAppendToBuffer(buf, ",\n      \"controls\": {\n")) return false;
        
        for (int i = 0; i < node->metadata->num_controls; i++) {
            const SituationControlDesc* ctrl = &node->metadata->controls[i];
            char escaped_ctrl_name[128];
            _SituationEscapeJSONString(ctrl->name, escaped_ctrl_name, sizeof(escaped_ctrl_name));
            
            if (!_SituationAppendFormatted(buf, "        \"%s\": %.6f", 
                escaped_ctrl_name, node->control_values[i])) return false;
            
            if (i < node->metadata->num_controls - 1) {
                if (!_SituationAppendToBuffer(buf, ",\n")) return false;
            } else {
                if (!_SituationAppendToBuffer(buf, "\n")) return false;
            }
        }
        
        if (!_SituationAppendToBuffer(buf, "      }\n")) return false;
    } else {
        if (!_SituationAppendToBuffer(buf, "\n")) return false;
    }
    
    // End node object
    if (!_SituationAppendToBuffer(buf, is_last ? "    }\n" : "    },\n")) return false;
    
    return true;
}

static bool _SituationSerializePatch(
    SituationJSONBuffer* buf,
    const SituationPatch* patch,
    bool is_last
) {
    if (!buf || !patch) return false;
    
    const char* type_str = patch->is_control ? "control" : "audio";
    
    uint16_t src_id = _SituationGetHandleIndex(patch->src_node);
    uint16_t dst_id = _SituationGetHandleIndex(patch->dst_node);
    
    if (!_SituationAppendToBuffer(buf, "    {\n")) return false;
    if (!_SituationAppendFormatted(buf, "      \"src_node\": %u,\n", src_id)) return false;
    if (!_SituationAppendFormatted(buf, "      \"src_port\": %d,\n", patch->src_port)) return false;
    if (!_SituationAppendFormatted(buf, "      \"dst_node\": %u,\n", dst_id)) return false;
    if (!_SituationAppendFormatted(buf, "      \"dst_port\": %d,\n", patch->dst_port)) return false;
    if (!_SituationAppendFormatted(buf, "      \"type\": \"%s\"\n", type_str)) return false;
    if (!_SituationAppendToBuffer(buf, is_last ? "    }\n" : "    },\n")) return false;
    
    return true;
}

char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph) {
    if (!graph) return NULL;
    
    SituationJSONBuffer* buf = _SituationCreateJSONBuffer();
    if (!buf) return NULL;
    
    // Start JSON object
    if (!_SituationAppendToBuffer(buf, "{\n")) goto error;
    if (!_SituationAppendFormatted(buf, "  \"version\": \"%s\",\n", SITUATION_SERIALIZATION_VERSION)) goto error;
    if (!_SituationAppendFormatted(buf, "  \"sample_rate\": %d,\n", 48000)) goto error;  // TODO: Get from audio context
    
    // Serialize nodes
    if (!_SituationAppendToBuffer(buf, "  \"nodes\": [\n")) goto error;
    
    // Count active nodes
    int node_count = 0;
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i] != NULL) {
            node_count++;
        }
    }
    
    // Serialize each node
    int current = 0;
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i] != NULL) {
            current++;
            if (!_SituationSerializeNode(buf, graph->nodes[i], current == node_count)) goto error;
        }
    }
    
    if (!_SituationAppendToBuffer(buf, "  ],\n")) goto error;
    
    // Serialize patches
    if (!_SituationAppendToBuffer(buf, "  \"patches\": [\n")) goto error;
    
    for (int i = 0; i < graph->patch_count; i++) {
        if (!_SituationSerializePatch(buf, &graph->patches[i], i == graph->patch_count - 1)) goto error;
    }
    
    if (!_SituationAppendToBuffer(buf, "  ]\n")) goto error;
    
    // End JSON object
    if (!_SituationAppendToBuffer(buf, "}\n")) goto error;
    
    // Transfer ownership of string to caller
    char* result = buf->data;
    buf->data = NULL;  // Prevent double-free
    _SituationDestroyJSONBuffer(buf);
    
    return result;
    
error:
    _SituationDestroyJSONBuffer(buf);
    return NULL;
}

void SituationFreeJSONString(char* json_string) {
    if (json_string) {
        SIT_FREE(json_string);
    }
}

SituationError SituationSaveGraphToFile(
    const SituationAudioGraph* graph,
    const char* filepath
) {
    if (!graph || !filepath) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Serialize to JSON string
    char* json = SituationSerializeGraphToJSON(graph);
    if (!json) {
        return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    }
    
    // Write to file
    FILE* file = fopen(filepath, "w");
    if (!file) {
        SituationFreeJSONString(json);
        return SITUATION_ERROR_NODE_SERIALIZATION_FAILED;
    }
    
    size_t written = fwrite(json, 1, strlen(json), file);
    fclose(file);
    
    SituationFreeJSONString(json);
    
    return (written > 0) ? SITUATION_SUCCESS : SITUATION_ERROR_NODE_SERIALIZATION_FAILED;
}

// ================================================================================================
// DESERIALIZATION IMPLEMENTATION (Simplified Parser)
// ================================================================================================

// JSON Token Types
typedef enum {
    JSON_TOKEN_OBJECT_START,    // {
    JSON_TOKEN_OBJECT_END,      // }
    JSON_TOKEN_ARRAY_START,     // [
    JSON_TOKEN_ARRAY_END,       // ]
    JSON_TOKEN_STRING,          // "text"
    JSON_TOKEN_NUMBER,          // 123.45
    JSON_TOKEN_TRUE,            // true
    JSON_TOKEN_FALSE,           // false
    JSON_TOKEN_NULL,            // null
    JSON_TOKEN_COLON,           // :
    JSON_TOKEN_COMMA,           // ,
    JSON_TOKEN_EOF,
    JSON_TOKEN_ERROR
} JSONTokenType;

typedef struct {
    JSONTokenType type;
    const char* start;
    size_t length;
    double number_value;
} JSONToken;

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
    JSONToken current;
    char error_message[256];
} JSONParser;

// ================================================================================================
// JSON PARSER HELPERS
// ================================================================================================

static void _JSONInitParser(JSONParser* parser, const char* json) {
    parser->json = json;
    parser->pos = 0;
    parser->length = strlen(json);
    parser->current.type = JSON_TOKEN_ERROR;
    parser->error_message[0] = '\0';
}

static bool _JSONIsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool _JSONIsDigit(char c) {
    return c >= '0' && c <= '9';
}

static void _JSONSkipWhitespace(JSONParser* parser) {
    while (parser->pos < parser->length && _JSONIsWhitespace(parser->json[parser->pos])) {
        parser->pos++;
    }
}

static bool _JSONNextToken(JSONParser* parser) {
    _JSONSkipWhitespace(parser);
    
    if (parser->pos >= parser->length) {
        parser->current.type = JSON_TOKEN_EOF;
        return true;
    }
    
    char c = parser->json[parser->pos];
    parser->current.start = &parser->json[parser->pos];
    parser->current.length = 1;
    
    switch (c) {
        case '{':
            parser->current.type = JSON_TOKEN_OBJECT_START;
            parser->pos++;
            return true;
            
        case '}':
            parser->current.type = JSON_TOKEN_OBJECT_END;
            parser->pos++;
            return true;
            
        case '[':
            parser->current.type = JSON_TOKEN_ARRAY_START;
            parser->pos++;
            return true;
            
        case ']':
            parser->current.type = JSON_TOKEN_ARRAY_END;
            parser->pos++;
            return true;
            
        case ':':
            parser->current.type = JSON_TOKEN_COLON;
            parser->pos++;
            return true;
            
        case ',':
            parser->current.type = JSON_TOKEN_COMMA;
            parser->pos++;
            return true;
            
        case '"': {
            // Parse string
            parser->pos++; // Skip opening quote
            parser->current.start = &parser->json[parser->pos];
            parser->current.length = 0;
            
            while (parser->pos < parser->length && parser->json[parser->pos] != '"') {
                if (parser->json[parser->pos] == '\\') {
                    parser->pos++; // Skip escape character
                }
                parser->pos++;
                parser->current.length++;
            }
            
            if (parser->pos >= parser->length) {
                snprintf(parser->error_message, sizeof(parser->error_message), 
                        "Unterminated string");
                parser->current.type = JSON_TOKEN_ERROR;
                return false;
            }
            
            parser->pos++; // Skip closing quote
            parser->current.type = JSON_TOKEN_STRING;
            return true;
        }
        
        case 't': {
            // Parse "true"
            if (parser->pos + 4 <= parser->length &&
                strncmp(&parser->json[parser->pos], "true", 4) == 0) {
                parser->current.type = JSON_TOKEN_TRUE;
                parser->current.length = 4;
                parser->pos += 4;
                return true;
            }
            break;
        }
        
        case 'f': {
            // Parse "false"
            if (parser->pos + 5 <= parser->length &&
                strncmp(&parser->json[parser->pos], "false", 5) == 0) {
                parser->current.type = JSON_TOKEN_FALSE;
                parser->current.length = 5;
                parser->pos += 5;
                return true;
            }
            break;
        }
        
        case 'n': {
            // Parse "null"
            if (parser->pos + 4 <= parser->length &&
                strncmp(&parser->json[parser->pos], "null", 4) == 0) {
                parser->current.type = JSON_TOKEN_NULL;
                parser->current.length = 4;
                parser->pos += 4;
                return true;
            }
            break;
        }
        
        default: {
            // Try to parse number
            if (_JSONIsDigit(c) || c == '-') {
                size_t start = parser->pos;
                
                if (c == '-') parser->pos++;
                
                while (parser->pos < parser->length && _JSONIsDigit(parser->json[parser->pos])) {
                    parser->pos++;
                }
                
                if (parser->pos < parser->length && parser->json[parser->pos] == '.') {
                    parser->pos++;
                    while (parser->pos < parser->length && _JSONIsDigit(parser->json[parser->pos])) {
                        parser->pos++;
                    }
                }
                
                if (parser->pos < parser->length && 
                    (parser->json[parser->pos] == 'e' || parser->json[parser->pos] == 'E')) {
                    parser->pos++;
                    if (parser->pos < parser->length && 
                        (parser->json[parser->pos] == '+' || parser->json[parser->pos] == '-')) {
                        parser->pos++;
                    }
                    while (parser->pos < parser->length && _JSONIsDigit(parser->json[parser->pos])) {
                        parser->pos++;
                    }
                }
                
                parser->current.type = JSON_TOKEN_NUMBER;
                parser->current.start = &parser->json[start];
                parser->current.length = parser->pos - start;
                parser->current.number_value = atof(parser->current.start);
                return true;
            }
            break;
        }
    }
    
    snprintf(parser->error_message, sizeof(parser->error_message), 
            "Unexpected character: '%c'", c);
    parser->current.type = JSON_TOKEN_ERROR;
    return false;
}

static bool _JSONExpect(JSONParser* parser, JSONTokenType type) {
    if (parser->current.type != type) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Expected token type %d, got %d", type, parser->current.type);
        return false;
    }
    return _JSONNextToken(parser);
}

static bool _JSONGetString(JSONParser* parser, char* buffer, size_t buffer_size) {
    if (parser->current.type != JSON_TOKEN_STRING) {
        return false;
    }
    
    size_t copy_len = parser->current.length;
    if (copy_len >= buffer_size) {
        copy_len = buffer_size - 1;
    }
    
    memcpy(buffer, parser->current.start, copy_len);
    buffer[copy_len] = '\0';
    
    return _JSONNextToken(parser);
}

static bool _JSONGetNumber(JSONParser* parser, float* value) {
    if (parser->current.type != JSON_TOKEN_NUMBER) {
        return false;
    }
    
    *value = (float)parser->current.number_value;
    return _JSONNextToken(parser);
}

static bool _JSONGetBool(JSONParser* parser, bool* value) {
    if (parser->current.type == JSON_TOKEN_TRUE) {
        *value = true;
        return _JSONNextToken(parser);
    } else if (parser->current.type == JSON_TOKEN_FALSE) {
        *value = false;
        return _JSONNextToken(parser);
    }
    return false;
}

static bool _JSONFindKey(JSONParser* parser, const char* key) {
    // Save current position in case we need to backtrack
    size_t saved_pos = parser->pos;
    JSONToken saved_token = parser->current;
    
    // Look for the key in the current object
    while (parser->current.type != JSON_TOKEN_OBJECT_END && 
           parser->current.type != JSON_TOKEN_EOF) {
        
        if (parser->current.type == JSON_TOKEN_STRING) {
            // Check if this string matches the key
            if (parser->current.length == strlen(key) &&
                strncmp(parser->current.start, key, parser->current.length) == 0) {
                // Found the key, expect colon next
                if (!_JSONNextToken(parser)) return false;
                if (!_JSONExpect(parser, JSON_TOKEN_COLON)) return false;
                return true;
            }
        }
        
        // Skip to next token
        if (!_JSONNextToken(parser)) {
            // Restore position and return false
            parser->pos = saved_pos;
            parser->current = saved_token;
            return false;
        }
    }
    
    // Key not found, restore position
    parser->pos = saved_pos;
    parser->current = saved_token;
    return false;
}

// ================================================================================================
// NODE & PATCH PARSING
// ================================================================================================

static bool _JSONParseNode(
    SituationAudioGraph* graph,
    JSONParser* parser,
    SituationNodeHandle* out_handle,
    int* out_id
) {
    // Expect object start
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_START)) return false;
    
    // Parse node ID
    int node_id = 0;
    if (_JSONFindKey(parser, "id")) {
        float id_float;
        _JSONGetNumber(parser, &id_float);
        node_id = (int)id_float;
    }
    
    // Parse node type (device name)
    char type_name[256];
    if (!_JSONFindKey(parser, "type")) return false;
    if (!_JSONGetString(parser, type_name, sizeof(type_name))) return false;
    
    // Find device type by name
    SituationNodeType device_type = (SituationNodeType)-1;
    bool found = false;
    
    for (int i = 0; i < SituationGetRegisteredDeviceCount(); i++) {
        SituationDeviceMetadata meta;
        if (SituationGetDeviceMetadataByIndex(i, &meta) == SITUATION_SUCCESS) {
            if (strcmp(meta.name, type_name) == 0) {
                device_type = meta.type;
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Unknown device type: %s", type_name);
        return false;
    }
    
    // Create node
    SituationError err = SituationCreateNode(graph, device_type, out_handle);
    if (err != SITUATION_SUCCESS) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Failed to create node (error code %d)", err);
        return false;
    }
    
    // Parse active state (optional)
    if (_JSONFindKey(parser, "active")) {
        bool active = true;
        _JSONGetBool(parser, &active);
        SituationNode* node = SituationGetNode(graph, *out_handle);
        if (node) node->is_active = active;
    }
    
    // Parse controls (optional)
    if (_JSONFindKey(parser, "controls")) {
        if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_START)) return false;
        
        // Get metadata for this device type
        SituationDeviceMetadata meta;
        if (SituationGetDeviceMetadata(device_type, &meta) != SITUATION_SUCCESS) {
            return false;
        }
        
        // Parse each control
        while (parser->current.type != JSON_TOKEN_OBJECT_END) {
            // Get control name
            char ctrl_name[128];
            if (!_JSONGetString(parser, ctrl_name, sizeof(ctrl_name))) break;
            
            // Expect colon
            if (!_JSONExpect(parser, JSON_TOKEN_COLON)) break;
            
            // Get control value
            float value;
            if (!_JSONGetNumber(parser, &value)) break;
            
            // Find control ID by name and set value
            for (int i = 0; i < meta.num_controls; i++) {
                if (strcmp(meta.controls[i].name, ctrl_name) == 0) {
                    SituationSetControl(graph, *out_handle, i, value);
                    break;
                }
            }
            
            // Check for comma or object end
            if (parser->current.type == JSON_TOKEN_COMMA) {
                _JSONNextToken(parser);
            }
        }
        
        // Expect object end
        if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_END)) return false;
    }
    
    // Expect node object end
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_END)) return false;
    
    *out_id = node_id;
    return true;
}

static bool _JSONParsePatch(
    SituationAudioGraph* graph,
    JSONParser* parser,
    const SituationNodeHandle* node_handles,
    const int* node_id_map,
    int num_nodes
) {
    // Expect object start
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_START)) return false;
    
    // Parse patch fields
    int src_id = -1, src_port = -1, dst_id = -1, dst_port = -1;
    bool is_control = false;
    
    if (_JSONFindKey(parser, "src_node")) {
        float val;
        _JSONGetNumber(parser, &val);
        src_id = (int)val;
    }
    
    if (_JSONFindKey(parser, "src_port")) {
        float val;
        _JSONGetNumber(parser, &val);
        src_port = (int)val;
    }
    
    if (_JSONFindKey(parser, "dst_node")) {
        float val;
        _JSONGetNumber(parser, &val);
        dst_id = (int)val;
    }
    
    if (_JSONFindKey(parser, "dst_port")) {
        float val;
        _JSONGetNumber(parser, &val);
        dst_port = (int)val;
    }
    
    if (_JSONFindKey(parser, "type")) {
        char type_str[32];
        _JSONGetString(parser, type_str, sizeof(type_str));
        is_control = (strcmp(type_str, "control") == 0);
    }
    
    // Map JSON IDs to handles
    SituationNodeHandle src_handle = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst_handle = SITUATION_INVALID_NODE_HANDLE;
    
    for (int i = 0; i < num_nodes; i++) {
        if (node_id_map[i] == src_id) src_handle = node_handles[i];
        if (node_id_map[i] == dst_id) dst_handle = node_handles[i];
    }
    
    if (src_handle == SITUATION_INVALID_NODE_HANDLE ||
        dst_handle == SITUATION_INVALID_NODE_HANDLE) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Invalid node IDs in patch: src=%d, dst=%d", src_id, dst_id);
        return false;
    }
    
    // Create patch
    SituationError err = SituationCreatePatch(
        graph, src_handle, src_port, dst_handle, dst_port, is_control
    );
    
    if (err != SITUATION_SUCCESS) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Failed to create patch (error code %d)", err);
        return false;
    }
    
    // Expect object end
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_END)) return false;
    
    return true;
}

// ================================================================================================
// MAIN DESERIALIZATION FUNCTIONS
// ================================================================================================

SituationError SituationDeserializeGraphFromJSON(
    SituationAudioGraph* graph,
    const char* json_string,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    (void)device_funcs;  // Not needed - registry is already populated
    (void)num_device_funcs;
    
    if (!graph || !json_string) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Initialize parser
    JSONParser parser;
    _JSONInitParser(&parser, json_string);
    
    // Start parsing - get first token
    if (!_JSONNextToken(&parser)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Parse root object
    if (!_JSONExpect(&parser, JSON_TOKEN_OBJECT_START)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Parse version (optional)
    if (_JSONFindKey(&parser, "version")) {
        char version[32];
        _JSONGetString(&parser, version, sizeof(version));
        if (!SituationIsVersionCompatible(version)) {
            // Warning: version mismatch (but continue anyway)
        }
    }
    
    // Parse sample_rate (optional, for future use)
    if (_JSONFindKey(&parser, "sample_rate")) {
        float sr;
        _JSONGetNumber(&parser, &sr);
        // TODO: Store sample rate somewhere
    }
    
    // Parse nodes array
    if (!_JSONFindKey(&parser, "nodes")) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_START)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Map from JSON node ID to handle
    SituationNodeHandle node_handles[SITUATION_MAX_NODES];
    int node_id_map[SITUATION_MAX_NODES];
    int num_nodes = 0;
    
    // Parse each node
    while (parser.current.type != JSON_TOKEN_ARRAY_END && 
           parser.current.type != JSON_TOKEN_EOF) {
        
        if (num_nodes >= SITUATION_MAX_NODES) {
            return SITUATION_ERROR_NODE_LIMIT_REACHED;
        }
        
        if (!_JSONParseNode(graph, &parser, &node_handles[num_nodes], &node_id_map[num_nodes])) {
            return SITUATION_ERROR_INVALID_PARAM;
        }
        num_nodes++;
        
        // Check for comma or array end
        if (parser.current.type == JSON_TOKEN_COMMA) {
            _JSONNextToken(&parser);
        }
    }
    
    // Expect array end
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_END)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Parse patches array
    if (!_JSONFindKey(&parser, "patches")) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_START)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Parse each patch
    while (parser.current.type != JSON_TOKEN_ARRAY_END && 
           parser.current.type != JSON_TOKEN_EOF) {
        
        if (!_JSONParsePatch(graph, &parser, node_handles, node_id_map, num_nodes)) {
            return SITUATION_ERROR_INVALID_PARAM;
        }
        
        // Check for comma or array end
        if (parser.current.type == JSON_TOKEN_COMMA) {
            _JSONNextToken(&parser);
        }
    }
    
    // Expect array end
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_END)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Expect root object end
    if (!_JSONExpect(&parser, JSON_TOKEN_OBJECT_END)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    return SITUATION_SUCCESS;
}

SituationError SituationLoadGraphFromFile(
    SituationAudioGraph* graph,
    const char* filepath,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    if (!graph || !filepath) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    
    // Read file into memory
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return SITUATION_ERROR_NODE_SERIALIZATION_FAILED;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(file);
        return SITUATION_ERROR_NODE_SERIALIZATION_FAILED;
    }
    
    // Allocate buffer
    char* json = (char*)SIT_MALLOC(size + 1);
    if (!json) {
        fclose(file);
        return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    }
    
    // Read file
    size_t read = fread(json, 1, size, file);
    fclose(file);
    json[read] = '\0';
    
    // Deserialize
    SituationError err = SituationDeserializeGraphFromJSON(
        graph, json, device_funcs, num_device_funcs
    );
    
    SIT_FREE(json);
    return err;
}

// ================================================================================================
// VERSION INFORMATION
// ================================================================================================

const char* SituationGetSerializationVersion(void) {
    return SITUATION_SERIALIZATION_VERSION;
}

bool SituationIsVersionCompatible(const char* json_version) {
    if (!json_version) return false;
    
    // For now, only accept exact version match
    // TODO: Implement semantic versioning comparison
    return strcmp(json_version, SITUATION_SERIALIZATION_VERSION) == 0;
}

#endif // SITUATION_NODE_GRAPH_SERIALIZATION_IMPL_H



