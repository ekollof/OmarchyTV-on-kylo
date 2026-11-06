/* Pure accessors for the firmware's serialized PictureSettingConfiguration
 * array (20 sources x 0x240 bytes, settingsDB.m_picture_setting_configuration).
 *
 * Field offsets and value IDs are recovered from the shipped Aurora ELF:
 *   +0x04  NovoPictureMode: 0 DYNAMIC, 1 NATURAL, 2 CINEMA, 3 GAME, 4 SPORT
 *   +0x08  zoom for the non-full path (NovoVideoTVZoomMode IDs)
 *   +0x0c  zoom for the full-mode path
 *   +0x44 + pictureMode*0x70 + device3d*0x38  NovoEnergySavingMode
 *
 * NovoPictureSettings::getZoomMode(index, format) returns +0x0c for format 2,
 * +0x08 for format 1, and for format 0 returns +0x0c only when
 * isFullModeApplicable(index) is true, otherwise +0x08. The UI always uses
 * format 0, so callers pass the full-mode decision explicitly.
 *
 * The stored values are the vendor SettingValue IDs, not JSON array indices.
 * Picture zoom IDs (construction order): AUTO 1, 16:9 5, SUBTITLE 6, 14:9 3,
 * 14:9 ZOOM 4, 4:3 2, 21:9 11, CINEMA 8, FULL 9. Energy IDs: OFF 0,
 * MINIMUM 1, MEDIUM 2, MAXIMUM 3, AUTO 4, CUSTOM 5, SCREEN_OFF 6.
 */
#ifndef OPENAURORA_PICTURE_CONFIG_H
#define OPENAURORA_PICTURE_CONFIG_H

#include <stddef.h>

#define OA_PICTURE_SOURCES 20
#define OA_PICTURE_STRIDE 0x240
#define OA_PICTURE_BYTES (OA_PICTURE_SOURCES * OA_PICTURE_STRIDE)

#define OA_PICTURE_OFF_MODE 0x04
#define OA_PICTURE_OFF_ZOOM_NORMAL 0x08
#define OA_PICTURE_OFF_ZOOM_FULL 0x0c
#define OA_PICTURE_OFF_ENERGY 0x44
#define OA_PICTURE_ENERGY_MODE_STRIDE 0x70
#define OA_PICTURE_ENERGY_3D_STRIDE 0x38

/* NovoVideoTVZoomMode::ZOOM_MODE_FULL. setZoomMode(format 0) stores this in
 * the full field even when isFullModeApplicable() is false. */
#define OA_PICTURE_ZOOM_FULL 9

/* All functions return 0 on success, EINVAL for NULL/out-of-range inputs.
 * The getters write *out; the setters mutate the caller's 0x240-byte blob. */
int oa_picture_get_mode(const unsigned char *blob, int source, int *out);
int oa_picture_set_mode(unsigned char *blob, int source, int mode);
int oa_picture_get_zoom(const unsigned char *blob, int source, int full_mode,
                        int *out);
int oa_picture_set_zoom(unsigned char *blob, int source, int full_mode,
                        int zoom);
int oa_picture_get_energy(const unsigned char *blob, int source, int mode,
                          int device3d, int *out);
int oa_picture_set_energy(unsigned char *blob, int source, int mode,
                          int device3d, int value);

/* Copy one source's 0x240-byte configuration. */
int oa_picture_get_config(const unsigned char *blob, int source,
                          const unsigned char **out);
int oa_picture_set_config(unsigned char *blob, int source,
                          const unsigned char *config);

#endif
