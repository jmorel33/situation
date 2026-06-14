#ifndef DYNAMO_H
#define DYNAMO_H

/**
 * Minimal physics body type used by RGL particles.
 * Full Dynamo integration can replace this stub later.
 */
#include <cglm/cglm.h>

typedef struct DynamoBody {
    vec3 position;
    vec3 velocity;
    float mass;
} DynamoBody;

#endif /* DYNAMO_H */
