#ifndef OPENAURORA_SOURCE_MAP_H
#define OPENAURORA_SOURCE_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bronlijst row <-> source service type and picture-configuration index.
 *
 * Recovered from InputManager::setCurrentExtSrc and
 * NovoPictureSettings::getPictureSettingsIndexForCurrentSource: the persisted
 * m_current_source is the external descriptor index + 1, with the tuner at 0.
 * The vendor UI's nine rows are TV, BACK AV, HDMI1..HDMI3, YPbPr, VGA/PC,
 * Wireless Display and AV share/APM.
 */
int oa_source_row_for_type(int type);   /* -1 when the type has no row */
int oa_source_type_for_row(int row);    /* -1 when the row is out of range */
int oa_source_picture_index(int row);   /* -1 when the row is out of range */

#ifdef __cplusplus
}
#endif

#endif
