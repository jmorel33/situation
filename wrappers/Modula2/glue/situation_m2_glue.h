#ifndef SITUATION_M2_GLUE_H
#define SITUATION_M2_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SituationInitInfo SituationInitInfo;

/* ABI-safe helpers for GNU Modula-2 — builds SituationInitInfo in C layout. */
void SituationM2InitInfoZero(SituationInitInfo* out);
void SituationM2InitInfoWindow(SituationInitInfo* out, int width, int height, const char* title);

#ifdef __cplusplus
}
#endif

#endif /* SITUATION_M2_GLUE_H */