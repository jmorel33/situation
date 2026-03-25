/***************************************************************************************************
*
*   sit/aud/graph_serialization.h - Audio Graph Serialization API
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 5: JSON serialization for audio graphs. Save/load graph topology and parameters.
*   
*   This provides persistence for audio graphs, allowing users to save complex setups and
*   restore them later. Uses a simple JSON format that's human-readable and version-tracked.
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_SERIALIZATION_H
#define SITUATION_NODE_GRAPH_SERIALIZATION_H

#include "node_graph.h"
#include "node_graph_process.h"  // For SituationDeviceFunctions
#include "device_registry.h"

// NOTE: SituationDeviceFunctions is defined in situation_api.h

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================================================
// SERIALIZATION API
// ================================================================================================

/**
 * @brief Save audio graph to JSON file.
 * @param graph Audio graph to save.
 * @param filepath Path to output JSON file.
 * @return SITUATION_NODE_SUCCESS on success, error code otherwise.
 * 
 * @note Graph must not be processing during save (not thread-safe).
 * @note Creates parent directories if they don't exist.
 */
SituationError SituationSaveGraphToFile(
    const SituationAudioGraph* graph,
    const char* filepath
);

/**
 * @brief Load audio graph from JSON file.
 * @param graph Audio graph to populate (must be initialized).
 * @param filepath Path to input JSON file.
 * @param device_funcs Device function table for creating nodes.
 * @param num_device_funcs Number of entries in device function table.
 * @return SITUATION_NODE_SUCCESS on success, error code otherwise.
 * 
 * @note Clears existing graph before loading.
 * @note Returns error if device types are not registered.
 * @note Validates graph after loading (cycle detection).
 */
SituationError SituationLoadGraphFromFile(
    SituationAudioGraph* graph,
    const char* filepath,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
);

/**
 * @brief Serialize audio graph to JSON string.
 * @param graph Audio graph to serialize.
 * @return Allocated JSON string, or NULL on error. Must be freed with SituationFreeJSONString().
 * 
 * @note Caller must free returned string.
 * @note Returns NULL if graph is NULL or empty.
 */
char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph);

/**
 * @brief Deserialize audio graph from JSON string.
 * @param graph Audio graph to populate (must be initialized).
 * @param json_string JSON string to parse.
 * @param device_funcs Device function table for creating nodes.
 * @param num_device_funcs Number of entries in device function table.
 * @return SITUATION_NODE_SUCCESS on success, error code otherwise.
 * 
 * @note Clears existing graph before loading.
 * @note Returns error if JSON is malformed or device types are not registered.
 */
SituationError SituationDeserializeGraphFromJSON(
    SituationAudioGraph* graph,
    const char* json_string,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
);

/**
 * @brief Free JSON string allocated by serialization functions.
 * @param json_string JSON string to free.
 */
void SituationFreeJSONString(char* json_string);

// ================================================================================================
// VERSION INFORMATION
// ================================================================================================

/**
 * @brief Get serialization format version.
 * @return Version string (e.g., "2.4.0").
 */
const char* SituationGetSerializationVersion(void);

/**
 * @brief Check if JSON version is compatible with current version.
 * @param json_version Version string from JSON file.
 * @return true if compatible, false otherwise.
 */
bool SituationIsVersionCompatible(const char* json_version);

#ifdef __cplusplus
}
#endif

#endif // SITUATION_NODE_GRAPH_SERIALIZATION_H

