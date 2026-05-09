# Phase 5 Session 2: JSON Parsing & Deserialization

**Date**: 2026-03-02  
**Status**: 🚀 Ready to Start  
**Estimated Time**: 1-2 hours  
**Complexity**: HIGH (JSON parsing is non-trivial)

## 🎯 Session Goals

Implement JSON parsing to load saved audio graphs back into memory, completing the save/load round-trip.

## 📋 Implementation Strategy

### Option 1: Minimal Custom Parser (RECOMMENDED)
**Pros**:
- No external dependencies
- Full control over error messages
- Lightweight
- Tailored to our specific JSON format

**Cons**:
- More code to write
- Need to handle edge cases

**Decision**: Use custom parser since our JSON format is simple and predictable.

### Option 2: External Library (parson, cJSON, jsmn)
**Pros**:
- Battle-tested
- Handles all edge cases
- Less code to write

**Cons**:
- External dependency
- Larger binary size
- Less control over errors

## 🔧 Implementation Plan

### Step 1: Simple JSON Tokenizer (30 min)

Create a basic tokenizer that can extract:
- Strings: `"key"`, `"value"`
- Numbers: `123`, `456.789`
- Booleans: `true`, `false`
- Objects: `{...}`
- Arrays: `[...]`

```c
typedef enum {
    JSON_TOKEN_OBJECT_START,    // {
    JSON_TOKEN_OBJECT_END,      // }
    JSON_TOKEN_ARRAY_START,     // [
    JSON_TOKEN_ARRAY_END,       // ]
    JSON_TOKEN_STRING,          // "text"
    JSON_TOKEN_NUMBER,          // 123.45
    JSON_TOKEN_TRUE,            // true
    JSON_TOKEN_FALSE,           // false
    JSON_TOKEN_COLON,           // :
    JSON_TOKEN_COMMA,           // ,
    JSON_TOKEN_EOF,
    JSON_TOKEN_ERROR
} JSONTokenType;

typedef struct {
    JSONTokenType type;
    const char* start;
    size_t length;
    double number_value;  // For numbers
} JSONToken;

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
    JSONToken current;
} JSONParser;
```

### Step 2: JSON Value Extraction (30 min)

Helper functions to extract values:
```c
// Skip whitespace
static void _JSONSkipWhitespace(JSONParser* parser);

// Get next token
static bool _JSONNextToken(JSONParser* parser);

// Expect specific token
static bool _JSONExpect(JSONParser* parser, JSONTokenType type);

// Extract string value
static bool _JSONGetString(JSONParser* parser, char* buffer, size_t buffer_size);

// Extract number value
static bool _JSONGetNumber(JSONParser* parser, float* value);

// Extract boolean value
static bool _JSONGetBool(JSONParser* parser, bool* value);

// Find object key
static bool _JSONFindKey(JSONParser* parser, const char* key);
```

### Step 3: Graph Deserialization (45 min)

Main deserialization logic:
```c
SituationNodeError SituationDeserializeGraphFromJSON(
    SituationAudioGraph* graph,
    const char* json_string,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    // 1. Initialize parser
    JSONParser parser;
    _JSONInitParser(&parser, json_string);
    
    // 2. Parse root object
    if (!_JSONExpect(&parser, JSON_TOKEN_OBJECT_START)) {
        return SITUATION_NODE_ERR_INVALID_PARAM;
    }
    
    // 3. Parse version
    char version[32];
    if (_JSONFindKey(&parser, "version")) {
        _JSONGetString(&parser, version, sizeof(version));
        if (!SituationIsVersionCompatible(version)) {
            // Warning: version mismatch
        }
    }
    
    // 4. Parse sample_rate (optional, for future use)
    int sample_rate = 48000;
    if (_JSONFindKey(&parser, "sample_rate")) {
        float sr;
        _JSONGetNumber(&parser, &sr);
        sample_rate = (int)sr;
    }
    
    // 5. Parse nodes array
    if (!_JSONFindKey(&parser, "nodes")) {
        return SITUATION_NODE_ERR_INVALID_PARAM;
    }
    
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_START)) {
        return SITUATION_NODE_ERR_INVALID_PARAM;
    }
    
    // Map from JSON node ID to handle
    SituationNodeHandle node_handles[SITUATION_MAX_NODES];
    int node_id_map[SITUATION_MAX_NODES];
    int num_nodes = 0;
    
    // Parse each node
    while (parser.current.type != JSON_TOKEN_ARRAY_END) {
        if (!_JSONParseNode(graph, &parser, device_funcs, num_device_funcs,
                           &node_handles[num_nodes], &node_id_map[num_nodes])) {
            return SITUATION_NODE_ERR_INVALID_PARAM;
        }
        num_nodes++;
        
        // Check for comma or array end
        _JSONNextToken(&parser);
        if (parser.current.type == JSON_TOKEN_COMMA) {
            _JSONNextToken(&parser);
        }
    }
    
    // 6. Parse patches array
    if (!_JSONFindKey(&parser, "patches")) {
        return SITUATION_NODE_ERR_INVALID_PARAM;
    }
    
    if (!_JSONExpect(&parser, JSON_TOKEN_ARRAY_START)) {
        return SITUATION_NODE_ERR_INVALID_PARAM;
    }
    
    // Parse each patch
    while (parser.current.type != JSON_TOKEN_ARRAY_END) {
        if (!_JSONParsePatch(graph, &parser, node_handles, node_id_map, num_nodes)) {
            return SITUATION_NODE_ERR_INVALID_PARAM;
        }
        
        // Check for comma or array end
        _JSONNextToken(&parser);
        if (parser.current.type == JSON_TOKEN_COMMA) {
            _JSONNextToken(&parser);
        }
    }
    
    return SITUATION_NODE_SUCCESS;
}
```

