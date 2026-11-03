#ifndef OA_LAUNCHER_IMAGE_ID_H
#define OA_LAUNCHER_IMAGE_ID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LauncherHelper::getImageId(NovoLauncherOperationId op, WIDGET_STATE state)
 *
 * Returns the launcher glyph image id for an operation in a widget state, or
 * -1 for unknown operations. Recovered from the shipped ARM body at
 * 0xe4aaf4 (62-way tbh switch); only state==1 selects the alternate id, every
 * other state value takes the default. See
 * artifacts/reports/LAUNCHER-IMAGEID-2026-11-03.md.
 */
int32_t oa_launcher_get_image_id(uint32_t operation,uint32_t state);

#ifdef __cplusplus
}
#endif

#endif