### Step 4: Node Parsing (30 min)

```c
static bool _JSONParseNode(
    SituationAudioGraph* graph,
    JSONParser* parser,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs,
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
    SituationNodeType device_type = SITUATION_NODE_INVALID;
    for (int i = 0; i < SituationGetRegisteredDeviceCount(); i++) {
        const SituationDeviceMetadata* meta = SituationGetDeviceMetadataByIndex(i);
        if (meta && strcmp(meta->name, type_name) == 0) {
            device_type = meta->type;
            break;
        }
    }
    
    if (device_type == SITUATION_NODE_INVALID) {
        // Device type not found
        return false;
    }
    
    // Create node
    SituationNodeError err = SituationCreateNode(graph, device_type, out_handle);
    if (err != SITUATION_NODE_SUCCESS) return false;
    
    // Parse active state
    bool active = true;
    if (_JSONFindKey(parser, "active")) {
        _JSONGetBool(parser, &active);
        // TODO: Set node active state
    }
    
    // Parse controls
    if (_JSONFindKey(parser, "controls")) {
        if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_START)) return false;
        
        // Parse each control
        const SituationDeviceMetadata* meta = SituationGetDeviceMetadata(device_type);
        while (parser->current.type != JSON_TOKEN_OBJECT_END) {
            // Get control name
            char ctrl_name[128];
            if (!_JSONGetString(parser, ctrl_name, sizeof(ctrl_name))) break;
            
            // Expect colon
            if (!_JSONExpect(parser, JSON_TOKEN_COLON)) break;
            
            // Get control value
            float value;
            if (!_JSONGetNumber(parser, &value)) break;
            
            // Find control ID by name
            for (int i = 0; i < meta->num_controls; i++) {
                if (strcmp(meta->controls[i].name, ctrl_name) == 0) {
                    SituationSetControl(graph, *out_handle, i, value);
                    break;
                }
            }
            
            // Check for comma or object end
            _JSONNextToken(parser);
            if (parser->current.type == JSON_TOKEN_COMMA) {
                _JSONNextToken(parser);
            }
        }
    }
    
    // Expect object end
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_END)) return false;
    
    *out_id = node_id;
    return true;
}
```

### Step 5: Patch Parsing (15 min)

```c
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
        return false;
    }
    
    // Create patch
    SituationNodeError err = SituationCreatePatch(
        graph, src_handle, src_port, dst_handle, dst_port, is_control
    );
    
    // Expect object end
    if (!_JSONExpect(parser, JSON_TOKEN_OBJECT_END)) return false;
    
    return (err == SITUATION_NODE_SUCCESS);
}
```

### Step 6: File Loading (5 min)

```c
SituationNodeError SituationLoadGraphFromFile(
    SituationAudioGraph* graph,
    const char* filepath,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    // Read file into memory
    FILE* file = fopen(filepath, "r");
    if (!file) return SITUATION_NODE_ERR_FILE_IO;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* json = (char*)SIT_MALLOC(size + 1);
    if (!json) {
        fclose(file);
        return SITUATION_NODE_ERR_OUT_OF_MEMORY;
    }
    
    // Read file
    size_t read = fread(json, 1, size, file);
    fclose(file);
    json[read] = '\0';
    
    // Deserialize
    SituationNodeError err = SituationDeserializeGraphFromJSON(
        graph, json, device_funcs, num_device_funcs
    );
    
    SIT_FREE(json);
    return err;
}
```

## 🧪 Testing Strategy

### Test 1: Round-Trip Test
```c
// Save graph
SituationSaveGraphToFile(graph, "test.json");

// Create new graph
SituationAudioGraph* graph2 = SituationCreateGraph();

// Load graph
SituationLoadGraphFromFile(graph2, "test.json", device_funcs, num_funcs);

// Verify:
// - Same number of nodes
// - Same node types
// - Same control values
// - Same patches
```

### Test 2: Error Handling
- Malformed JSON
- Missing required fields
- Invalid device types
- Out-of-range port indices
- Version mismatch

### Test 3: Edge Cases
- Empty graph
- Graph with no patches
- Graph with only one node
- Large graph (100+ nodes)

## 📊 Success Metrics

- ✅ Parser handles valid JSON correctly
- ✅ Round-trip preserves all data
- ✅ Error messages are helpful
- ✅ No memory leaks
- ✅ Performance acceptable (< 100ms for typical graphs)

## 🚨 Potential Challenges

1. **JSON Parsing Complexity**: Need robust tokenizer
2. **Error Recovery**: Graceful handling of malformed JSON
3. **ID Mapping**: JSON node IDs → runtime handles
4. **Memory Management**: Proper cleanup on errors
5. **Version Compatibility**: Handle format changes

## 💡 Implementation Tips

1. **Start Simple**: Get basic parsing working first
2. **Test Incrementally**: Test each function as you write it
3. **Use Assertions**: Catch bugs early
4. **Handle Errors**: Every parse step can fail
5. **Log Progress**: Print what's being parsed for debugging

## 🎯 Deliverables

1. ✅ JSON tokenizer implementation
2. ✅ Node deserialization
3. ✅ Patch deserialization
4. ✅ File loading function
5. ✅ Round-trip test demo
6. ✅ Error handling
7. ✅ Documentation

---

**Ready to Start**: 2026-03-02  
**Estimated Completion**: 1-2 hours  
**Difficulty**: HIGH 🔥  
**Maintained By**: Kiro AI Assistant
