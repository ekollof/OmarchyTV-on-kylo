# openaurora — standard GNU make build.
#
#   make                 host build into build/ (vendor HAL stubbed out)
#   make check           build and run the module lifecycle tests
#   make arm             cross build into build-arm/, musl with vendor HAL stubs
#   make vendor          fetch the vendor libs from the firmware dump
#   make clean
#   make install         PREFIX=/usr/local, DESTDIR= staged
#
# The firmware libraries are a fixed link target (we do not update the set), so the
# ARM build links them directly instead of dlopen()ing them at runtime.

PROJECT  := openaurora
VERSION  := 0.1.0

CCACHE   := $(shell command -v ccache 2>/dev/null)
CC       ?= cc
# Use ccache transparently for the default host compiler when it is installed.
# An explicit CC=... (environment or command line) is respected; the arm/tv
# wrappers below pass the cross compiler wrapped explicitly.
ifeq ($(origin CC),default)
  ifneq ($(CCACHE),)
    CC := $(CCACHE) cc
  endif
endif
CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude -D_GNU_SOURCE
LDFLAGS  ?=
LDLIBS   += -ldl -pthread -lm

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
LIBDIR   ?= $(PREFIX)/lib/$(PROJECT)

BUILD    ?= build
MODULES  := plat ticker ir ui_console panel power display audio settings service_db source ui_vendor ui_engine ui_omarchy ui_diag ui_clock ui_anim audio_curve cec audio_mixer app_launch graphics scene_display ui_theme ui_menu i18n ui_navigation ui_profile ui_list image_decode audio_amp_profile audio_amp epg tuner pvr channels lua spi
# Four-way inventory. CORE_MODULES is the hardware/platform base (SSH/build
# target): hardware access + the local data stores, no TV or UI layer.
CORE_MODULES := plat display panel ir audio source power tuner settings image_decode
HW_MODULES := plat panel display graphics scene_display audio cec ir source tuner spi power audio_amp
MW_MODULES := settings service_db channels epg pvr image_decode i18n audio_curve audio_mixer audio_amp_profile lua
UTIL_MODULES := ticker ui_clock
UI_MODULES := ui_engine ui_vendor ui_omarchy ui_theme ui_profile ui_menu ui_list ui_navigation ui_anim ui_console ui_diag
# Additive distribution layers on the base platform. Each stage is cumulative.
TV_MODULES := epg pvr channels service_db i18n
AV_MODULES := audio_curve audio_mixer audio_amp_profile audio_amp cec
UI_TIER_MODULES := ticker ui_clock ui_anim ui_engine ui_vendor ui_theme ui_profile ui_menu ui_list ui_navigation ui_omarchy lua
DEV_MODULES := ui_console ui_diag spi
GPU_MODULES := graphics scene_display
STAGE ?= core
STAGE_core := $(CORE_MODULES)
STAGE_tv := $(STAGE_core) $(TV_MODULES)
STAGE_av := $(STAGE_tv) $(AV_MODULES)
STAGE_ui := $(STAGE_av) $(UI_TIER_MODULES)
STAGE_dev := $(STAGE_ui) $(DEV_MODULES)
STAGE_full := $(MODULES)
STAGE_MODULES := $(STAGE_$(STAGE))
ifeq ($(strip $(STAGE_MODULES)),)
$(error unknown STAGE '$(STAGE)'; use core, tv, av, ui, dev or full)
endif
empty :=
space := $(empty) $(empty)
comma := ,
stage_autoload = $(subst $(space),$(comma),$(1))
EXTRA_MODULES := $(filter-out $(CORE_MODULES),$(MODULES))
# only these modules call the vendor HAL, so only they get the dependency
VENDOR_MODULES := plat panel audio source audio_source display audio_amp
# modules that need a third-party library (sqlite) rather than the vendor HAL
SETTINGS_MODULES := settings service_db
SQLITE_LIBS := -lsqlite3

# --- vendor HAL ------------------------------------------------------------
# The vendor libs are glibc 2.23 binaries, so only a glibc target can link them.
#   TARGET=host : laptop build, vendor HAL stubbed  (make)
#   TARGET=arm  : ARM + musl, vendor HAL stubbed    (make arm; sanity/size only)
#   TARGET=tv   : ARM + glibc, links the vendor HAL (make tv TV_CROSS=...)
TARGET   ?= host
VENDOR   ?= vendor
VENDOR_LIB := $(VENDOR)/lib
CROSS    ?= arm-linux-musleabihf-gcc
TV_CROSS ?=

ifeq ($(TARGET),tv)
  ALSA_DIAGNOSTIC_LIBS := -L$(VENDOR_LIB) -Wl,--allow-shlib-undefined -l:libasound.so.2 \
                          -Wl,-rpath,/vendor/lib:/vendor/lib/libs
  VENDOR_CPPFLAGS :=
  VENDOR_LDFLAGS  := -L$(VENDOR_LIB) -Wl,--no-as-needed \
	                   -l:libNDAL3.so -l:libNDAL_EX.so -l:libasound.so.2 \
	                   -l:libv4l2.so.0 -l:libv4lconvert.so.0 -l:libnteec.so -l:libmhash.so.2 \
	                   -Wl,-rpath,/vendor/lib:/vendor/lib/libs -Wl,--allow-shlib-undefined
else
  ALSA_DIAGNOSTIC_LIBS := -lasound
  VENDOR_CPPFLAGS := -DOA_NO_VENDOR
  VENDOR_LDFLAGS  :=
endif
CPPFLAGS += $(VENDOR_CPPFLAGS)

# The Arm GNU 10.2 libm defaults log/pow/exp/log2/log2f to GLIBC_2.27/2.29.
# The TV is glibc 2.23 and provides those symbols at GLIBC_2.4. Link that
# libm when this compiler is the glibc hard-float cross compiler.
TV_LIBM := $(abspath ../artifacts/workbench/rootfs/root/lib/libm.so.6)
ifneq ($(findstring gnueabihf,$(CC)),)
  ifneq ($(wildcard $(TV_LIBM)),)
    LIBM := $(TV_LIBM)
  endif
endif
LIBM ?= -lm

# ccache stores a -c compilation and does not store a -shared link. Each
# shared library therefore compiles its sources to PIC objects, then links
# those objects. The object tree is per library so differing -D and
# visibility flags do not share an object. Compile flags stay on -c; the
# flags after -shared stay on the link. CFLAGS, including -Werror and
# -pedantic, are unchanged.
PIC_CPPFLAGS_SOURCE_FAKE = $(filter-out -DOA_NO_VENDOR,$(CPPFLAGS))

pic_obj = $(BUILD)/pic/$(1)/$(patsubst %.c,%.o,$(2))
pic_objs = $(foreach s,$(2),$(call pic_obj,$(1),$(s)))

define emit_one_pic
$(call pic_obj,$(1),$(2)): $(2) $(4)
	@mkdir -p $$(dir $$@)
	$$(CC) $(or $($(5)),$$(CPPFLAGS)) $(3) $$(CFLAGS) -fPIC -c -MMD -MP -o $$@ $$<
endef

define emit_pics
$(foreach s,$(2),$(eval $(call emit_one_pic,$(1),$(s),$(3),$(4),$(5))))
endef

# Pattern-rule objects (abi, phase, skin) are intermediate. An empty
# .SECONDARY keeps them, so make does not delete a compilation ccache stored.
.SECONDARY:

CORE_SRC := $(wildcard core/*.c)
CORE_OBJ := $(CORE_SRC:core/%.c=$(BUILD)/core/%.o) $(BUILD)/common/audio_speaker.o $(BUILD)/common/cec.o
CORE_DEP := $(CORE_OBJ:.o=.d)

MOD_SO   := $(foreach m,$(MODULES),$(BUILD)/modules/mod_$(m).so)
CORE_SO  := $(foreach m,$(CORE_MODULES),$(BUILD)/modules/mod_$(m).so)
STAGE_SO := $(foreach m,$(STAGE_MODULES),$(BUILD)/modules/mod_$(m).so)

all: $(BUILD)/$(PROJECT) $(MOD_SO) $(BUILD)/oa-supervise $(BUILD)/oa-ir-blast $(BUILD)/oa-audio-broker $(BUILD)/fb-restore

$(BUILD)/tests/settings_hotel: tests/settings_hotel.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Ithird_party $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS) -L$(VENDOR_LIB) $(SQLITE_LIBS)
.PHONY: check-settings-hotel
check-settings-hotel: $(BUILD)/tests/settings_hotel $(BUILD)/modules/mod_settings.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/settings_hotel $(BUILD)/modules
check: check-settings-hotel
.PHONY: check-settings-hotel-ipc
check-settings-hotel-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_settings.so
	node tests/settings_hotel_ipc.cjs $(abspath $(BUILD))

$(BUILD)/modules/mod_ui_profile.so: $(call pic_objs,modules__mod_ui_profile,modules/ui_profile/ui_profile.c common/profile.c common/tab_policy.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_profile,modules/ui_profile/ui_profile.c common/profile.c common/tab_policy.c,-pthread,include/openaurora/profile.h include/openaurora/tab_policy.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/ui_profile: tests/ui_profile.c core/host.h include/openaurora/services.h include/openaurora/tab_policy.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)
.PHONY: check-ui-profile-quiesce
check-ui-profile-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_profile.so
	node tests/ui_profile_quiesce.cjs $(abspath $(BUILD))
check: check-ui-profile-quiesce

.PHONY: check-ui-profile
check-ui-profile: $(BUILD)/tests/ui_profile $(BUILD)/modules/mod_ui_profile.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_profile $(BUILD)/modules
check: check-ui-profile

$(BUILD)/modules/mod_ui_navigation.so: $(call pic_objs,modules__mod_ui_navigation,modules/ui_navigation/ui_navigation.c common/launcher_route.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_navigation,modules/ui_navigation/ui_navigation.c common/launcher_route.c,-pthread,include/openaurora/launcher_route.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/ui_navigation: tests/ui_navigation.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)
.PHONY: check-ui-navigation
check-ui-navigation: $(BUILD)/tests/ui_navigation $(BUILD)/modules/mod_ui_navigation.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_navigation $(BUILD)/modules
check: check-ui-navigation

.PHONY: check-ui-navigation-quiesce
check-ui-navigation-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_navigation.so
	node tests/ui_navigation_quiesce.cjs $(abspath $(BUILD))
check: check-ui-navigation-quiesce

.PHONY: check-ui-navigation-ipc
check-ui-navigation-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_navigation.so
	node tests/ui_navigation_ipc.cjs $(abspath $(BUILD))

$(BUILD)/modules/mod_ui_list.so: $(call pic_objs,modules__mod_ui_list,modules/ui_list/ui_list.c common/svg_list_input.c common/svg_list_bodies.c common/svg_list_move.c common/svg_list_rows.c common/svg_list_impl.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_list,modules/ui_list/ui_list.c common/svg_list_input.c common/svg_list_bodies.c common/svg_list_move.c common/svg_list_rows.c common/svg_list_impl.c,-pthread,include/openaurora/svg_list_input.h include/openaurora/svg_list_bodies.h include/openaurora/svg_list_move.h include/openaurora/svg_list_rows.h include/openaurora/svg_list_impl.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_image_decode.so: $(call pic_objs,modules__mod_image_decode,modules/image_decode/image_decode.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) -ldl
$(eval $(call emit_pics,modules__mod_image_decode,modules/image_decode/image_decode.c,-pthread,include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/ui_list: tests/ui_list.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)
.PHONY: check-ui-list
check-ui-list: $(BUILD)/tests/ui_list $(BUILD)/modules/mod_ui_list.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_list $(BUILD)/modules
check: check-ui-list

.PHONY: check-ui-list-quiesce
check-ui-list-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_list.so
	node tests/ui_list_quiesce.cjs $(abspath $(BUILD))
check: check-ui-list-quiesce

.PHONY: check-ui-list-ipc
check-ui-list-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_list.so
	node tests/ui_list_ipc.cjs $(abspath $(BUILD))

$(BUILD)/tests/launcher_route: tests/launcher_route.c common/launcher_route.c include/openaurora/launcher_route.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-launcher-route
check-launcher-route: $(BUILD)/tests/launcher_route
	@$(BUILD)/tests/launcher_route
check: check-launcher-route

$(BUILD)/tests/tab_policy: tests/tab_policy.c common/tab_policy.c common/profile.c include/openaurora/tab_policy.h include/openaurora/profile.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-tab-policy
check-tab-policy: $(BUILD)/tests/tab_policy
	@$(BUILD)/tests/tab_policy
check: check-tab-policy

$(BUILD)/tests/tab_events: tests/tab_events.c common/tab_events.c include/openaurora/tab_events.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-tab-events
check-tab-events: $(BUILD)/tests/tab_events
	@$(BUILD)/tests/tab_events
check: check-tab-events

$(BUILD)/tests/tab_prehandle: tests/tab_prehandle.c tests/tab_prehandle_fixture.h common/tab_prehandle.c include/openaurora/tab_prehandle.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-tab-prehandle
check-tab-prehandle: $(BUILD)/tests/tab_prehandle
	@$(BUILD)/tests/tab_prehandle
check: check-tab-prehandle

$(BUILD)/tests/tab_dispatch: tests/tab_dispatch.c tests/tab_prehandle_fixture.h common/tab_dispatch.c common/tab_prehandle.c include/openaurora/tab_dispatch.h include/openaurora/tab_prehandle.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-tab-dispatch
check-tab-dispatch: $(BUILD)/tests/tab_dispatch
	@$(BUILD)/tests/tab_dispatch
check: check-tab-dispatch

$(BUILD)/tests/tab_leaf: tests/tab_leaf.c tests/tab_leaf_fixture.h common/tab_leaf.c include/openaurora/tab_leaf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-tab-leaf
check-tab-leaf: $(BUILD)/tests/tab_leaf
	@$(BUILD)/tests/tab_leaf
check: check-tab-leaf

$(BUILD)/tests/settings_input: tests/settings_input.c tests/settings_input_fixture.h common/settings_input.c include/openaurora/settings_input.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-input
check-settings-input: $(BUILD)/tests/settings_input
	@$(BUILD)/tests/settings_input
check: check-settings-input

$(BUILD)/tests/settings_navigation: tests/settings_navigation.c tests/settings_navigation_fixture.h common/settings_navigation.c include/openaurora/settings_navigation.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-navigation
check-settings-navigation: $(BUILD)/tests/settings_navigation
	@$(BUILD)/tests/settings_navigation
check: check-settings-navigation

$(BUILD)/tests/settings_list_input: tests/settings_list_input.c tests/settings_list_fixture.h common/settings_list_input.c include/openaurora/settings_list_input.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-list
check-settings-list: $(BUILD)/tests/settings_list_input
	@$(BUILD)/tests/settings_list_input
check: check-settings-list

$(BUILD)/tests/svg_list_input: tests/svg_list_input.c common/svg_list_input.c include/openaurora/svg_list_input.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-input
check-svg-list-input: $(BUILD)/tests/svg_list_input
	@$(BUILD)/tests/svg_list_input
$(BUILD)/tests/svg_list_bodies: tests/svg_list_bodies.c common/svg_list_bodies.c include/openaurora/svg_list_bodies.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-bodies
check-svg-list-bodies: $(BUILD)/tests/svg_list_bodies
	@$(BUILD)/tests/svg_list_bodies
check: check-svg-list-bodies

$(BUILD)/tests/profile: tests/profile.c common/profile.c common/tab_policy.c include/openaurora/profile.h include/openaurora/tab_policy.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-profile
check-profile: $(BUILD)/tests/profile
	@$(BUILD)/tests/profile
check: check-profile

$(BUILD)/tests/svg_list_move: tests/svg_list_move.c common/svg_list_move.c include/openaurora/svg_list_move.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-move
check-svg-list-move: $(BUILD)/tests/svg_list_move
	@$(BUILD)/tests/svg_list_move
check: check-svg-list-move

$(BUILD)/tests/svg_list_rows: tests/svg_list_rows.c common/svg_list_rows.c include/openaurora/svg_list_rows.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-rows
check-svg-list-rows: $(BUILD)/tests/svg_list_rows
	@$(BUILD)/tests/svg_list_rows
check: check-svg-list-rows

$(BUILD)/tests/svg_list_scroll: tests/svg_list_scroll.c common/svg_list_scroll.c include/openaurora/svg_list_scroll.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-scroll
check-svg-list-scroll: $(BUILD)/tests/svg_list_scroll
	@$(BUILD)/tests/svg_list_scroll
check: check-svg-list-scroll

$(BUILD)/tests/svg_list_impl: tests/svg_list_impl.c common/svg_list_impl.c include/openaurora/svg_list_impl.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-impl
check-svg-list-impl: $(BUILD)/tests/svg_list_impl
	@$(BUILD)/tests/svg_list_impl
check: check-svg-list-impl

$(BUILD)/tests/svg_list_scrollbar: tests/svg_list_scrollbar.c common/svg_list_scrollbar.c include/openaurora/svg_list_scrollbar.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-scrollbar
check-svg-list-scrollbar: $(BUILD)/tests/svg_list_scrollbar
	@$(BUILD)/tests/svg_list_scrollbar
check: check-svg-list-scrollbar

$(BUILD)/tests/svg_list_update: tests/svg_list_update.c common/svg_list_rows.c common/svg_list_update.c include/openaurora/svg_list_rows.h include/openaurora/svg_list_update.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-update
check-svg-list-update: $(BUILD)/tests/svg_list_update
	@$(BUILD)/tests/svg_list_update
check: check-svg-list-update

$(BUILD)/tests/svg_list_widgets: tests/svg_list_widgets.c common/svg_list_widgets.c include/openaurora/svg_list_widgets.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-widgets
check-svg-list-widgets: $(BUILD)/tests/svg_list_widgets
	@$(BUILD)/tests/svg_list_widgets
check: check-svg-list-widgets

$(BUILD)/tests/settings_draw: tests/settings_draw.c tests/settings_draw_fixture.h common/settings_draw.c include/openaurora/settings_draw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-draw
check-settings-draw: $(BUILD)/tests/settings_draw
	@$(BUILD)/tests/settings_draw
check: check-settings-draw

$(BUILD)/tests/settings_model: tests/settings_model.c common/settings_model.c common/settings_draw.c include/openaurora/settings_model.h include/openaurora/settings_draw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-model
check-settings-model: $(BUILD)/tests/settings_model
	@$(BUILD)/tests/settings_model
check: check-settings-model

$(BUILD)/tests/tab_policy_firmware: common/settings_draw.c include/openaurora/settings_draw.h tests/settings_draw_fixture.h tests/settings_draw_native.h
$(BUILD)/tests/tab_policy_firmware: tests/settings_row_type_native.h common/settings_row_type.c include/openaurora/settings_row_type.h
$(BUILD)/tests/tab_policy_firmware: tests/settings_row_setters_native.h common/settings_row_setters.c include/openaurora/settings_row_setters.h
$(BUILD)/tests/tab_policy_firmware: tests/settings_row_render_native.h common/settings_row_render.c include/openaurora/settings_row_render.h
$(BUILD)/tests/tab_policy_firmware: tests/launcher_image_id_native.h common/launcher_image_id.c include/openaurora/launcher_image_id.h
$(BUILD)/tests/tab_policy_firmware: tests/svg_image_native.h common/svg_image.c include/openaurora/svg_image.h
$(BUILD)/tests/tab_policy_firmware: tests/settings_row_construct_native.h common/settings_row_construct.c include/openaurora/settings_row_construct.h
$(BUILD)/tests/tab_policy_firmware: tests/svg_widget_state_native.h common/svg_widget_state.c include/openaurora/svg_widget_state.h
$(BUILD)/tests/tab_policy_firmware: tests/launcher_item_native.h common/launcher_item.c include/openaurora/launcher_item.h
$(BUILD)/tests/tab_policy_firmware: tests/launcher_info_native.h
$(BUILD)/tests/tab_policy_firmware: tests/svg_scrollbar_native.h common/svg_scrollbar.c include/openaurora/svg_scrollbar.h
$(BUILD)/tests/tab_policy_firmware: tests/svg_scrollbar_skin_native.h
$(BUILD)/tests/tab_policy_firmware: tests/svg_scrollbar_render_native.h
$(BUILD)/tests/tab_policy_firmware: tests/audio_amp_native.h common/audio_amp.c include/openaurora/audio_amp.h
$(BUILD)/tests/tab_policy_firmware: tests/profile_data_native.h
$(BUILD)/tests/tab_policy_firmware: tests/amp_init_native.h common/amp_init.c include/openaurora/amp_init.h

$(BUILD)/tests/settings_row_type: tests/settings_row_type.c common/settings_row_type.c common/settings_model.c common/settings_draw.c include/openaurora/settings_row_type.h include/openaurora/settings_model.h include/openaurora/settings_draw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-row-type
check-settings-row-type: $(BUILD)/tests/settings_row_type
	@$(BUILD)/tests/settings_row_type
check: check-settings-row-type

$(BUILD)/tests/settings_row_setters: tests/settings_row_setters.c common/settings_row_setters.c include/openaurora/settings_row_setters.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-row-setters
check-settings-row-setters: $(BUILD)/tests/settings_row_setters
	@$(BUILD)/tests/settings_row_setters
check: check-settings-row-setters

$(BUILD)/tests/settings_row_render: tests/settings_row_render.c common/settings_row_render.c common/settings_row_setters.c common/launcher_image_id.c include/openaurora/settings_row_render.h include/openaurora/settings_row_setters.h include/openaurora/launcher_image_id.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-row-render
check-settings-row-render: $(BUILD)/tests/settings_row_render
	@$(BUILD)/tests/settings_row_render
check: check-settings-row-render

$(BUILD)/tests/launcher_image_id: tests/launcher_image_id.c common/launcher_image_id.c include/openaurora/launcher_image_id.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-launcher-image-id
check-launcher-image-id: $(BUILD)/tests/launcher_image_id
	@$(BUILD)/tests/launcher_image_id
check: check-launcher-image-id

$(BUILD)/tests/svg_scrollbar: tests/svg_scrollbar.c common/svg_scrollbar.c include/openaurora/svg_scrollbar.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
$(BUILD)/tests/svg_scrollbar_skin: tests/svg_scrollbar_skin.c common/svg_scrollbar.c include/openaurora/svg_scrollbar.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
$(BUILD)/tests/svg_scrollbar_render: tests/svg_scrollbar_render.c common/svg_scrollbar.c include/openaurora/svg_scrollbar.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-scrollbar
check-svg-scrollbar: $(BUILD)/tests/svg_scrollbar $(BUILD)/tests/svg_scrollbar_skin $(BUILD)/tests/svg_scrollbar_render
	@$(BUILD)/tests/svg_scrollbar
	@$(BUILD)/tests/svg_scrollbar_skin
	@$(BUILD)/tests/svg_scrollbar_render
check: check-svg-scrollbar

$(BUILD)/tests/launcher_item: tests/launcher_item.c common/launcher_item.c include/openaurora/launcher_item.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-launcher-item
check-launcher-item: $(BUILD)/tests/launcher_item
	@$(BUILD)/tests/launcher_item
check: check-launcher-item

$(BUILD)/tests/launcher_db: tests/launcher_db.c common/launcher_db.c include/openaurora/launcher_db.h include/openaurora/launcher_item.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Ithird_party $(LDFLAGS) -o $@ $(filter %.c,$^) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)
.PHONY: check-launcher-db
check-launcher-db: $(BUILD)/tests/launcher_db
	@$(BUILD)/tests/launcher_db $(CURDIR)/../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db || test $$? -eq 77
check: check-launcher-db

$(BUILD)/tests/launcher_db_module: tests/launcher_db_module.c common/launcher_db.c core/host.h include/openaurora/services.h include/openaurora/launcher_db.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Ithird_party $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)
.PHONY: check-launcher-db-module
check-launcher-db-module: $(BUILD)/tests/launcher_db_module $(BUILD)/modules/mod_settings.so
	@$(BUILD)/tests/launcher_db_module $(BUILD)/modules $(CURDIR)/../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db || test $$? -eq 77
check: check-launcher-db-module

$(BUILD)/tests/mod_broadcast_fixture.so: $(call pic_objs,tests__mod_broadcast_fixture,tests/broadcast_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^
$(eval $(call emit_pics,tests__mod_broadcast_fixture,tests/broadcast_fixture.c,,include/openaurora/broadcast.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/broadcast_services: tests/broadcast_services.c core/host.h include/openaurora/services.h include/openaurora/broadcast.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-broadcast-services
check-broadcast-services: $(BUILD)/tests/broadcast_services $(BUILD)/tests/mod_broadcast_fixture.so
	@$(BUILD)/tests/broadcast_services $(BUILD)/tests
check: check-broadcast-services

.PHONY: check-broadcast-ipc
check-broadcast-ipc: $(BUILD)/$(PROJECT) $(BUILD)/tests/mod_broadcast_fixture.so
	node tests/broadcast_ipc.cjs $(abspath $(BUILD))
check: check-broadcast-ipc

$(BUILD)/tests/eit_time: tests/eit_time.c common/eit_time.c include/openaurora/eit_time.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-eit-time
check-eit-time: $(BUILD)/tests/eit_time
	@$(BUILD)/tests/eit_time
check: check-eit-time

$(BUILD)/tests/eit_cache: tests/eit_cache.c common/eit_cache.c common/eit_time.c include/openaurora/eit_cache.h include/openaurora/eit_time.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-eit-cache
check-eit-cache: $(BUILD)/tests/eit_cache
	@$(BUILD)/tests/eit_cache
check: check-eit-cache

$(BUILD)/tests/eit_section: tests/eit_section.c common/eit_section.c common/eit_time.c common/eit_ts.c include/openaurora/eit_section.h include/openaurora/eit_time.h include/openaurora/eit_ts.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-eit-section
check-eit-section: $(BUILD)/tests/eit_section
	@$(BUILD)/tests/eit_section
check: check-eit-section

$(BUILD)/tests/eit_ts_fixture: tests/eit_ts_fixture.c common/eit_section.c common/eit_time.c include/openaurora/eit_section.h include/openaurora/eit_time.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/nim_stub: tests/nim_stub.c common/nim_stub.c common/nim_ts.c common/nim_ndal.c common/eit_dvb.c common/eit_ts.c common/eit_section.c common/eit_time.c include/openaurora/nim.h include/openaurora/eit_dvb.h include/openaurora/eit_section.h include/openaurora/eit_ts.h include/openaurora/eit_time.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)
.PHONY: check-nim-stub
check-nim-stub: $(BUILD)/tests/nim_stub
	@$(BUILD)/tests/nim_stub
check: check-nim-stub

$(BUILD)/tests/pvr_stub: tests/pvr_stub.c common/pvr_stub.c common/dvr_sink.c include/openaurora/pvr.h include/openaurora/dvr.h include/openaurora/broadcast.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)
.PHONY: check-pvr-stub
check-pvr-stub: $(BUILD)/tests/pvr_stub
	@$(BUILD)/tests/pvr_stub
check: check-pvr-stub

$(BUILD)/tests/channels: tests/channels.c common/channels.c include/openaurora/channels.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-channels
check-channels: $(BUILD)/tests/channels
	@$(BUILD)/tests/channels
check: check-channels

.PHONY: check-channels-quiesce
check-channels-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_channels.so
	node tests/channels_quiesce.cjs $(abspath $(BUILD))
check: check-channels-quiesce

.PHONY: check-channels-ipc
check-channels-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_channels.so
	node tests/channels_ipc.cjs $(abspath $(BUILD))
check: check-channels-ipc

.PHONY: check-epg-ipc
check-epg-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_epg.so
	node tests/epg_ipc.cjs $(abspath $(BUILD))
check: check-epg-ipc

.PHONY: check-epg-quiesce
check-epg-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_epg.so
	node tests/epg_quiesce.cjs $(abspath $(BUILD))
check: check-epg-quiesce

.PHONY: check-epg-persist-ipc
check-epg-persist-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_epg.so
	node tests/epg_persist_ipc.cjs $(abspath $(BUILD))
check: check-epg-persist-ipc

.PHONY: check-tuner-ipc
check-tuner-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_epg.so $(BUILD)/modules/mod_tuner.so
	node tests/tuner_ipc.cjs $(abspath $(BUILD))
check: check-tuner-ipc

.PHONY: check-tuner-quiesce
check-tuner-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_tuner.so $(BUILD)/tests/eit_ts_fixture
	node tests/tuner_quiesce.cjs $(abspath $(BUILD))
check: check-tuner-quiesce

.PHONY: check-tuner-ts-ipc
check-tuner-ts-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_epg.so $(BUILD)/modules/mod_tuner.so $(BUILD)/tests/eit_ts_fixture
	node tests/tuner_ts_ipc.cjs $(abspath $(BUILD))
check: check-tuner-ts-ipc

.PHONY: check-pvr-quiesce
check-pvr-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_pvr.so
	node tests/pvr_quiesce.cjs $(abspath $(BUILD))
check: check-pvr-quiesce

.PHONY: check-pvr-ipc
check-pvr-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_pvr.so
	node tests/pvr_ipc.cjs $(abspath $(BUILD))
check: check-pvr-ipc

$(BUILD)/easi-probe: tools/probe/easi-probe.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/oa-client: tools/probe/oa-client.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/dfbkey: tools/probe/dfbkey.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/tests/easi_probe: tests/easi_probe.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

.PHONY: check-easi-probe
check-easi-probe: $(BUILD)/tests/easi_probe $(BUILD)/easi-probe
	@$(BUILD)/tests/easi_probe $(abspath $(BUILD)/easi-probe)
check: check-easi-probe

$(BUILD)/tests/service_db: tests/service_db.c common/service_db.c core/service_query.c include/openaurora/service_db.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Ithird_party $(LDFLAGS) -o $@ $(filter %.c,$^) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)
.PHONY: check-service-db
check-service-db: $(BUILD)/tests/service_db
	@$(BUILD)/tests/service_db $(CURDIR)/../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db probe || test $$? -eq 77
check: check-service-db

$(BUILD)/tests/service_db_module: tests/service_db_module.c common/service_db.c core/host.h include/openaurora/services.h include/openaurora/service_db.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Ithird_party $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)
.PHONY: check-service-db-module
check-service-db-module: $(BUILD)/tests/service_db_module $(BUILD)/modules/mod_service_db.so
	@$(BUILD)/tests/service_db_module $(BUILD)/modules $(CURDIR)/../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db || test $$? -eq 77
check: check-service-db-module

.PHONY: check-service-db-quiesce
check-service-db-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_service_db.so
	node tests/service_db_quiesce.cjs $(abspath $(BUILD))
check: check-service-db-quiesce

.PHONY: check-service-db-ipc
check-service-db-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_service_db.so
	node tests/service_db_ipc.cjs $(abspath $(BUILD))
check: check-service-db-ipc

$(BUILD)/tests/svg_widget_state: tests/svg_widget_state.c common/svg_widget_state.c include/openaurora/svg_widget_state.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-widget-state
check-svg-widget-state: $(BUILD)/tests/svg_widget_state
	@$(BUILD)/tests/svg_widget_state
check: check-svg-widget-state

$(BUILD)/tests/settings_row_construct: tests/settings_row_construct.c common/settings_row_construct.c include/openaurora/settings_row_construct.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-settings-row-construct
check-settings-row-construct: $(BUILD)/tests/settings_row_construct
	@$(BUILD)/tests/settings_row_construct
check: check-settings-row-construct

$(BUILD)/tests/svg_image: tests/svg_image.c common/svg_image.c include/openaurora/svg_image.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-image
check-svg-image: $(BUILD)/tests/svg_image
	@$(BUILD)/tests/svg_image
check: check-svg-image

$(BUILD)/tests/svg_list_anim: tests/svg_list_anim.c tests/svg_list_anim_fixture.h common/svg_list_anim.c include/openaurora/svg_list_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-svg-list-anim
check-svg-list-anim: $(BUILD)/tests/svg_list_anim
	@$(BUILD)/tests/svg_list_anim
check: check-svg-list-anim

$(BUILD)/tests/tab_policy_firmware: tests/tab_policy_firmware.c common/tab_policy.c common/profile.c common/tab_events.c common/tab_prehandle.c common/tab_dispatch.c common/tab_leaf.c common/settings_input.c common/settings_navigation.c common/settings_list_input.c common/svg_list_input.c common/svg_list_bodies.c common/svg_list_move.c common/svg_list_rows.c common/svg_list_scroll.c common/svg_list_impl.c common/svg_list_scrollbar.c common/svg_list_update.c common/svg_list_widgets.c common/svg_list_anim.c tests/tab_prehandle_fixture.h tests/tab_leaf_fixture.h tests/settings_input_fixture.h tests/settings_navigation_fixture.h tests/settings_list_fixture.h include/openaurora/tab_policy.h include/openaurora/profile.h include/openaurora/tab_events.h include/openaurora/tab_prehandle.h include/openaurora/tab_dispatch.h include/openaurora/tab_leaf.h include/openaurora/settings_input.h include/openaurora/settings_navigation.h include/openaurora/settings_list_input.h include/openaurora/svg_list_input.h include/openaurora/svg_list_bodies.h include/openaurora/svg_list_move.h include/openaurora/svg_list_rows.h include/openaurora/svg_list_scroll.h include/openaurora/svg_list_impl.h include/openaurora/svg_list_scrollbar.h include/openaurora/svg_list_update.h include/openaurora/svg_list_widgets.h include/openaurora/svg_list_anim.h tests/svg_list_anim_fixture.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/ui-object-scan: tools/re/ui-object-scan.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/tests/ui_object_scan: tests/ui_object_scan.c tools/re/ui-object-scan.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

.PHONY: check-ui-object-scan
check-ui-object-scan: $(BUILD)/tests/ui_object_scan
	@$(BUILD)/tests/ui_object_scan

check: check-ui-object-scan

$(BUILD)/modules/mod_ui_theme.so: $(call pic_objs,modules__mod_ui_theme,modules/ui_theme/ui_theme.c common/svg_pack.c common/binsvg.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_theme,modules/ui_theme/ui_theme.c common/svg_pack.c common/binsvg.c,,include/openaurora/svg_pack.h include/openaurora/binsvg.h include/openaurora/text_layout.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_ui_menu.so: $(call pic_objs,modules__mod_ui_menu,modules/ui_menu/ui_menu.c common/menu_model.c common/json.c common/launcher_anim.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_menu,modules/ui_menu/ui_menu.c common/menu_model.c common/json.c common/launcher_anim.c,-fvisibility=hidden -pthread,include/openaurora/menu_model.h include/openaurora/json.h include/openaurora/launcher_anim.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_i18n.so: $(call pic_objs,modules__mod_i18n,modules/i18n/i18n.c common/langprofile.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_i18n,modules/i18n/i18n.c common/langprofile.c,-fvisibility=hidden -pthread,include/openaurora/langprofile.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/binsvg: tests/binsvg.c common/binsvg.c common/text_layout.c include/openaurora/binsvg.h include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-binsvg
check-binsvg: $(BUILD)/tests/binsvg
	@$(BUILD)/tests/binsvg

check: check-binsvg

$(BUILD)/tests/ui_theme: tests/ui_theme.c common/svg_pack.c common/binsvg.c core/host.h include/openaurora/svg_pack.h include/openaurora/binsvg.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c,$^) $(filter %.o,$^) $(LDLIBS)

.PHONY: check-ui-theme-quiesce
check-ui-theme-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_theme.so
	node tests/ui_theme_quiesce.cjs $(abspath $(BUILD))
check: check-ui-theme-quiesce

.PHONY: check-ui-theme
check-ui-theme: $(BUILD)/tests/ui_theme $(BUILD)/modules/mod_ui_theme.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_theme $(BUILD)/modules

check: check-ui-theme

$(BUILD)/tests/ui_theme_skin: tests/ui_theme_skin.c tests/ui_nav_trace.h core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-ui-theme-skin
check-ui-theme-skin: $(BUILD)/tests/ui_theme_skin $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_theme.so
	@mkdir -p $(BUILD)/tests/theme-skin/modules
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/theme-skin/modules/mod_nav_display.so
	@cp $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_theme.so $(BUILD)/tests/theme-skin/modules/
	@$(BUILD)/tests/ui_theme_skin $(BUILD)/tests/theme-skin/modules

check: check-ui-theme-skin

$(BUILD)/tests/ui_vendor_subs: tests/ui_vendor_subs.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-ui-vendor-subs
check-ui-vendor-subs: $(BUILD)/tests/ui_vendor_subs $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so
	@mkdir -p $(BUILD)/tests/vendor-subs/modules
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/vendor-subs/modules/mod_nav_display.so
	@cp $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/tests/vendor-subs/modules/
	@$(BUILD)/tests/ui_vendor_subs $(BUILD)/tests/vendor-subs/modules

check: check-ui-vendor-subs

$(BUILD)/tests/widget_layout: tests/widget_layout.c common/widget_layout.c common/binsvg.c common/svg_pack.c common/text_layout.c include/openaurora/widget_layout.h include/openaurora/binsvg.h include/openaurora/svg_pack.h include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-widget-layout
check-widget-layout: $(BUILD)/tests/widget_layout
	@$(BUILD)/tests/widget_layout

check: check-widget-layout

$(BUILD)/tests/textbox_layout: tests/textbox_layout.c common/widget_layout.c common/binsvg.c common/svg_pack.c common/text_layout.c common/font_pack.c common/font_bitmap.c common/font_finder.c include/openaurora/widget_layout.h include/openaurora/binsvg.h include/openaurora/svg_pack.h include/openaurora/text_layout.h include/openaurora/font_pack.h include/openaurora/font_bitmap.h include/openaurora/font_finder.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-textbox-layout
check-textbox-layout: $(BUILD)/tests/textbox_layout
	@$(BUILD)/tests/textbox_layout

check: check-textbox-layout

$(BUILD)/tests/font_box: tests/font_box.c common/font_box.c common/font_text.c common/font_draw.c common/font_bitmap.c common/font_pack.c common/canvas.c common/text_origin.c common/widget_layout.c common/binsvg.c common/svg_pack.c common/text_layout.c include/openaurora/font_box.h include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/canvas.h include/openaurora/text_origin.h include/openaurora/widget_layout.h include/openaurora/binsvg.h include/openaurora/svg_pack.h include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-font-box
check-font-box: $(BUILD)/tests/font_box
	@$(BUILD)/tests/font_box

check: check-font-box

$(BUILD)/tests/font_bitmap: tests/font_bitmap.c common/font_bitmap.c common/font_finder.c common/font_draw.c common/canvas.c include/openaurora/font_bitmap.h include/openaurora/font_finder.h include/openaurora/font_draw.h include/openaurora/canvas.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-font-bitmap
check-font-bitmap: $(BUILD)/tests/font_bitmap
	@$(BUILD)/tests/font_bitmap

$(BUILD)/tests/font_finder: tests/font_finder.c common/font_finder.c include/openaurora/font_finder.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-font-finder
check-font-finder: $(BUILD)/tests/font_finder
	@$(BUILD)/tests/font_finder

$(BUILD)/tests/font_draw: tests/font_draw.c common/font_draw.c common/font_bitmap.c common/canvas.c include/openaurora/font_draw.h include/openaurora/font_bitmap.h include/openaurora/canvas.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-font-draw
check-font-draw: $(BUILD)/tests/font_draw
	@$(BUILD)/tests/font_draw

$(BUILD)/tests/font_pack: tests/font_pack.c common/font_pack.c include/openaurora/font_pack.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-font-pack
check-font-pack: $(BUILD)/tests/font_pack
	@$(BUILD)/tests/font_pack

$(BUILD)/tests/font_text: tests/font_text.c common/font_text.c common/font_draw.c common/font_bitmap.c common/font_pack.c common/canvas.c include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/canvas.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-font-text
check-font-text: $(BUILD)/tests/font_text
	@$(BUILD)/tests/font_text

$(BUILD)/tests/font_line: tests/font_line.c common/font_text.c common/font_draw.c common/font_bitmap.c common/font_pack.c common/canvas.c include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/canvas.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

.PHONY: check-font-line
check-font-line: $(BUILD)/tests/font_line
	@$(BUILD)/tests/font_line

check: check-font-line

$(BUILD)/tests/text_layout: tests/text_layout.c common/text_layout.c include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-text-layout
check-text-layout: $(BUILD)/tests/text_layout
	@$(BUILD)/tests/text_layout

$(BUILD)/tests/list_metrics: tests/list_metrics.c common/text_layout.c include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-list-metrics
check-list-metrics: $(BUILD)/tests/list_metrics
	@$(BUILD)/tests/list_metrics

check: check-list-metrics

$(BUILD)/tests/slice_stretch: tests/slice_stretch.c common/text_layout.c include/openaurora/text_layout.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-slice-stretch
check-slice-stretch: $(BUILD)/tests/slice_stretch
	@$(BUILD)/tests/slice_stretch

check: check-slice-stretch

$(BUILD)/tests/text_origin: tests/text_origin.c common/text_origin.c include/openaurora/text_origin.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-text-origin
check-text-origin: $(BUILD)/tests/text_origin
	@$(BUILD)/tests/text_origin

check: check-text-origin

check: check-font-bitmap check-font-finder check-font-draw check-font-pack check-font-text check-text-layout

$(BUILD)/oa-audio-broker: tools/audio/oa-audio-broker.c common/audio_broker.c common/audio_broker_wire.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_broker.h include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/modules/mod_audio_curve.so: $(call pic_objs,modules__mod_audio_curve,modules/audio_curve/audio_curve.c common/audio_curve.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_audio_curve,modules/audio_curve/audio_curve.c common/audio_curve.c,-pthread,include/openaurora/audio_curve.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/oa-supervise: tools/boot/oa-supervise.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/oa-wdtfeed: tools/mcu/oa-wdtfeed.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/tests/supervise_child: tests/supervise_child.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/$(PROJECT): $(CORE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJ) $(LDLIBS)

$(BUILD)/common/audio_speaker.o: common/audio_speaker.c include/openaurora/audio_speaker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/common/cec.o: common/cec.c include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/core/%.o: core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/modules/mod_ui_anim.so: $(call pic_objs,modules__mod_ui_anim,modules/ui_anim/ui_anim.c common/ui_anim.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) -lm
$(eval $(call emit_pics,modules__mod_ui_anim,modules/ui_anim/ui_anim.c common/ui_anim.c,,include/openaurora/ui_anim.h))

$(BUILD)/modules/mod_ui_vendor.so: $(call pic_objs,modules__mod_ui_vendor,modules/ui_vendor/ui_vendor.c core/service_query.c common/ui_anim.c common/binsvg.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c common/font_box.c common/text_origin.c common/svg_scrollbar.c common/menu_model.c common/json.c common/launcher_anim.c common/langprofile.c common/rtl_order.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) -lm
$(eval $(call emit_pics,modules__mod_ui_vendor,modules/ui_vendor/ui_vendor.c core/service_query.c common/ui_anim.c common/binsvg.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c common/font_box.c common/text_origin.c common/svg_scrollbar.c common/menu_model.c common/json.c common/launcher_anim.c common/langprofile.c common/rtl_order.c,-fvisibility=hidden -pthread,include/openaurora/binsvg.h include/openaurora/ui_anim.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_box.h include/openaurora/text_origin.h include/openaurora/canvas.h include/openaurora/svg_scrollbar.h include/openaurora/service_db.h include/openaurora/menu_model.h include/openaurora/json.h include/openaurora/launcher_anim.h include/openaurora/langprofile.h))

$(BUILD)/modules/mod_ui_engine.so: $(call pic_objs,modules__mod_ui_engine,modules/ui_engine/ui_engine.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_engine,modules/ui_engine/ui_engine.c,-pthread,include/openaurora/ui_engine.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_ir.so: $(call pic_objs,modules__mod_ir,modules/ir/ir.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) -ldl
$(eval $(call emit_pics,modules__mod_ir,modules/ir/ir.c,-pthread,include/openaurora/events.h include/openaurora/ir_blast.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_ui_omarchy.so: $(call pic_objs,modules__mod_ui_omarchy,modules/ui_omarchy/ui_omarchy.c common/canvas.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBM)
$(eval $(call emit_pics,modules__mod_ui_omarchy,modules/ui_omarchy/ui_omarchy.c common/canvas.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c,,include/openaurora/ui_engine.h include/openaurora/events.h include/openaurora/module.h include/openaurora/services.h include/openaurora/canvas.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/font_text.h include/openaurora/font_draw.h))

$(BUILD)/modules/mod_ui_diag.so: $(call pic_objs,modules__mod_ui_diag,modules/ui_diag/ui_diag.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) -lm
$(eval $(call emit_pics,modules__mod_ui_diag,modules/ui_diag/ui_diag.c common/font_bitmap.c common/font_pack.c common/font_text.c common/font_draw.c,,include/openaurora/ui_engine.h include/openaurora/events.h include/openaurora/module.h include/openaurora/services.h include/openaurora/lua_skin.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/canvas.h))

$(BUILD)/modules/mod_audio_mixer.so: $(call pic_objs,modules__mod_audio_mixer,modules/audio_mixer/audio_mixer.c common/audio_broker_wire.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_audio_mixer,modules/audio_mixer/audio_mixer.c common/audio_broker_wire.c,-pthread,include/openaurora/module.h include/openaurora/services.h include/openaurora/audio_broker.h))

$(BUILD)/modules/mod_app_launch.so: $(call pic_objs,modules__mod_app_launch,modules/app_launch/app_launch.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_app_launch,modules/app_launch/app_launch.c,,include/openaurora/app_launch.h include/openaurora/events.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_graphics.so: $(call pic_objs,modules__mod_graphics,modules/graphics/graphics.c common/scene_client.c common/scene_broker_wire.c common/scene_broker_transport.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_graphics,modules/graphics/graphics.c common/scene_client.c common/scene_broker_wire.c common/scene_broker_transport.c,,include/openaurora/module.h include/openaurora/services.h include/openaurora/scene_client.h include/openaurora/scene_broker.h))

$(BUILD)/modules/mod_scene_display.so: $(call pic_objs,modules__mod_scene_display,modules/scene_display/scene_display.c common/canvas.c common/scene_broker_transport.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBM)
$(eval $(call emit_pics,modules__mod_scene_display,modules/scene_display/scene_display.c common/canvas.c common/scene_broker_transport.c,,include/openaurora/canvas.h include/openaurora/module.h include/openaurora/services.h include/openaurora/scene_broker.h))

$(BUILD)/modules/mod_cec.so: $(call pic_objs,modules__mod_cec,modules/cec/cec.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_cec,modules/cec/cec.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c,-pthread,include/openaurora/cec_authority_mem.h include/openaurora/cec_module_transport.h))

$(BUILD)/modules/mod_audio.so: $(call pic_objs,modules__mod_audio,modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),audio),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_audio,modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c,-pthread,))

$(BUILD)/modules/mod_audio_amp.so: $(call pic_objs,modules__mod_audio_amp,modules/audio_amp/audio_amp.c common/amp_owner.c common/amp_init.c common/amp_vendor.c common/amp_board.c common/audio_amp.c common/amp_i2c.c common/audio_amp_profile.c common/profile.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),audio_amp),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_audio_amp,modules/audio_amp/audio_amp.c common/amp_owner.c common/amp_init.c common/amp_vendor.c common/amp_board.c common/audio_amp.c common/amp_i2c.c common/audio_amp_profile.c common/profile.c,-pthread,include/openaurora/amp_board.h include/openaurora/amp_i2c.h include/openaurora/amp_init.h include/openaurora/amp_owner.h include/openaurora/amp_vendor.h include/openaurora/audio_amp.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_display.so: $(call pic_objs,modules__mod_display,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(VENDOR_LDFLAGS) $(LIBM) -pthread
$(eval $(call emit_pics,modules__mod_display,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c,-pthread,include/openaurora/ui_anim.h include/openaurora/canvas.h include/openaurora/fb_snapshot.h))

$(BUILD)/modules/mod_audio_source.so: $(call pic_objs,modules__mod_audio_source,modules/audio_source/audio_source.c common/audio_source_hal_vendor.c common/audio_curve.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),audio_source),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_audio_source,modules/audio_source/audio_source.c common/audio_source_hal_vendor.c common/audio_curve.c,-pthread,include/openaurora/module.h include/openaurora/services.h include/openaurora/audio_source_hal.h include/openaurora/audio_curve.h))

$(BUILD)/modules/mod_source.so: $(call pic_objs,modules__mod_source,modules/source/source.c common/source_hal_vendor.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),source),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_source,modules/source/source.c common/source_hal_vendor.c,-pthread,include/openaurora/module.h include/openaurora/services.h include/openaurora/source_hal.h))

$(BUILD)/modules/mod_display_fb.so: $(call pic_objs,modules__mod_display_fb,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBM) -pthread
$(eval $(call emit_pics,modules__mod_display_fb,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c,-DOA_DISPLAY_NO_PLAT -pthread,include/openaurora/ui_anim.h include/openaurora/canvas.h include/openaurora/fb_snapshot.h))

$(BUILD)/modules/mod_settings.so: $(call pic_objs,modules__mod_settings,modules/settings/settings.c common/launcher_db.c common/picture_config.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) -L$(VENDOR_LIB) $(SQLITE_LIBS)
$(eval $(call emit_pics,modules__mod_settings,modules/settings/settings.c common/launcher_db.c common/picture_config.c,-Ithird_party -pthread,include/openaurora/launcher_db.h include/openaurora/launcher_item.h include/openaurora/picture_config.h))

$(BUILD)/modules/mod_service_db.so: $(call pic_objs,modules__mod_service_db,modules/service_db/service_db.c common/service_db.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)
$(eval $(call emit_pics,modules__mod_service_db,modules/service_db/service_db.c common/service_db.c,-Ithird_party -pthread,include/openaurora/service_db.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_epg.so: $(call pic_objs,modules__mod_epg,modules/epg/epg.c common/eit_section.c common/eit_cache.c common/eit_time.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_epg,modules/epg/epg.c common/eit_section.c common/eit_cache.c common/eit_time.c,-pthread,include/openaurora/eit_section.h include/openaurora/eit_cache.h include/openaurora/eit_time.h include/openaurora/broadcast.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_tuner.so: $(call pic_objs,modules__mod_tuner,modules/tuner/tuner.c common/nim_stub.c common/nim_ts.c common/nim_ndal.c common/eit_dvb.c common/eit_ts.c common/eit_section.c common/eit_time.c common/efsl_profile.c common/tuner_identity.c common/mxl661.c common/mxl661_dev.c common/mxl661_rf.c common/mxl661_atv.c common/mxl661_tune.c common/tuner_hal.c common/tuner_sequence.c common/tuner_vendor.c common/nim_dtv.c common/nim_dtv_hal.c common/nim_dtv_hw.c common/nim_tune.c common/nim_hal.c common/lnb_rt5047.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter tv,$(TARGET)),$(VENDOR_LDFLAGS)) -ldl
$(eval $(call emit_pics,modules__mod_tuner,modules/tuner/tuner.c common/nim_stub.c common/nim_ts.c common/nim_ndal.c common/eit_dvb.c common/eit_ts.c common/eit_section.c common/eit_time.c common/efsl_profile.c common/tuner_identity.c common/mxl661.c common/mxl661_dev.c common/mxl661_rf.c common/mxl661_atv.c common/mxl661_tune.c common/tuner_hal.c common/tuner_sequence.c common/tuner_vendor.c common/nim_dtv.c common/nim_dtv_hal.c common/nim_dtv_hw.c common/nim_tune.c common/nim_hal.c common/lnb_rt5047.c,-pthread,include/openaurora/nim.h include/openaurora/eit_dvb.h include/openaurora/eit_section.h include/openaurora/eit_ts.h include/openaurora/eit_time.h include/openaurora/broadcast.h include/openaurora/module.h include/openaurora/services.h include/openaurora/efsl_profile.h include/openaurora/tuner_identity.h include/openaurora/mxl661.h include/openaurora/mxl661_dev.h include/openaurora/mxl661_rf.h include/openaurora/mxl661_atv.h include/openaurora/mxl661_tune.h include/openaurora/tuner_hal.h include/openaurora/tuner_sequence.h include/openaurora/tuner_vendor.h include/openaurora/nim_dtv.h include/openaurora/nim_dtv_hal.h include/openaurora/nim_dtv_hw.h include/openaurora/nim_tune.h include/openaurora/nim_hal.h include/openaurora/lnb_rt5047.h include/openaurora/hal_iic.h))

$(BUILD)/modules/mod_pvr.so: $(call pic_objs,modules__mod_pvr,modules/pvr/pvr.c common/pvr_stub.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_pvr,modules/pvr/pvr.c common/pvr_stub.c,-pthread,include/openaurora/pvr.h include/openaurora/broadcast.h include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/modules/mod_channels.so: $(call pic_objs,modules__mod_channels,modules/channels/channels.c common/channels.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_channels,modules/channels/channels.c common/channels.c,,include/openaurora/channels.h include/openaurora/module.h include/openaurora/services.h))

LUA_SRC := $(sort $(wildcard third_party/lua/*.c))
LUA_OBJ := $(patsubst third_party/lua/%.c,$(BUILD)/third_party/lua/%.o,$(LUA_SRC))

$(BUILD)/third_party/lua/%.o: third_party/lua/%.c
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -Werror -pedantic,$(CFLAGS)) $(CPPFLAGS) -fPIC -w -Ithird_party/lua -c -o $@ $<

$(BUILD)/modules/lua_skin.o: common/lua_skin.c include/openaurora/lua_skin.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -Ithird_party/lua -c -o $@ $<

$(BUILD)/modules/mod_lua.so: $(call pic_objs,modules__mod_lua,modules/lua/lua.c) $(BUILD)/modules/lua_skin.o $(LUA_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBM)
$(eval $(call emit_pics,modules__mod_lua,modules/lua/lua.c,-Ithird_party/lua,include/openaurora/lua_skin.h include/openaurora/module.h))

$(BUILD)/modules/mod_panel.so: $(call pic_objs,modules__mod_panel,modules/panel/panel.c common/panel_curve.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),panel),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_panel,modules/panel/panel.c common/panel_curve.c,-pthread,include/openaurora/events.h include/openaurora/module.h include/openaurora/services.h include/openaurora/vendor_pwm.h include/openaurora/panel_curve.h))

$(BUILD)/modules/mod_plat.so: $(call pic_objs,modules__mod_plat,modules/plat/plat.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(if $(filter $(VENDOR_MODULES),plat),$(VENDOR_LDFLAGS),)
$(eval $(call emit_pics,modules__mod_plat,modules/plat/plat.c,,))

$(BUILD)/modules/mod_ticker.so: $(call pic_objs,modules__mod_ticker,modules/ticker/ticker.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ticker,modules/ticker/ticker.c,,))

$(BUILD)/modules/mod_ui_console.so: $(call pic_objs,modules__mod_ui_console,modules/ui_console/ui.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_console,modules/ui_console/ui.c,,))

$(BUILD)/modules/mod_power.so: $(call pic_objs,modules__mod_power,modules/power/power.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_power,modules/power/power.c,,))

$(BUILD)/modules/mod_spi.so: $(call pic_objs,modules__mod_spi,modules/spi/spi.c common/spi_dev.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_spi,modules/spi/spi.c common/spi_dev.c,,include/openaurora/module.h include/openaurora/services.h include/openaurora/spi_dev.h))

$(BUILD)/modules/mod_ui_clock.so: $(call pic_objs,modules__mod_ui_clock,modules/ui_clock/ui_clock.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_ui_clock,modules/ui_clock/ui_clock.c,,))

vendor:
	@./scripts/fetch-vendor.sh

arm:
	$(MAKE) TARGET=arm CC="$(if $(CCACHE),$(CCACHE) ,)$(CROSS)" BUILD=build-arm

# TV build: needs a glibc arm-linux-gnueabihf compiler. musl cannot load a glibc .so.
tv:
	@test -n "$(TV_CROSS)" || { \
	  echo "TV_CROSS is unset."; \
	  echo "The vendor HAL is glibc 2.23; set TV_CROSS to an arm-linux-gnueabihf gcc"; \
	  echo "(glibc <= 2.33). See docs/ARCHITECTURE.md 'Toolchain'."; exit 1; }
	@test -f $(VENDOR_LIB)/libNDAL3.so || { echo "run 'make vendor' first"; exit 1; }
	$(MAKE) TARGET=tv CC="$(if $(CCACHE),$(CCACHE) ,)$(TV_CROSS)" BUILD=build-tv

$(BUILD)/tests/mod_recovery_fixture.so: $(call pic_objs,tests__mod_recovery_fixture,tests/recovery_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^
$(eval $(call emit_pics,tests__mod_recovery_fixture,tests/recovery_fixture.c,,include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/mod_failed_init.so: $(call pic_objs,tests__mod_failed_init,tests/failed_init.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^
$(eval $(call emit_pics,tests__mod_failed_init,tests/failed_init.c,,include/openaurora/module.h))

$(BUILD)/tests/registry: tests/registry.c $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/tests/mod_post_worker.so: $(call pic_objs,tests__mod_post_worker,tests/post_worker.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_post_worker,tests/post_worker.c,-pthread,tests/post_worker.h include/openaurora/module.h))

$(BUILD)/tests/mod_post_fail_init.so: $(call pic_objs,tests__mod_post_fail_init,tests/post_worker.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_post_fail_init,tests/post_worker.c,-DTEST_FAIL_INIT_POST -pthread,tests/post_worker.h include/openaurora/module.h))

$(BUILD)/tests/mod_post_fail_start.so: $(call pic_objs,tests__mod_post_fail_start,tests/post_worker.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_post_fail_start,tests/post_worker.c,-DTEST_FAIL_START_POST -pthread,tests/post_worker.h include/openaurora/module.h))

$(BUILD)/pic/tests__mod_abi_%/tests/abi_fixture.o: tests/abi_fixture.c include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(if $(filter v1,$*),-DTEST_ABI_V1,) $(if $(filter v2,$*),-DTEST_ABI_V2,) $(if $(filter extended,$*),-DTEST_ABI_EXTENDED,) -fPIC -c -MMD -MP -o $@ $<

$(BUILD)/tests/mod_abi_%.so: $(BUILD)/pic/tests__mod_abi_%/tests/abi_fixture.o
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/module_safety: tests/module_safety.c tests/post_worker.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

SAFETY_FIXTURES := mod_post_worker mod_post_fail_init mod_post_fail_start mod_abi_v1 mod_abi_v2 mod_abi_short mod_abi_extended
check-safety: all check-quiesce $(BUILD)/tests/module_safety $(addprefix $(BUILD)/tests/,$(addsuffix .so,$(SAFETY_FIXTURES)))
	@$(BUILD)/tests/module_safety $(BUILD)/modules $(BUILD)/tests

$(BUILD)/tests/mod_quiesce_fixture.so: $(call pic_objs,tests__mod_quiesce_fixture,tests/quiesce_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_quiesce_fixture,tests/quiesce_fixture.c,,tests/quiesce_fixture.h include/openaurora/module.h))

$(BUILD)/tests/quiesce: tests/quiesce.c tests/quiesce_fixture.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-quiesce
check-quiesce: $(BUILD)/tests/quiesce $(BUILD)/tests/mod_quiesce_fixture.so
	@$(BUILD)/tests/quiesce $(BUILD)/tests

$(BUILD)/tests/mod_reload_fixture.so: $(call pic_objs,tests__mod_reload_fixture,tests/reload_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_reload_fixture,tests/reload_fixture.c,,tests/reload_fixture.h include/openaurora/module.h))

$(BUILD)/tests/mod_reload_no_restore.so: $(call pic_objs,tests__mod_reload_no_restore,tests/reload_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_reload_no_restore,tests/reload_fixture.c,-DRELOAD_WITHOUT_RESTORE,tests/reload_fixture.h include/openaurora/module.h))

$(BUILD)/tests/mod_reload_renamed.so: $(call pic_objs,tests__mod_reload_renamed,tests/reload_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_reload_renamed,tests/reload_fixture.c,-DRELOAD_RENAMED,tests/reload_fixture.h include/openaurora/module.h))

$(BUILD)/tests/reload: tests/reload.c tests/reload_fixture.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS) -Wl,--wrap=malloc

check-reload: $(BUILD)/tests/reload $(BUILD)/tests/mod_reload_fixture.so $(BUILD)/tests/mod_reload_no_restore.so $(BUILD)/tests/mod_reload_renamed.so
	@$(BUILD)/tests/reload $(BUILD)/tests

$(BUILD)/pic/tests__mod_phase_%/tests/phases_fixture.o: tests/phases_fixture.c tests/phases.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPHASE_ID=$(if $(filter b,$*),1,$(if $(filter c replacement,$*),2,0)) \
	  $(if $(filter no_suspend,$*),-DNO_PHASE_HOOKS,) $(if $(filter replacement,$*),-DPHASE_REPLACEMENT,) \
	  -fPIC -c -MMD -MP -o $@ $<

$(BUILD)/tests/mod_phase_%.so: $(BUILD)/pic/tests__mod_phase_%/tests/phases_fixture.o
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/phases: tests/phases.c tests/phases.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

PHASE_FIXTURES := a b c no_suspend replacement
check-phases: $(BUILD)/tests/phases $(addprefix $(BUILD)/tests/mod_phase_,$(addsuffix .so,$(PHASE_FIXTURES)))
	@$(BUILD)/tests/phases $(BUILD)/tests

$(BUILD)/tests/power_cycle: tests/power_cycle.c tests/phases.h core/host.h modules/power/power.c include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS) \
	  -Wl,--wrap=open -Wl,--wrap=write -Wl,--wrap=close -Wl,--wrap=sync

.PHONY: check-panel-quiesce
check-panel-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_panel.so $(BUILD)/modules/mod_plat.so
	node tests/panel_quiesce.cjs $(abspath $(BUILD))
check: check-panel-quiesce

$(BUILD)/tests/panel_curve: tests/panel_curve.c common/panel_curve.c include/openaurora/panel_curve.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/panel_hal: tests/panel_hal.c modules/panel/panel.c common/panel_curve.c include/openaurora/module.h include/openaurora/services.h include/openaurora/vendor_pwm.h include/openaurora/panel_curve.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

$(BUILD)/tests/panel_standby: tests/panel_standby.c modules/panel/panel.c common/panel_curve.c include/openaurora/module.h include/openaurora/services.h include/openaurora/vendor_pwm.h include/openaurora/panel_curve.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

.PHONY: check-panel-curve
check-panel-curve: $(BUILD)/tests/panel_curve $(BUILD)/tests/panel_hal $(BUILD)/tests/panel_standby
	@$(BUILD)/tests/panel_curve
	@$(BUILD)/tests/panel_hal
	@$(BUILD)/tests/panel_standby
check: check-panel-curve

.PHONY: check-cold-config
check-cold-config:
	@sh tests/cold_config.sh
check: check-cold-config

.PHONY: check-boot-gate check-wdtfeed check-stage-profiles check-supervise-boot check-deploy-staged-cold
check-boot-gate:
	@sh tests/boot_gate.sh
check-wdtfeed: $(BUILD)/oa-wdtfeed
	@sh tests/wdtfeed.sh $(abspath $(BUILD)/oa-wdtfeed)
check-stage-profiles:
	@sh tests/stage_profiles.sh
check-supervise-boot: $(BUILD)/oa-supervise $(BUILD)/tests/supervise_child
	@sh tests/supervise_boot.sh $(abspath $(BUILD)/oa-supervise) $(abspath $(BUILD)/tests/supervise_child)
check-deploy-staged-cold:
	@sh tests/deploy_staged_cold.sh
check: check-boot-gate check-wdtfeed check-stage-profiles check-supervise-boot check-deploy-staged-cold

$(BUILD)/tests/nim_dtv: tests/nim_dtv.c common/nim_dtv.c include/openaurora/nim_dtv.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/nim_dtv_hal: tests/nim_dtv_hal.c common/nim_dtv_hal.c common/nim_dtv.c include/openaurora/nim_dtv.h include/openaurora/nim_dtv_hal.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-nim-dtv
check-nim-dtv: $(BUILD)/tests/nim_dtv $(BUILD)/tests/nim_dtv_hal
	@$(BUILD)/tests/nim_dtv
	@$(BUILD)/tests/nim_dtv_hal
check: check-nim-dtv

$(BUILD)/tests/efsl_profile: tests/efsl_profile.c common/efsl_profile.c common/tuner_identity.c include/openaurora/efsl_profile.h include/openaurora/tuner_identity.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-efsl
check-efsl: $(BUILD)/tests/efsl_profile
	@$(BUILD)/tests/efsl_profile
check: check-efsl

$(BUILD)/tests/tuner_hal: tests/tuner_hal.c common/tuner_hal.c include/openaurora/tuner_hal.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/mxl661: tests/mxl661.c common/mxl661.c include/openaurora/mxl661.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-tuner-hardware
check-tuner-hardware: $(BUILD)/tests/tuner_hal $(BUILD)/tests/mxl661
	@$(BUILD)/tests/tuner_hal
	@$(BUILD)/tests/mxl661
check: check-tuner-hardware

$(BUILD)/tests/mxl661_dev: tests/mxl661_dev.c common/mxl661_dev.c common/mxl661.c include/openaurora/mxl661.h include/openaurora/mxl661_dev.h include/openaurora/mxl661_rf.h include/openaurora/mxl661_tune.h include/openaurora/hal_iic.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/tuner_sequence: tests/tuner_sequence.c common/tuner_sequence.c common/tuner_identity.c common/efsl_profile.c include/openaurora/tuner_sequence.h include/openaurora/tuner_identity.h include/openaurora/efsl_profile.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/nim_dtv_hw: tests/nim_dtv_hw.c common/nim_dtv_hw.c include/openaurora/nim_dtv_hw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/lnb_rt5047: tests/lnb_rt5047.c common/lnb_rt5047.c include/openaurora/lnb_rt5047.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/nim_tune: tests/nim_tune.c common/nim_tune.c common/mxl661_tune.c include/openaurora/nim_tune.h include/openaurora/mxl661_tune.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/mxl661_tune: tests/mxl661_tune.c common/mxl661_tune.c include/openaurora/mxl661_tune.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/mxl661_rf: tests/mxl661_rf.c common/mxl661_rf.c include/openaurora/mxl661.h include/openaurora/mxl661_rf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/mxl661_atv: tests/mxl661_atv.c common/mxl661_atv.c common/mxl661_rf.c include/openaurora/mxl661.h include/openaurora/mxl661_rf.h include/openaurora/mxl661_atv.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/nim_hal: tests/nim_hal.c common/nim_hal.c common/nim_tune.c common/mxl661_tune.c include/openaurora/nim.h include/openaurora/nim_hal.h include/openaurora/nim_tune.h include/openaurora/mxl661_tune.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-tuner-backend
check-tuner-backend: $(BUILD)/tests/mxl661_dev $(BUILD)/tests/tuner_sequence $(BUILD)/tests/nim_dtv_hw $(BUILD)/tests/lnb_rt5047 $(BUILD)/tests/nim_tune $(BUILD)/tests/mxl661_tune $(BUILD)/tests/mxl661_rf $(BUILD)/tests/mxl661_atv $(BUILD)/tests/nim_hal
	@$(BUILD)/tests/mxl661_dev
	@$(BUILD)/tests/tuner_sequence
	@$(BUILD)/tests/nim_dtv_hw
	@$(BUILD)/tests/lnb_rt5047
	@$(BUILD)/tests/nim_tune
	@$(BUILD)/tests/mxl661_tune
	@$(BUILD)/tests/mxl661_rf
	@$(BUILD)/tests/mxl661_atv
	@$(BUILD)/tests/nim_hal
check: check-tuner-backend

# Explicit corpus target: uses the caller's private firmware ELF; never part of
# `make check`. The loader prints nothing and embeds no vendor data.
.PHONY: check-mxl661-atv-corpus
check-mxl661-atv-corpus: $(BUILD)/tests/mxl661_atv
	@test -f $(AURORA_ELF) || { echo "missing $(AURORA_ELF); the real aurora.elf is required" >&2; exit 2; }
	@$(BUILD)/tests/mxl661_atv $(AURORA_ELF)

$(BUILD)/tests/tuner_gate: tests/tuner_gate.c include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -ldl

.PHONY: check-tuner-gate
check-tuner-gate: $(BUILD)/tests/tuner_gate $(BUILD)/modules/mod_tuner.so
	@$(BUILD)/tests/tuner_gate $(abspath $(BUILD)/modules/mod_tuner.so)
check: check-tuner-gate

# Analog scan stepping over the offline stub backend; the real MxL661 path
# remains behind tuner.hw_init and the owner-authorized takeover.
.PHONY: check-tuner-analog-scan
check-tuner-analog-scan: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_tuner.so
	node tests/tuner_analog_scan.cjs $(abspath $(BUILD))
check: check-tuner-analog-scan

# The vendor profiles are private artifacts. This corpus target is explicit:
# it fails when they are missing or when their MD5 is not the analysed file.
.PHONY: check-efsl-corpus
check-efsl-corpus: $(BUILD)/tests/efsl_profile
	@test -f ../artifacts/dumps/apdat_files/mb230_hwprofile.bin
	@test -f ../artifacts/dumps/apdat_files/mb230_swprofile.bin
	@test "$$(md5sum ../artifacts/dumps/apdat_files/mb230_hwprofile.bin | cut -d' ' -f1)" = "1b063b2cc7faa8b92c0f843921e561c7"
	@test "$$(md5sum ../artifacts/dumps/apdat_files/mb230_swprofile.bin | cut -d' ' -f1)" = "a1d9e5e80a926a6e0abd812c98852894"
	@$(BUILD)/tests/efsl_profile $(abspath ../artifacts/dumps/apdat_files/mb230_hwprofile.bin) $(abspath ../artifacts/dumps/apdat_files/mb230_swprofile.bin)

check-power: $(BUILD)/tests/power_cycle $(addprefix $(BUILD)/tests/mod_phase_,$(addsuffix .so,$(PHASE_FIXTURES)))
	@$(BUILD)/tests/power_cycle $(BUILD)/tests

$(BUILD)/tests/ui_clock: tests/ui_clock.c core/host.h include/openaurora/ui_time.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

check-ui-clock: $(BUILD)/tests/ui_clock $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_clock $(BUILD)/modules

.PHONY: check-ui-clock-quiesce
check-ui-clock-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so
	node tests/ui_clock_quiesce.cjs $(abspath $(BUILD))
check: check-ui-clock-quiesce

.PHONY: check-ticker-quiesce
check-ticker-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ticker.so
	node tests/ticker_quiesce.cjs $(abspath $(BUILD))
check: check-ticker-quiesce

.PHONY: check-ui-console-quiesce
check-ui-console-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_console.so
	node tests/ui_console_quiesce.cjs $(abspath $(BUILD))
check: check-ui-console-quiesce

.PHONY: check-ui-omarchy-quiesce
check-ui-omarchy-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_lua.so $(BUILD)/tests/ui_nav_display.so
	node tests/ui_omarchy_quiesce.cjs $(abspath $(BUILD))
check: check-ui-omarchy-quiesce

# plat/power own no deferable cleanup; prove the synchronous service gate.
.PHONY: check-plat-power-sync-stop
check-plat-power-sync-stop: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_power.so
	@test "$(TARGET)" = host || { echo "  skip plat/power sync stop: host-only (no vendor HAL)"; exit 0; }
	node tests/plat_power_sync_stop.cjs $(abspath $(BUILD))
check: check-plat-power-sync-stop

# Vendor ICE_HardwareInit order: wait for the kernel driver marker before the HAL prelude.
.PHONY: check-plat-startup-wait
check-plat-startup-wait: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_plat.so
	@test "$(TARGET)" = host || { echo "  skip plat startup wait: host-only (no vendor HAL)"; exit 0; }
	node tests/plat_startup_wait.cjs $(abspath $(BUILD))
check: check-plat-startup-wait

$(BUILD)/tests/spi_dev: tests/spi_dev.c common/spi_dev.c include/openaurora/spi_dev.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) \
	  -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl

.PHONY: check-spi-dev
check-spi-dev: $(BUILD)/tests/spi_dev
	@$(BUILD)/tests/spi_dev
check: check-spi-dev

$(BUILD)/tests/spi_module: tests/spi_module.c modules/spi/spi.c common/spi_dev.c include/openaurora/module.h include/openaurora/services.h include/openaurora/spi_dev.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-spi-module
check-spi-module: $(BUILD)/tests/spi_module
	@$(BUILD)/tests/spi_module
check: check-spi-module

$(BUILD)/tests/sched: tests/sched.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-sched
check-sched: $(BUILD)/tests/sched
	@$(BUILD)/tests/sched
check: check-sched

$(BUILD)/tests/cec: tests/cec.c common/cec.c include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/cec_packet: tests/cec_packet.c common/cec_packet.c common/cec.c include/openaurora/cec.h include/openaurora/vendor_cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-cec-packet: $(BUILD)/tests/cec_packet
	@$(BUILD)/tests/cec_packet

$(BUILD)/tests/cec_device: tests/cec_device.c common/cec_device.c common/cec_packet.c common/cec.c include/openaurora/cec_device.h include/openaurora/vendor_cec.h include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-cec-device: $(BUILD)/tests/cec_device
	@$(BUILD)/tests/cec_device

$(BUILD)/tests/cec_authority_mem: tests/cec_authority_mem.c common/cec_authority_mem.c common/cec_authority.c include/openaurora/cec_authority_mem.h include/openaurora/cec_authority.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-cec-authority-mem
check-cec-authority-mem: $(BUILD)/tests/cec_authority_mem
	@$(BUILD)/tests/cec_authority_mem
check: check-cec-authority-mem

.PHONY: check-cec-transport-ipc
check-cec-transport-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_cec.so
	node tests/cec_transport_ipc.cjs $(abspath $(BUILD))
check: check-cec-transport-ipc

$(BUILD)/tests/mod_cec_fake.so: $(call pic_objs,tests__mod_cec_fake,modules/cec/cec.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c tests/cec_module_fake.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread -ldl $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_cec_fake,modules/cec/cec.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c tests/cec_module_fake.c,-pthread,include/openaurora/cec_authority_mem.h include/openaurora/cec_module_transport.h include/openaurora/vendor_cec.h))

.PHONY: check-cec-module-fake
check-cec-module-fake: $(BUILD)/$(PROJECT) $(BUILD)/tests/mod_cec_fake.so
	node tests/cec_module_fake.cjs $(abspath $(BUILD))
check: check-cec-module-fake

$(BUILD)/tests/cec_authority_lifetime: tests/cec_authority_lifetime.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c modules/cec/cec.c include/openaurora/cec_owner.h include/openaurora/cec_authority_mem.h include/openaurora/cec_module_transport.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
	  tests/cec_authority_lifetime.c common/cec_owner.c common/cec_authority.c \
	  common/cec_authority_mem.c common/cec_device.c common/cec_packet.c \
	  common/cec.c common/cec_policy.c common/cec_module_transport.c -pthread \
	  -Wl,--wrap=oa_cec_owner_join \
	  -Wl,--wrap=oa_cec_authority_mem_open \
	  -Wl,--wrap=oa_cec_authority_mem_close
.PHONY: check-cec-authority-lifetime
check-cec-authority-lifetime: $(BUILD)/tests/cec_authority_lifetime
	@$(BUILD)/tests/cec_authority_lifetime
check: check-cec-authority-lifetime

$(BUILD)/tests/cec_suspend_resume: tests/cec_suspend_resume.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c modules/cec/cec.c include/openaurora/cec_owner.h include/openaurora/cec_authority_mem.h include/openaurora/cec_device.h include/openaurora/cec_module_transport.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
	  tests/cec_suspend_resume.c common/cec_owner.c common/cec_authority.c \
	  common/cec_authority_mem.c common/cec_device.c common/cec_packet.c \
	  common/cec.c common/cec_policy.c common/cec_module_transport.c -pthread \
	  -Wl,--wrap=oa_cec_owner_join \
	  -Wl,--wrap=oa_cec_authority_mem_open \
	  -Wl,--wrap=oa_cec_authority_mem_close \
	  -Wl,--wrap=oa_cec_device_open \
	  -Wl,--wrap=oa_cec_device_close
.PHONY: check-cec-suspend-resume
check-cec-suspend-resume: $(BUILD)/tests/cec_suspend_resume
	@$(BUILD)/tests/cec_suspend_resume
check: check-cec-suspend-resume

$(BUILD)/tests/cec_reply_queue: tests/cec_reply_queue.c common/cec_owner.c common/cec_authority.c common/cec_authority_mem.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c common/cec_module_transport.c modules/cec/cec.c include/openaurora/cec_owner.h include/openaurora/cec_authority_mem.h include/openaurora/cec_device.h include/openaurora/cec_module_transport.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
	  tests/cec_reply_queue.c common/cec_owner.c common/cec_authority.c \
	  common/cec_authority_mem.c common/cec_device.c common/cec_packet.c \
	  common/cec.c common/cec_policy.c common/cec_module_transport.c -pthread \
	  -Wl,--wrap=oa_cec_owner_send_async \
	  -Wl,--wrap=oa_cec_owner_take \
	  -Wl,--wrap=oa_cec_owner_receive_async
.PHONY: check-cec-reply-queue
check-cec-reply-queue: $(BUILD)/tests/cec_reply_queue
	@$(BUILD)/tests/cec_reply_queue
check: check-cec-reply-queue

$(BUILD)/tests/cec_owner: tests/cec_owner.c tests/lifecycle_retry.h common/cec_owner.c common/cec_authority.c common/cec_device.c common/cec_packet.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-cec-owner: check-cec-async $(BUILD)/tests/cec_owner $(BUILD)/modules/mod_cec.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/cec_owner $(BUILD)/modules

.PHONY: check-cec-quiesce
check-cec-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_cec.so
	node tests/cec_quiesce.cjs $(abspath $(BUILD))
check: check-cec-quiesce

$(BUILD)/tests/cec_async: tests/cec_async.c common/cec_owner.c common/cec_authority.c common/cec_device.c common/cec_packet.c common/cec.c include/openaurora/cec_owner.h include/openaurora/cec_authority.h include/openaurora/cec_device.h include/openaurora/vendor_cec.h include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

.PHONY: check-cec-async
check-cec-async: $(BUILD)/tests/cec_async
	@$(BUILD)/tests/cec_async

$(BUILD)/tests/audio_owner: tests/audio_owner.c common/audio_owner.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_owner.h include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

check-audio-owner: check-owner-races check-audio-async check-audio-broker $(BUILD)/tests/audio_owner
	@$(BUILD)/tests/audio_owner

$(BUILD)/tests/audio_async: tests/audio_async.c common/audio_owner.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_owner.h include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

.PHONY: check-audio-async
check-audio-async: $(BUILD)/tests/audio_async
	@$(BUILD)/tests/audio_async

$(BUILD)/tests/audio_broker: tests/audio_broker.c common/audio_broker.c common/audio_broker_wire.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_broker.h include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

.PHONY: check-audio-broker
check-audio-broker: check-mixer-module $(BUILD)/tests/audio_broker $(BUILD)/oa-audio-broker
	@$(BUILD)/tests/audio_broker $(if $(filter tv arm,$(TARGET)),,--spawn $(BUILD)/oa-audio-broker)

$(BUILD)/tests/mixer_peer: tests/mixer_peer.c common/audio_broker_wire.c include/openaurora/audio_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/tests/mixer_module: tests/mixer_module.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-mixer-module
$(BUILD)/tests/mixer_bad_exec: tests/mixer_bad_exec
	@mkdir -p $(dir $@)
	cp $< $@
	chmod 755 $@

check-mixer-module: $(BUILD)/tests/mixer_module $(BUILD)/tests/mixer_peer $(BUILD)/tests/mixer_bad_exec $(BUILD)/modules/mod_audio_mixer.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/mixer_module $(BUILD)/modules $(abspath $(BUILD)/tests/mixer_peer) $(abspath $(BUILD)/tests/mixer_bad_exec)

$(BUILD)/tests/app_launch: tests/app_launch.c core/host.h include/openaurora/app_launch.h include/openaurora/events.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-app-launch
check-app-launch: $(BUILD)/tests/app_launch $(BUILD)/modules/mod_app_launch.so
	@$(BUILD)/tests/app_launch $(BUILD)/modules
check: check-app-launch

.PHONY: check-audio-mixer-quiesce
check-audio-mixer-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_audio_mixer.so $(BUILD)/tests/mixer_peer
	node tests/audio_mixer_quiesce.cjs $(abspath $(BUILD))
check: check-audio-mixer-quiesce

$(BUILD)/tests/owner_races: tests/owner_races.c common/audio_owner.c common/audio_client.c common/audio_netlink.c common/cec_owner.c common/cec_authority.c common/cec_device.c common/cec_packet.c common/cec.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: check-owner-races check-ui-anim-review
check-owner-races: $(BUILD)/tests/owner_races
	@$(BUILD)/tests/owner_races

$(BUILD)/tests/ui_anim_review: tests/ui_anim_review.c common/ui_anim.c include/openaurora/ui_anim.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-ui-anim-review: $(BUILD)/tests/ui_anim_review $(BUILD)/modules/mod_ui_anim.so
	@$(BUILD)/tests/ui_anim_review $(BUILD)/modules

.PHONY: check-ui-anim-quiesce
check-ui-anim-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so
	node tests/ui_anim_quiesce.cjs $(abspath $(BUILD))
check: check-ui-anim-quiesce

$(BUILD)/tests/audio_speaker: tests/audio_speaker.c common/audio_speaker.c include/openaurora/audio_speaker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)
.PHONY: check-audio-speaker
check-audio-speaker: $(BUILD)/tests/audio_speaker
	@$(BUILD)/tests/audio_speaker
check: check-audio-speaker

$(BUILD)/tests/audio_curve: tests/audio_curve.c common/audio_curve.c include/openaurora/audio_curve.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-audio-curve: $(BUILD)/tests/audio_curve
	@$(BUILD)/tests/audio_curve

$(BUILD)/tests/audio_netlink: tests/audio_netlink.c common/audio_netlink.c include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-audio-netlink: $(BUILD)/tests/audio_netlink
	@$(BUILD)/tests/audio_netlink

$(BUILD)/tests/audio_client: tests/audio_client.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-audio-client: $(BUILD)/tests/audio_client
	@$(BUILD)/tests/audio_client

$(BUILD)/tests/audio_curve_module: tests/audio_curve_module.c core/host.h include/openaurora/audio_curve.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-audio-curve-quiesce
check-audio-curve-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_audio_curve.so
	node tests/audio_curve_quiesce.cjs $(abspath $(BUILD))
check: check-audio-curve-quiesce

check-audio-curve-module: $(BUILD)/tests/audio_curve_module $(BUILD)/modules/mod_audio_curve.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/audio_curve_module $(BUILD)/modules

$(BUILD)/tests/audio: tests/audio.c modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c include/openaurora/vendor_audio.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

$(BUILD)/tests/plat_audio: tests/plat_audio.c modules/plat/plat.c include/openaurora/vendor_audio.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR -DOA_PLAT_TEST $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-cec-audio: $(BUILD)/tests/cec $(BUILD)/tests/audio $(BUILD)/tests/plat_audio
	@$(BUILD)/tests/cec
	@$(BUILD)/tests/audio
	@$(BUILD)/tests/plat_audio

$(BUILD)/tests/audio-mods/mod_audio.so: $(call pic_objs,tests__audio-mods__mod_audio,modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c tests/audio_hal_stub.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,tests__audio-mods__mod_audio,modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c tests/audio_hal_stub.c,-UOA_NO_VENDOR -pthread,))

$(BUILD)/tests/audio_session: tests/audio_session.c tests/lifecycle_retry.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-audio-session: $(BUILD)/tests/audio_session $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/modules/mod_plat.so
	@mkdir -p $(BUILD)/tests/audio-mods/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/tests/audio-mods/modules/mod_plat.so
	@cp $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/tests/audio-mods/modules/mod_audio.so
	@printf 'audio.enable = 1\n' > $(BUILD)/tests/audio-mods/audio.cfg
	@$(BUILD)/tests/audio_session $(BUILD)/tests/audio-mods/modules $(BUILD)/tests/audio-mods/audio.cfg

.PHONY: check-audio-quiesce
check-audio-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/modules/mod_plat.so
	node tests/audio_quiesce.cjs $(abspath $(BUILD))
check: check-audio-quiesce

$(BUILD)/tests/cec_audio_policy: tests/cec_audio_policy.c tests/lifecycle_retry.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-cec-audio-policy: $(BUILD)/tests/cec_audio_policy $(BUILD)/modules/mod_cec.so $(BUILD)/modules/mod_plat.so $(BUILD)/tests/audio-mods/mod_audio.so
	@mkdir -p $(BUILD)/tests/cec-route/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_cec.so $(BUILD)/tests/cec-route/modules/
	@cp $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/tests/cec-route/modules/mod_audio.so
	@printf 'audio.enable = 1\n' > $(BUILD)/tests/cec-route/audio.cfg
	@$(BUILD)/tests/cec_audio_policy $(BUILD)/tests/cec-route/modules $(BUILD)/tests/cec-route/audio.cfg

$(BUILD)/tests/image_decode: tests/image_decode.c core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-image-decode
check-image-decode: $(BUILD)/tests/image_decode $(BUILD)/modules/mod_image_decode.so
	@$(BUILD)/tests/image_decode $(BUILD)/modules $(CURDIR)/../artifacts/workbench/thumbs \
	  $(wildcard $(CURDIR)/../artifacts/dumps/apdat_files/dlna-dmr/xml/icon) || test $$? -eq 77
check: check-image-decode

$(BUILD)/tests/ui_live_cards: tests/ui_live_cards.c tests/ui_nav_trace.h core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests -Ithird_party $(LDFLAGS) -o $@ tests/ui_live_cards.c $(filter %.o,$^) $(LDLIBS) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)

$(BUILD)/tests/ui_channels: tests/ui_channels.c tests/ui_nav_trace.h core/host.h include/openaurora/services.h include/openaurora/ui_engine.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests -Ithird_party $(LDFLAGS) -o $@ tests/ui_channels.c $(filter %.o,$^) $(LDLIBS) \
	  $(if $(filter tv,$(TARGET)),-L$(VENDOR_LIB)) $(SQLITE_LIBS)

ui-live-modules = $(BUILD)/tests/ui-live-cards/modules
ui-live-stage: $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_settings.so $(BUILD)/modules/mod_image_decode.so
	@mkdir -p $(ui-live-modules)
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_settings.so $(BUILD)/modules/mod_image_decode.so $(ui-live-modules)/
	@cp $(BUILD)/tests/ui_nav_display.so $(ui-live-modules)/mod_nav_display.so
.PHONY: check-ui-live-cards check-ui-live-dump
check-ui-live-cards: $(BUILD)/tests/ui_live_cards ui-live-stage
	@$(BUILD)/tests/ui_live_cards $(ui-live-modules) synthetic
check: check-ui-live-cards
check-ui-live-dump: $(BUILD)/tests/ui_live_cards ui-live-stage
	@$(BUILD)/tests/ui_live_cards $(ui-live-modules) $(CURDIR)/../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db || test $$? -eq 77
.PHONY: check-ui-apps-grid
check-ui-apps-grid: $(BUILD)/tests/ui_live_cards ui-live-stage
	@$(BUILD)/tests/ui_live_cards $(ui-live-modules) synthetic "" apps
	@OA_TEST_MANY=1 $(BUILD)/tests/ui_live_cards $(ui-live-modules) synthetic "" apps
check: check-ui-apps-grid

ui-channels-modules = $(BUILD)/tests/ui-channels/modules
.PHONY: check-ui-channels
check-ui-channels: $(BUILD)/tests/ui_channels $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_service_db.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_channels.so
	@mkdir -p $(ui-channels-modules)
	@cp $(BUILD)/modules/mod_service_db.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_channels.so $(ui-channels-modules)/
	@cp $(BUILD)/tests/ui_nav_display.so $(ui-channels-modules)/mod_nav_display.so
	@$(BUILD)/tests/ui_channels $(ui-channels-modules)
check: check-ui-channels
.PHONY: check-ui-png-cards
check-ui-png-cards: $(BUILD)/tests/ui_live_cards ui-live-stage
	@$(BUILD)/tests/ui_live_cards $(ui-live-modules) synthetic "$(CURDIR)/../artifacts/workbench/thumbs" png || test $$? -eq 77
check: check-ui-png-cards
.PHONY: check-ui-jpeg-cards
check-ui-jpeg-cards: $(BUILD)/tests/ui_live_cards ui-live-stage
	@$(BUILD)/tests/ui_live_cards $(ui-live-modules) synthetic "$(CURDIR)/../artifacts/dumps/apdat_files/dlna-dmr/xml/icon" jpeg || test $$? -eq 77
check: check-ui-jpeg-cards

$(BUILD)/tests/ui_nav_display.so: $(call pic_objs,tests__ui_nav_display,tests/ui_nav_display.c common/canvas.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LIBM)
$(eval $(call emit_pics,tests__ui_nav_display,tests/ui_nav_display.c common/canvas.c,-Itests,tests/ui_nav_trace.h include/openaurora/canvas.h include/openaurora/services.h))

$(BUILD)/tests/ui_nav: tests/ui_nav.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_nav.c $(filter %.o,$^) $(LDLIBS)

$(BUILD)/tests/ui_sources: tests/ui_sources.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_sources.c $(filter %.o,$^) $(LDLIBS)

check-ui-sources: $(BUILD)/tests/ui_sources $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so
	@mkdir -p $(BUILD)/tests/ui-sources/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so $(BUILD)/tests/ui-sources/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-sources/modules/mod_nav_display.so
	@sqlite3 $(CURDIR)/$(BUILD)/tests/ui-sources/settings.db 'DROP TABLE IF EXISTS Settings; CREATE TABLE Settings (m_current_source INTEGER); INSERT INTO Settings (m_current_source) VALUES (4);'
	@printf 'ir.device =\nui.ok_file =\nui.paint = 1\nui.focus = 2\nui.fps = 0\nui.launch_anim = 1\nsettings.db = $(CURDIR)/$(BUILD)/tests/ui-sources/settings.db\nsettings.readonly = 1\n' > $(BUILD)/tests/ui-sources/ui.cfg
	@$(BUILD)/tests/ui_sources $(BUILD)/tests/ui-sources/modules $(BUILD)/tests/ui-sources/ui.cfg

$(BUILD)/tests/ui_skin: tests/ui_skin.c tests/ui_nav_trace.h include/openaurora/ui_engine.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_skin.c $(filter %.o,$^) $(LDLIBS)

check-ui-skin: $(BUILD)/tests/ui_skin $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so
	@mkdir -p $(BUILD)/tests/ui-skin/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/tests/ui-skin/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-skin/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_skin $(BUILD)/tests/ui-skin/modules

$(BUILD)/tests/ui_diag: tests/ui_diag.c tests/ui_nav_trace.h include/openaurora/ui_engine.h include/openaurora/services.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-ui-diag
check-ui-diag: $(BUILD)/tests/ui_diag $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_diag.so
	@mkdir -p $(BUILD)/tests/ui-diag/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_diag.so $(BUILD)/tests/ui-diag/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-diag/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_diag $(BUILD)/tests/ui-diag/modules
check: check-ui-diag

.PHONY: check-ui-diag-quiesce
check-ui-diag-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_diag.so
	node tests/ui_diag_quiesce.cjs $(abspath $(BUILD))
check: check-ui-diag-quiesce

.PHONY: check-skin-switch
check-skin-switch: $(BUILD)/$(PROJECT) $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_ui_diag.so
	node tests/skin_switch_ipc.cjs $(abspath $(BUILD))
check: check-skin-switch

$(BUILD)/tests/ui_lua: tests/ui_lua.c tests/ui_nav_trace.h include/openaurora/lua_skin.h include/openaurora/ui_engine.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_lua.c $(filter %.o,$^) $(LDLIBS)

check-ui-lua: $(BUILD)/tests/ui_lua $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_lua.so
	@mkdir -p $(BUILD)/tests/ui-lua/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_lua.so $(BUILD)/tests/ui-lua/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-lua/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_lua $(BUILD)/tests/ui-lua/modules

check: check-ui-lua

.PHONY: check-lua-ipc
check-lua-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_lua.so
	node tests/lua_ipc.cjs $(abspath $(BUILD))
check: check-lua-ipc

$(BUILD)/tests/skin_selection: tests/skin_selection.c core/host.h include/openaurora/ui_engine.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

$(BUILD)/pic/tests__mod_ui_%/tests/skin_candidate.o: tests/skin_candidate.c include/openaurora/ui_engine.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c -MMD -MP -DSKIN_NAME='"$(*)"' \
	  $(if $(filter startfail busyfail,$*),-DFAIL_START=1) \
	  $(if $(filter busyfail,$*),-DBUSY=1) \
	  $(if $(filter badenter,$*),-DFAIL_ENTER=1) \
	  $(if $(filter silent,$*),-DSILENT=1) \
	  $(if $(filter initbusy,$*),-DFAIL_INIT=1) \
	  -o $@ $<

$(BUILD)/tests/mod_ui_custom.so $(BUILD)/tests/mod_ui_startfail.so $(BUILD)/tests/mod_ui_badenter.so $(BUILD)/tests/mod_ui_silent.so $(BUILD)/tests/mod_ui_busyfail.so $(BUILD)/tests/mod_ui_initbusy.so: $(BUILD)/tests/mod_ui_%.so: $(BUILD)/pic/tests__mod_ui_%/tests/skin_candidate.o
	@mkdir -p $(dir $@)
	$(CC) -shared $(LDFLAGS) -o $@ $<

.PHONY: check-skin-selection
check-skin-selection: check-ui-skin $(BUILD)/tests/skin_selection $(BUILD)/tests/mod_ui_custom.so $(BUILD)/tests/mod_ui_startfail.so $(BUILD)/tests/mod_ui_badenter.so $(BUILD)/tests/mod_ui_silent.so $(BUILD)/tests/mod_ui_busyfail.so $(BUILD)/tests/mod_ui_initbusy.so
	@cp $(BUILD)/tests/mod_ui_custom.so $(BUILD)/tests/mod_ui_startfail.so $(BUILD)/tests/mod_ui_badenter.so $(BUILD)/tests/mod_ui_silent.so $(BUILD)/tests/mod_ui_busyfail.so $(BUILD)/tests/mod_ui_initbusy.so $(BUILD)/tests/ui-skin/modules/
	@$(BUILD)/tests/skin_selection $(BUILD)/tests/ui-skin/modules

check: check-skin-selection

$(BUILD)/tests/ui_settings: tests/ui_settings.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_settings.c $(filter %.o,$^) $(LDLIBS)

check-ui-settings: $(BUILD)/tests/ui_settings $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@mkdir -p $(BUILD)/tests/ui-settings/modules $(BUILD)/tests/ui-settings/ui
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_settings $(BUILD)/tests/ui-settings/modules $(BUILD)/tests/ui-settings/ui

$(BUILD)/tests/ui_settings_panel: tests/ui_settings_panel.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_settings_panel.c $(filter %.o,$^) $(LDLIBS)

check-ui-settings-panel: $(BUILD)/tests/ui_settings_panel $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@mkdir -p $(BUILD)/tests/ui-settings-panel/modules $(BUILD)/tests/ui-settings-panel/ui
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-panel/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-panel/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_settings_panel $(BUILD)/tests/ui-settings-panel/modules $(BUILD)/tests/ui-settings-panel/ui

$(BUILD)/tests/ui_settings_quick: tests/ui_settings_quick.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

$(BUILD)/tests/ui_portal_quick: tests/ui_portal_quick.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

PORTAL_REAL_UI ?= ../artifacts/workbench/source-stage-20261101/ui
.PHONY: capture-portal-real
capture-portal-real: $(BUILD)/tests/ui_portal_quick $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(PORTAL_REAL_UI)/title.rgba || { echo "missing $(PORTAL_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-portal-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-portal-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-portal-real/modules/mod_nav_display.so
	@OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-portal-real/portal) \
		$(BUILD)/tests/ui_portal_quick $(abspath $(BUILD)/tests/ui-portal-real/modules) \
		$(abspath $(PORTAL_REAL_UI))
	python3 tools/rawrgba-to-png.py --over-black \
		$(BUILD)/tests/ui-portal-real/portal.portal \
		1920 1080 \
		$(BUILD)/tests/ui-portal-real/portal.png

.PHONY: capture-portal-card-real
capture-portal-card-real: $(BUILD)/tests/ui_portal_quick $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(PORTAL_REAL_UI)/title.rgba || { echo "missing $(PORTAL_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-portal-card-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-portal-card-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-portal-card-real/modules/mod_nav_display.so
	@OA_PORTAL_CARD=1 OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-portal-card-real/portal-card) \
		$(BUILD)/tests/ui_portal_quick $(abspath $(BUILD)/tests/ui-portal-card-real/modules) \
		$(abspath $(PORTAL_REAL_UI))
	python3 tools/rawrgba-to-png.py --over-black \
		$(BUILD)/tests/ui-portal-card-real/portal-card.portal \
		1920 1080 \
		$(BUILD)/tests/ui-portal-card-real/portal-card.png

SETTINGS_REAL_UI ?= ../artifacts/workbench/source-stage-20261101/ui
.PHONY: capture-settings-quick-real
capture-settings-quick-real: $(BUILD)/tests/ui_settings_quick $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(SETTINGS_REAL_UI)/settings_panel.rgba || { echo "missing $(SETTINGS_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-settings-quick/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-quick/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-quick/modules/mod_nav_display.so
	@OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-settings-quick/settings-quick) \
		$(BUILD)/tests/ui_settings_quick $(abspath $(BUILD)/tests/ui-settings-quick/modules) \
		$(abspath $(SETTINGS_REAL_UI))
	python3 tools/rawrgba-to-png.py \
		$(BUILD)/tests/ui-settings-quick/settings-quick.quick \
		1920 1080 \
		$(BUILD)/tests/ui-settings-quick/settings-quick.png

.PHONY: capture-settings-panel-real
capture-settings-panel-real: $(BUILD)/tests/ui_settings_panel $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(SETTINGS_REAL_UI)/settings_panel.rgba || { echo "missing $(SETTINGS_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-settings-panel-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-panel-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-panel-real/modules/mod_nav_display.so
	@OA_SETTINGS_REAL=1 OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-settings-panel-real/settings-panel) \
		$(BUILD)/tests/ui_settings_panel $(abspath $(BUILD)/tests/ui-settings-panel-real/modules) \
		$(abspath $(SETTINGS_REAL_UI))
	python3 tools/panel-source-compare.py \
		$(BUILD)/tests/ui-settings-panel-real/settings-panel.panel 1920 1080 \
		$(SETTINGS_REAL_UI)/settings_panel.rgba \
		$(BUILD)/tests/ui-settings-panel-real/settings-panel.png

.PHONY: capture-settings-text-real
capture-settings-text-real: $(BUILD)/tests/ui_settings_panel $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(SETTINGS_REAL_UI)/settings_panel.rgba || { echo "missing $(SETTINGS_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-settings-text-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-text-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-text-real/modules/mod_nav_display.so
	@OA_SETTINGS_REAL=1 OA_SETTINGS_TEXT=1 OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-settings-text-real/settings-text) \
		$(BUILD)/tests/ui_settings_panel $(abspath $(BUILD)/tests/ui-settings-text-real/modules) \
		$(abspath $(SETTINGS_REAL_UI))
	python3 tools/panel-source-compare.py \
		$(BUILD)/tests/ui-settings-text-real/settings-text.panel 1920 1080 \
		$(SETTINGS_REAL_UI)/settings_panel.rgba \
		$(BUILD)/tests/ui-settings-text-real/settings-text.png

$(BUILD)/tests/ui_sources_quick: tests/ui_sources_quick.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: capture-sources-real
capture-sources-real: $(BUILD)/tests/ui_sources_quick $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(SETTINGS_REAL_UI)/src_title.rgba || { echo "missing $(SETTINGS_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-sources-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-sources-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-sources-real/modules/mod_nav_display.so
	@OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-sources-real/sources) \
		$(BUILD)/tests/ui_sources_quick $(abspath $(BUILD)/tests/ui-sources-real/modules) \
		$(abspath $(SETTINGS_REAL_UI))
	python3 tools/rawrgba-to-png.py --over-black \
		$(BUILD)/tests/ui-sources-real/sources.sources 1920 1080 \
		$(BUILD)/tests/ui-sources-real/sources.png

.PHONY: capture-settings-focus-real
capture-settings-focus-real: $(BUILD)/tests/ui_settings_panel $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@test -f $(SETTINGS_REAL_UI)/settings_focus_0.rgba || { echo "missing $(SETTINGS_REAL_UI); stage the workbench UI assets first"; exit 2; }
	@mkdir -p $(BUILD)/tests/ui-settings-focus-real/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-focus-real/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-focus-real/modules/mod_nav_display.so
	@OA_SETTINGS_REAL=1 OA_SETTINGS_SEL=0 OA_NAV_CAPTURE=$(abspath $(BUILD)/tests/ui-settings-focus-real/settings-focus) \
		$(BUILD)/tests/ui_settings_panel $(abspath $(BUILD)/tests/ui-settings-focus-real/modules) \
		$(abspath $(SETTINGS_REAL_UI))
	python3 tools/panel-source-compare.py \
		$(BUILD)/tests/ui-settings-focus-real/settings-focus.panel 1920 1080 \
		$(SETTINGS_REAL_UI)/settings_focus_0.rgba \
		$(BUILD)/tests/ui-settings-focus-real/settings-focus.png 20 186

$(BUILD)/tests/ui_settings_rows: tests/ui_settings_rows.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_settings_rows.c $(filter %.o,$^) $(LDLIBS)

check-ui-settings-rows: $(BUILD)/tests/ui_settings_rows $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@mkdir -p $(BUILD)/tests/ui-settings-rows/modules $(BUILD)/tests/ui-settings-rows/ui
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-rows/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-rows/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_settings_rows $(BUILD)/tests/ui-settings-rows/modules $(BUILD)/tests/ui-settings-rows/ui

$(BUILD)/tests/ui_settings_text: tests/ui_settings_text.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_settings_text.c $(filter %.o,$^) $(LDLIBS)

check-ui-settings-text: $(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so
	@mkdir -p $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui-settings-text/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-settings-text/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui on
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui missing
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui notitle
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui badtitle
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui preset
	@$(BUILD)/tests/ui_settings_text $(BUILD)/tests/ui-settings-text/modules $(BUILD)/tests/ui-settings-text/ui editoff

check-ui-nav: check-ui-anim-review $(BUILD)/tests/ui_nav $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so
	@mkdir -p $(BUILD)/tests/ui-nav/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so $(BUILD)/tests/ui-nav/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-nav/modules/mod_nav_display.so
	@sqlite3 $(CURDIR)/$(BUILD)/tests/ui-nav/settings.db 'DROP TABLE IF EXISTS Settings; CREATE TABLE Settings (m_current_source INTEGER); INSERT INTO Settings (m_current_source) VALUES (4);'
	@printf 'ir.device =\nui.ok_file =\nui.paint = 1\nui.focus = 2\nui.fps = 0\nui.launch_anim = 1\nsettings.db = $(CURDIR)/$(BUILD)/tests/ui-nav/settings.db\nsettings.readonly = 1\n' > $(BUILD)/tests/ui-nav/ui.cfg
	@$(BUILD)/tests/ui_nav $(BUILD)/tests/ui-nav/modules $(BUILD)/tests/ui-nav/ui.cfg

$(BUILD)/tests/control_reply: tests/control_reply.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

$(BUILD)/tests/control_transport: tests/control_transport.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS) -Wl,--wrap=send -Wl,--wrap=accept4

check-control: $(BUILD)/tests/control_reply $(BUILD)/tests/control_transport
	@$(BUILD)/tests/control_reply
	@$(BUILD)/tests/control_transport

.PHONY: check-oa-client
check-oa-client: $(BUILD)/oa-client
	node tests/oa_client.cjs $(abspath $(BUILD)/oa-client)

.PHONY: check-reload-suite
check-reload-suite: $(BUILD)/$(PROJECT)
	node tests/reload_suite.cjs $(abspath $(BUILD))
check: check-reload-suite

.PHONY: check-autoload
check-autoload: $(BUILD)/$(PROJECT)
	node tests/autoload.cjs $(abspath $(BUILD))
check: check-autoload

$(BUILD)/tests/display_present: tests/display_present.c modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c include/openaurora/canvas.h include/openaurora/fb_snapshot.h include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm -pthread \
	  $(foreach call,open close system mmap munmap msync ioctl pthread_create pthread_tryjoin_np,-Wl,--wrap=$(call))

$(BUILD)/tests/display_binding: tests/display_present.c modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c include/openaurora/canvas.h include/openaurora/fb_snapshot.h include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_DISPLAY_NO_PLAT $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm -pthread \
	  $(foreach call,open close system mmap munmap msync ioctl pthread_create pthread_tryjoin_np,-Wl,--wrap=$(call))

$(BUILD)/tests/display_resume_frame: tests/display_resume_frame.c modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c include/openaurora/canvas.h include/openaurora/fb_snapshot.h include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm -pthread \
	  $(foreach call,open close system mmap munmap msync ioctl,-Wl,--wrap=$(call))

$(BUILD)/fbregion: tools/mcu/fbregion.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/fb-restore: tools/display/fb-restore.c common/fb_snapshot.c include/openaurora/fb_snapshot.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/fb-region-sampler: tools/display/fb-region-sampler.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

.PHONY: cold-bundle
cold-bundle: $(BUILD)/$(PROJECT) $(MOD_SO) $(BUILD)/oa-supervise $(BUILD)/fb-restore
	@rm -rf $(BUILD)/cold-bundle
	@mkdir -p $(BUILD)/cold-bundle/modules
	@cp $(BUILD)/$(PROJECT) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/cold-bundle/
	@cp $(MOD_SO) $(BUILD)/cold-bundle/modules/
	@cp tools/boot/openaurora-cold.conf $(BUILD)/cold-bundle/openaurora.conf
	@cd $(BUILD)/cold-bundle && sha256sum openaurora oa-supervise fb-restore openaurora.conf modules/*.so > manifest.sha256
	@echo "cold bundle: $(BUILD)/cold-bundle ($(words $(MOD_SO)) modules)"

# Consolidated deployment: only the modules the cold config autoloads. The
# other $(words $(EXTRA_MODULES)) stay in the build and full bundle for
# feature/takeover use; this does not change reload granularity.
.PHONY: all-core cold-bundle-core module-categories
all-core: $(BUILD)/$(PROJECT) $(CORE_SO) $(BUILD)/oa-ir-blast

cold-bundle-core: all-core $(BUILD)/oa-supervise $(BUILD)/fb-restore
	@rm -rf $(BUILD)/cold-bundle-core
	@mkdir -p $(BUILD)/cold-bundle-core/modules
	@cp $(BUILD)/$(PROJECT) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-ir-blast $(BUILD)/cold-bundle-core/
	@cp $(CORE_SO) $(BUILD)/cold-bundle-core/modules/
	@sed 's/^autoload =.*/autoload = $(call stage_autoload,$(CORE_MODULES))/' tools/boot/openaurora-cold.conf > $(BUILD)/cold-bundle-core/openaurora.conf
	@cd $(BUILD)/cold-bundle-core && sha256sum openaurora oa-supervise fb-restore oa-ir-blast openaurora.conf modules/*.so > manifest.sha256
	@echo "core cold bundle: $(BUILD)/cold-bundle-core ($(words $(CORE_SO)) modules)"

# Additive layers on the core: STAGE=core|tv|av|ui|script|diag|full.
.PHONY: all-stage stage-bundle install-stage
all-stage: $(BUILD)/$(PROJECT) $(STAGE_SO) $(BUILD)/oa-ir-blast

stage-bundle: all-stage $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client
	@rm -rf $(BUILD)/stage-$(STAGE)
	@mkdir -p $(BUILD)/stage-$(STAGE)/modules $(BUILD)/stage-$(STAGE)/menus
	@cp $(BUILD)/$(PROJECT) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client $(BUILD)/oa-ir-blast $(BUILD)/stage-$(STAGE)/
	@cp $(STAGE_SO) $(BUILD)/stage-$(STAGE)/modules/
	@cp menus/sub_launcher.json $(BUILD)/stage-$(STAGE)/menus/
	@sed 's/^autoload =.*/autoload = $(call stage_autoload,$(STAGE_MODULES))/' tools/boot/openaurora-cold.conf > $(BUILD)/stage-$(STAGE)/openaurora.conf
	@cd $(BUILD)/stage-$(STAGE) && sha256sum openaurora oa-supervise fb-restore oa-client oa-ir-blast openaurora.conf menus/*.json modules/*.so > manifest.sha256
	@echo "stage $(STAGE) bundle: $(BUILD)/stage-$(STAGE) ($(words $(STAGE_SO)) modules)"

# Takeover-mode replacement UI: plat_stub preserves the vendor-enabled OSD;
# the real plat DRM/KMS black-screen issue is documented in
# artifacts/reports/HW-ACCEPT-2-2026-11-07.md. UI assets are local/private and
# expected under /buffer/openaurora/ui on the TV.
REPLACEMENT_MODULES := plat display panel ir settings service_db image_decode ui_engine ui_vendor ticker ui_clock ui_anim
REPLACEMENT_SO := $(foreach m,$(REPLACEMENT_MODULES),$(BUILD)/modules/mod_$(m).so)
.PHONY: replacement-bundle
replacement-bundle: $(BUILD)/$(PROJECT) $(REPLACEMENT_SO) $(BUILD)/tests/mod_plat_stub.so $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client $(BUILD)/oa-ir-blast
	@rm -rf $(BUILD)/replacement
	@mkdir -p $(BUILD)/replacement/modules $(BUILD)/replacement/menus
	@cp $(BUILD)/$(PROJECT) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client $(BUILD)/oa-ir-blast $(BUILD)/replacement/
	@cp $(REPLACEMENT_SO) $(BUILD)/replacement/modules/
	@cp $(BUILD)/tests/mod_plat_stub.so $(BUILD)/replacement/modules/mod_plat.so
	@cp tools/boot/openaurora-replacement.conf $(BUILD)/replacement/openaurora.conf
	@cp menus/sub_launcher.json $(BUILD)/replacement/menus/
	@cd $(BUILD)/replacement && sha256sum openaurora oa-supervise fb-restore oa-client oa-ir-blast openaurora.conf menus/*.json modules/*.so > manifest.sha256
	@echo "replacement bundle: $(BUILD)/replacement ($(words $(REPLACEMENT_SO)) modules + plat_stub)"

# Complete real-platform composition: all feature/UI modules, fixed asset
# configs, real plat with plat.drm_init=0 (preserve the vendor OSD). Takeover
# candidate; cold OSD enable remains open.
FULL_REPLACEMENT_MODULES := plat display panel ir audio source audio_source power tuner settings service_db image_decode epg pvr channels i18n audio_curve audio_mixer app_launch audio_amp_profile audio_amp cec ticker ui_clock ui_anim ui_engine ui_vendor ui_omarchy ui_theme ui_profile ui_menu ui_list ui_navigation lua spi
FULL_REPLACEMENT_SO := $(foreach m,$(FULL_REPLACEMENT_MODULES),$(BUILD)/modules/mod_$(m).so)
$(BUILD)/oa-alsactl: tools/audio/oa-alsactl.c
	@mkdir -p $(dir $@)
	# The vendor ALSA 1.0.29 headers are not -pedantic clean (pcm.h zero-length
	# array); keep the project's default flags except that one diagnostic for
	# this TV-only helper, and link the TV's own libasound.
	$(CC) $(CPPFLAGS) $(if $(filter tv arm,$(TARGET)),-idirafter /usr/include,) $(filter-out -pedantic,$(CFLAGS)) $(LDFLAGS) -o $@ $< -L$(VENDOR_LIB) -Wl,-rpath,/vendor/lib/libs -l:libasound.so.2 -lpthread -ldl

# TV-only one-shot NAM launcher. The vendor libnam is glibc ARM; the helper is
# spawned by mod_app_launch with an explicit display-name->app map.
$(BUILD)/oa-app-launch: tools/app/oa-app-launch.c
	@mkdir -p $(dir $@)
	@test -f $(VENDOR_LIB)/nlib/libnam.so || { echo "missing $(VENDOR_LIB)/nlib/libnam.so (run make vendor)"; exit 1; }
	$(CC) $(CPPFLAGS) $(if $(filter tv arm,$(TARGET)),-idirafter /usr/include,) $(filter-out -pedantic,$(CFLAGS)) $(LDFLAGS) -o $@ $< -L$(VENDOR_LIB)/nlib -Wl,-rpath,/vendor/lib/nlib -Wl,--allow-shlib-undefined -Wl,--no-as-needed -lpthread -Wl,--as-needed -l:libnam.so

BUNDLE_DIR ?= $(BUILD)/full-replacement
BUNDLE_CONF ?= tools/boot/openaurora-full-replacement.conf
BUNDLE_LABEL ?= full replacement
.PHONY: full-replacement-bundle
full-replacement-bundle: $(BUILD)/$(PROJECT) $(FULL_REPLACEMENT_SO) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client $(BUILD)/oa-ir-blast $(BUILD)/oa-audio-broker $(BUILD)/oa-alsactl $(BUILD)/oa-app-launch $(BUILD)/oa-wdtfeed
	@rm -rf $(BUNDLE_DIR)
	@mkdir -p $(BUNDLE_DIR)/modules $(BUNDLE_DIR)/menus $(BUNDLE_DIR)/ui $(BUNDLE_DIR)/lua
	@cp $(BUILD)/$(PROJECT) $(BUILD)/oa-supervise $(BUILD)/fb-restore $(BUILD)/oa-client $(BUILD)/oa-ir-blast $(BUILD)/oa-audio-broker $(BUILD)/oa-alsactl $(BUILD)/oa-app-launch $(BUILD)/oa-wdtfeed $(BUNDLE_DIR)/
	@cp tools/boot/takeover-full.sh tools/boot/run-aurora.sh tools/boot/oa-boot-gate.sh tools/boot/aurora-boot-hook.sh tools/boot/deploy-staged-cold.sh tools/boot/oa_apps.map $(BUNDLE_DIR)/
	@cp $(FULL_REPLACEMENT_SO) $(BUNDLE_DIR)/modules/
	@cp $(BUNDLE_CONF) $(BUNDLE_DIR)/openaurora.conf
	@for s in 0 1 2 3 4 5; do sh tools/boot/stage-profile.sh $$s $(BUNDLE_CONF) > $(BUNDLE_DIR)/openaurora-stage$$s.conf; done
	@cp menus/sub_launcher.json $(BUNDLE_DIR)/menus/
	@cp ui/README.md $(BUNDLE_DIR)/ui/
	@cp lua/omarchy.lua $(BUNDLE_DIR)/lua/
	@if ls ui/*.rgba >/dev/null 2>&1; then cp ui/*.rgba $(BUNDLE_DIR)/ui/; fi
	@cd $(BUNDLE_DIR) && sha256sum openaurora oa-supervise fb-restore oa-client oa-ir-blast oa-audio-broker oa-alsactl oa-app-launch oa-wdtfeed takeover-full.sh run-aurora.sh oa-boot-gate.sh aurora-boot-hook.sh deploy-staged-cold.sh oa_apps.map openaurora.conf openaurora-stage*.conf menus/*.json modules/*.so ui/* lua/* > manifest.sha256
	@echo "$(BUNDLE_LABEL) bundle: $(BUNDLE_DIR) ($(words $(FULL_REPLACEMENT_SO)) modules)"

# OmarchyTV-native UI composition: same modules and hardware stack, but the
# autoloaded skin is ui_omarchy over ui.engine instead of ui_vendor.
.PHONY: native-replacement-bundle
native-replacement-bundle:
	@$(MAKE) --no-print-directory full-replacement-bundle \
	    BUNDLE_DIR=$(BUILD)/native-replacement \
	    BUNDLE_CONF=tools/boot/openaurora-native.conf \
	    BUNDLE_LABEL='native replacement'

# Cold-start candidate: stock vendor UI, real platform, no takeover fallback.
# NOT TV-accepted; build only until a cold-boot/recovery gate is authorized.
.PHONY: cold-replacement-bundle
cold-replacement-bundle:
	@$(MAKE) --no-print-directory full-replacement-bundle \
	    BUNDLE_DIR=$(BUILD)/cold-replacement \
	    BUNDLE_CONF=tools/boot/openaurora-cold.conf \
	    BUNDLE_LABEL='cold replacement candidate'

install-stage: all-stage
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILD)/$(PROJECT) $(DESTDIR)$(BINDIR)/$(PROJECT)
	install -m 755 $(BUILD)/oa-supervise $(DESTDIR)$(BINDIR)/oa-supervise
	install -m 755 $(BUILD)/oa-ir-blast $(DESTDIR)$(BINDIR)/oa-ir-blast
	install -m 755 $(BUILD)/oa-audio-broker $(DESTDIR)$(BINDIR)/oa-audio-broker
	install -m 755 $(STAGE_SO) $(DESTDIR)$(LIBDIR)/

module-categories:
	@echo "hardware ($(words $(HW_MODULES))): $(HW_MODULES)"
	@echo "middleware ($(words $(MW_MODULES))): $(MW_MODULES)"
	@echo "utilities ($(words $(UTIL_MODULES))): $(UTIL_MODULES)"
	@echo "ui/ux ($(words $(UI_MODULES))): $(UI_MODULES)"
	@echo "core ($(words $(CORE_MODULES))): $(CORE_MODULES)"
	@echo "stage tv ($(words $(STAGE_tv))): $(STAGE_tv)"
	@echo "stage av ($(words $(STAGE_av))): $(STAGE_av)"
	@echo "stage ui ($(words $(STAGE_ui))): $(STAGE_ui)"
	@echo "stage dev ($(words $(STAGE_dev))): $(STAGE_dev)"
	@echo "stage full ($(words $(STAGE_full))): $(STAGE_full)"

$(BUILD)/tests/vendor_decoder_probe.so: $(call pic_objs,tests__vendor_decoder_probe,tests/vendor_decoder_probe.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -ldl
$(eval $(call emit_pics,tests__vendor_decoder_probe,tests/vendor_decoder_probe.c,,))
.PHONY: check-image-decode-quiesce
check-image-decode-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_image_decode.so
	node tests/image_decode_quiesce.cjs $(abspath $(BUILD))
check: check-image-decode-quiesce

.PHONY: check-image-decode-ipc
check-image-decode-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_image_decode.so
	node tests/image_decode_ipc.cjs $(abspath $(BUILD))
check: check-image-decode-ipc

.PHONY: check-ir-ipc
check-ir-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui_nav_display.so
	node tests/ir_ipc.cjs $(abspath $(BUILD))
check: check-ir-ipc

.PHONY: check-ir-quiesce
check-ir-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so
	@command -v socat >/dev/null || { echo "  skip ir quiesce: socat unavailable"; exit 0; }
	node tests/ir_quiesce.cjs $(abspath $(BUILD))
check: check-ir-quiesce

$(BUILD)/oa-ir-blast: tools/ir/oa-ir-blast.c include/openaurora/ir_blast.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -ldl \
	  -Wl,-rpath,/vendor/lib:/vendor/lib/libs

$(BUILD)/tests/fake_blast_lib.so: tests/fake_blast_lib.c
	@mkdir -p $(dir $@)
	$(CC) -shared -fPIC -o $@ $< -pthread

.PHONY: check-ir-blast-worker
check-ir-blast-worker: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ir.so $(BUILD)/tests/mod_plat_stub.so $(BUILD)/tests/fake_blast_lib.so $(BUILD)/oa-ir-blast
	node tests/ir_blast_worker.cjs $(abspath $(BUILD))
check: check-ir-blast-worker

$(BUILD)/tests/ir_helper: tests/ir_helper.c modules/ir/ir.c include/openaurora/events.h include/openaurora/ir_blast.h include/openaurora/module.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/ir_helper.c -pthread
.PHONY: check-ir-helper
check-ir-helper: $(BUILD)/tests/ir_helper
	@$(BUILD)/tests/ir_helper
check: check-ir-helper

.PHONY: check-ui-vendor-ipc
check-ui-vendor-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui_nav_display.so
	node tests/ui_vendor_ipc.cjs $(abspath $(BUILD))
check: check-ui-vendor-ipc

.PHONY: check-ui-stack-ipc
check-ui-stack-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_settings.so $(BUILD)/modules/mod_image_decode.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui_nav_display.so
	node tests/ui_stack_ipc.cjs $(abspath $(BUILD))
check: check-ui-stack-ipc

.PHONY: check-full-stack-ipc
check-full-stack-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_settings.so $(BUILD)/modules/mod_service_db.so $(BUILD)/modules/mod_image_decode.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/tests/ui_nav_display.so
	node tests/full_stack_ipc.cjs $(abspath $(BUILD))
check: check-full-stack-ipc

.PHONY: check-module-matrix-ipc
check-module-matrix-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_navigation.so $(BUILD)/modules/mod_ui_list.so $(BUILD)/modules/mod_power.so $(BUILD)/modules/mod_cec.so
	node tests/module_matrix_ipc.cjs $(abspath $(BUILD))
check: check-module-matrix-ipc

# Additive batch queue: one module lifecycle operation per host-loop turn.
$(BUILD)/pic/tests__mod_batch_%/tests/batch_fixture.o: tests/batch_fixture.c include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DBATCH_MODULE_NAME='"batch_$(*)"' -fPIC -c -MMD -MP -o $@ $<

$(BUILD)/tests/mod_batch_%.so: $(BUILD)/pic/tests__mod_batch_%/tests/batch_fixture.o
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $< $(LDFLAGS)

BATCH_FIXTURES := a b c
$(BUILD)/tests/batch_load: tests/batch_load.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-batch-load
check-batch-load: $(BUILD)/tests/batch_load $(addprefix $(BUILD)/tests/mod_batch_,$(addsuffix .so,$(BATCH_FIXTURES)))
	@$(BUILD)/tests/batch_load $(BUILD)/tests
check: check-batch-load

.PHONY: check-batch-ipc
check-batch-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_power.so
	node tests/batch_ipc.cjs $(abspath $(BUILD))
check: check-batch-ipc

.PHONY: check-fixture-modules-ipc
check-fixture-modules-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_theme.so $(BUILD)/modules/mod_ui_profile.so $(BUILD)/modules/mod_audio_amp_profile.so $(BUILD)/modules/mod_audio_curve.so $(BUILD)/modules/mod_audio_mixer.so $(BUILD)/oa-audio-broker
	node tests/fixture_modules_ipc.cjs $(abspath $(BUILD))
check: check-fixture-modules-ipc

$(BUILD)/tests/fake_fb_preload.so: $(call pic_objs,tests__fake_fb_preload,tests/fake_fb_preload.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -ldl
$(eval $(call emit_pics,tests__fake_fb_preload,tests/fake_fb_preload.c,,include/openaurora/fb_snapshot.h))
.PHONY: check-fake-fb-socket
check-fake-fb-socket: $(BUILD)/$(PROJECT) $(BUILD)/tests/fake_fb_preload.so $(BUILD)/modules/mod_display_fb.so
	node tests/fake_fb_socket.cjs $(abspath $(BUILD))
check: check-fake-fb-socket

.PHONY: check-graphics-socket
check-graphics-socket: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_graphics.so $(BUILD)/modules/mod_scene_display.so $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so
	node tests/graphics_socket.cjs $(abspath $(BUILD))
check: check-graphics-socket

$(BUILD)/tests/mod_plat_fixture.so: $(call pic_objs,tests__mod_plat_fixture,tests/plat_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_plat_fixture,tests/plat_fixture.c,,include/openaurora/module.h include/openaurora/services.h))

# qemu-vin-real-check.sh loads this as modules/mod_plat.so and calls
# `load plat`; keep the descriptor name "plat" even though the source is a
# test stub. It publishes only the no-HAL plat service.
$(BUILD)/tests/mod_plat_stub.so: $(call pic_objs,tests__mod_plat_stub,tests/mod_plat_stub.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_plat_stub,tests/mod_plat_stub.c,,include/openaurora/module.h include/openaurora/services.h))

.PHONY: check-plat-consumers-socket
check-plat-consumers-socket: $(BUILD)/$(PROJECT) $(BUILD)/tests/mod_plat_fixture.so $(BUILD)/modules/mod_audio.so $(BUILD)/modules/mod_source.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_panel.so
	node tests/plat_consumers_socket.cjs $(abspath $(BUILD))
check: check-plat-consumers-socket

.PHONY: check-capture-compare
check-capture-compare:
	python3 tests/capture_compare_test.py
check: check-capture-compare

STOCK_APPS ?= ../artifacts/protocol/stock-apps-grid.png
$(BUILD)/tests/apps_grid_stock: tests/apps_grid_stock.c tests/ui_nav_trace.h core/host.h include/openaurora/services.h include/openaurora/ui_engine.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS) $(SQLITE_LIBS)

apps-grid-stock-ui = $(BUILD)/tests/apps-grid-stock/ui
.PHONY: capture-stock-apps-grid
capture-stock-apps-grid: $(BUILD)/tests/apps_grid_stock ui-live-stage
	@test -f $(STOCK_APPS) || { echo "missing $(STOCK_APPS); capture the stock apps grid first"; exit 2; }
	python3 tools/stage-apps-grid.py --out $(abspath $(apps-grid-stock-ui)) \
		--thumbs $(abspath ../artifacts/workbench/thumbs)
	$(BUILD)/tests/apps_grid_stock $(abspath $(ui-live-modules)) \
		$(abspath $(apps-grid-stock-ui)) \
		$(abspath ../artifacts/analysis/stock-apps-grid-oracle-2026-11-05/offline-apps-grid-stock.rgba)
	python3 tools/rawrgba-to-png.py \
		../artifacts/analysis/stock-apps-grid-oracle-2026-11-05/offline-apps-grid-stock.rgba \
		1920 1080 \
		../artifacts/analysis/stock-apps-grid-oracle-2026-11-05/offline-apps-grid-stock.png

LIVE_DB ?= ../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
LIVE_THUMBS ?= ../artifacts/workbench/thumbs
apps-grid-live-ui = $(BUILD)/tests/apps-grid-live/ui
.PHONY: capture-firmware-apps-grid
capture-firmware-apps-grid: $(BUILD)/tests/apps_grid_stock ui-live-stage
	@test -f $(LIVE_DB) || { echo "missing $(LIVE_DB); copy the live settingsDB first"; exit 2; }
	@test -d $(LIVE_THUMBS) || { echo "missing $(LIVE_THUMBS); copy the downloaded images first"; exit 2; }
	python3 tools/stage-apps-grid.py --out $(abspath $(apps-grid-live-ui)) \
		--db $(abspath $(LIVE_DB)) --thumbs $(abspath $(LIVE_THUMBS))
	$(BUILD)/tests/apps_grid_stock $(abspath $(ui-live-modules)) \
		$(abspath $(apps-grid-live-ui)) \
		$(abspath ../artifacts/analysis/stock-apps-grid-live-2026-11-05/offline-apps-grid-live.rgba) \
		$(abspath $(LIVE_DB))
	python3 tools/rawrgba-to-png.py \
		../artifacts/analysis/stock-apps-grid-live-2026-11-05/offline-apps-grid-live.rgba \
		1920 1080 \
		../artifacts/analysis/stock-apps-grid-live-2026-11-05/offline-apps-grid-live.png

.PHONY: compare-stock-apps-grid
compare-stock-apps-grid:
	@test -f $(STOCK_APPS) || { echo "missing $(STOCK_APPS); capture the stock apps grid first"; exit 2; }
	python3 tools/capture_compare.py --reference $(STOCK_APPS) \
		--candidate ../artifacts/analysis/stock-apps-grid-oracle-2026-11-05/offline-apps-grid-stock.png \
		--manifest ../artifacts/analysis/stock-parity-2026-11-05/apps-grid-regions.json

ui-skin-socket-modules = $(BUILD)/tests/ui-skin-socket/modules
.PHONY: check-ui-skin-socket
check-ui-skin-socket: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_ui_diag.so $(BUILD)/tests/ui_nav_display.so
	@mkdir -p $(ui-skin-socket-modules)
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_omarchy.so $(BUILD)/modules/mod_ui_diag.so $(ui-skin-socket-modules)/
	@cp $(BUILD)/tests/ui_nav_display.so $(ui-skin-socket-modules)/mod_nav_display.so
	@command -v socat >/dev/null || { echo "  skip ui skin socket: socat unavailable"; exit 0; }
	@sh tests/skin_socket.sh $(abspath $(BUILD)/$(PROJECT)) $(abspath $(ui-skin-socket-modules))
check: check-ui-skin-socket

.PHONY: check-vendor-decoder
check-vendor-decoder: $(BUILD)/tests/vendor_decoder_probe.so $(BUILD)/tests/image_decode $(BUILD)/modules/mod_image_decode.so
	@test "$(TARGET)" = tv || { echo "check-vendor-decoder requires TARGET=tv"; exit 2; }
	@BUILD=$(BUILD) python3 tests/vendor_decoder.py --build $(BUILD)

$(BUILD)/tests/fb_snapshot: tests/fb_snapshot.c common/fb_snapshot.c include/openaurora/fb_snapshot.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) \
	  $(foreach call,mmap munmap msync ioctl,-Wl,--wrap=$(call))

$(BUILD)/tests/fb_restore_tool: tests/fb_restore_tool.c tools/display/fb-restore.c common/fb_snapshot.c include/openaurora/fb_snapshot.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/fb_restore_tool.c common/fb_snapshot.c \
	  $(foreach call,open close mmap munmap msync ioctl,-Wl,--wrap=$(call))
.PHONY: check-fb-snapshot
check-fb-snapshot: $(BUILD)/tests/fb_snapshot $(BUILD)/tests/fb_restore_tool
	@$(BUILD)/tests/fb_snapshot
	@$(BUILD)/tests/fb_restore_tool
check: check-fb-snapshot

# Explicit opt-in live fbdev diagnostic; never run by make check. No HAL setup.
$(BUILD)/scene-probe: tools/display/scene-probe.c modules/display/display.c common/canvas.c common/ui_anim.c common/fb_snapshot.c include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h include/openaurora/canvas.h include/openaurora/fb_snapshot.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm -pthread

# Optional GPU diagnostic. Headers are installed Khronos EGL/GLES2 declarations;
# use them after the cross toolchain's own standard headers, never its host libc.
ifeq ($(TARGET),tv)
EGL_PROBE_LIBS = -L../artifacts/workbench/rootfs/vendor/lib -l:libmali.so \
  -Wl,--allow-shlib-undefined -Wl,-rpath,/vendor/lib:/vendor/lib/libs
else
EGL_PROBE_LIBS = -lEGL -lGLESv2
endif
$(BUILD)/egl-probe: tools/display/egl-probe.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $< $(EGL_PROBE_LIBS)

$(BUILD)/tests/scene_broker: tests/scene_broker.c common/scene_broker.c common/scene_broker_wire.c common/scene_broker_transport.c include/openaurora/scene_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread -lm

.PHONY: check-scene-broker
check: check-scene-broker check-scene-spawn check-scene-client check-graphics-module check-scene-display check-ui-async
check-scene-broker: $(BUILD)/tests/scene_broker
	@$(BUILD)/tests/scene_broker

$(BUILD)/tests/scene-renderer-fixture.so: $(call pic_objs,tests__scene-renderer-fixture,tests/scene_renderer_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) -Wl,-init,scene_fixture_init -Wl,-fini,scene_fixture_fini
$(eval $(call emit_pics,tests__scene-renderer-fixture,tests/scene_renderer_fixture.c,,include/openaurora/scene_renderer.h include/openaurora/scene_broker.h))

$(BUILD)/tests/scene-renderer-bad.so: $(call pic_objs,tests__scene-renderer-bad,tests/scene_renderer_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) -Wl,-init,scene_fixture_init -Wl,-fini,scene_fixture_fini
$(eval $(call emit_pics,tests__scene-renderer-bad,tests/scene_renderer_fixture.c,-DBAD_RENDERER_ABI,include/openaurora/scene_renderer.h include/openaurora/scene_broker.h))

$(BUILD)/tests/scene_spawn: tests/scene_spawn.c common/scene_broker_wire.c common/scene_broker_transport.c include/openaurora/scene_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

.PHONY: check-scene-spawn
check-scene-spawn: $(BUILD)/tests/scene_spawn $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so $(BUILD)/tests/scene-renderer-bad.so
	@$(BUILD)/tests/scene_spawn $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so $(BUILD)/tests/scene-renderer-bad.so

$(BUILD)/tests/scene_client: tests/scene_client.c common/scene_client.c common/scene_broker_wire.c common/scene_broker_transport.c include/openaurora/scene_client.h include/openaurora/scene_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--wrap=sendmsg -o $@ $(filter %.c,$^)

.PHONY: check-scene-client
check-scene-client: $(BUILD)/tests/scene_client $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so
	@$(BUILD)/tests/scene_client $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so

$(BUILD)/tests/graphics_module: tests/graphics_module.c common/scene_broker_transport.c include/openaurora/scene_broker.h core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-graphics-module
check-graphics-module: $(BUILD)/tests/graphics_module $(BUILD)/modules/mod_graphics.so $(BUILD)/modules/mod_ticker.so $(BUILD)/oa-scene-broker $(BUILD)/tests/scene-renderer-fixture.so
	@$(BUILD)/tests/graphics_module $(BUILD)/modules $(abspath $(BUILD)/oa-scene-broker) $(abspath $(BUILD)/tests/scene-renderer-fixture.so)

$(BUILD)/modules/mod_scene_gpu_fixture.so: $(call pic_objs,modules__mod_scene_gpu_fixture,tests/scene_gpu_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_scene_gpu_fixture,tests/scene_gpu_fixture.c,-Itests,tests/scene_gpu_trace.h include/openaurora/services.h include/openaurora/module.h))

$(BUILD)/tests/scene_display: tests/scene_display.c tests/scene_gpu_trace.h common/canvas.c include/openaurora/canvas.h core/host.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-scene-display
check-scene-display: $(BUILD)/tests/scene_display $(BUILD)/modules/mod_scene_display.so $(BUILD)/modules/mod_scene_gpu_fixture.so
	@$(BUILD)/tests/scene_display $(BUILD)/modules

$(BUILD)/tests/ui_async: tests/ui_async.c tests/scene_gpu_trace.h core/host.h include/openaurora/services.h include/openaurora/ui_engine.h include/openaurora/events.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-ui-async
check-ui-async: $(BUILD)/tests/ui_async $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_scene_display.so $(BUILD)/modules/mod_scene_gpu_fixture.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/modules/mod_ui_anim.so
	@$(BUILD)/tests/ui_async $(BUILD)/modules $(abspath $(BUILD)/tests/mod_fake_clock.so)

# Offline GLES regression on host Mesa. The production scene code requests
# vendor display 1; tests/egl_default_display.c redirects that contract to
# EGL_DEFAULT_DISPLAY so the same GLES2 path runs on llvmpipe without a TV or
# a window. Opt-in: requires host Mesa, so it is not part of make check.
$(BUILD)/egl-scene-probe-mesa: tools/display/egl-scene-probe.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c tests/egl_default_display.c include/openaurora/scene_gles.h include/openaurora/glyph_quad.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $(filter %.c,$^) -Wl,--wrap=eglGetDisplay -Wl,--wrap=glCreateProgram -Wl,--wrap=glGenTextures $(EGL_PROBE_LIBS) -pthread -lm
.PHONY: scene-gles-mesa
scene-gles-mesa: $(BUILD)/egl-scene-probe-mesa
	@EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 $(BUILD)/egl-scene-probe-mesa

$(BUILD)/egl-scene-probe: tools/display/egl-scene-probe.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c include/openaurora/scene_gles.h include/openaurora/glyph_quad.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $(filter %.c,$^) $(EGL_PROBE_LIBS) -pthread -lm

GLYPH_PROBE_SRC := tools/display/egl-glyph-probe.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c common/font_box.c common/font_text.c common/font_draw.c common/font_bitmap.c common/font_pack.c common/text_origin.c
GLYPH_PROBE_HDR := include/openaurora/scene_gles.h include/openaurora/glyph_quad.h include/openaurora/ui_anim.h include/openaurora/font_box.h include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_bitmap.h include/openaurora/font_pack.h include/openaurora/text_origin.h include/openaurora/canvas.h
$(BUILD)/egl-glyph-probe: $(GLYPH_PROBE_SRC) $(GLYPH_PROBE_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $(GLYPH_PROBE_SRC) $(EGL_PROBE_LIBS) -pthread -lm

$(BUILD)/tests/glyph_gles: $(GLYPH_PROBE_SRC) $(GLYPH_PROBE_HDR) tests/egl_default_display.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DOA_GLYPH_SOFTWARE_TEST $(LDFLAGS) -Wl,--wrap=eglGetDisplay -Wl,--wrap=glCreateProgram -Wl,--wrap=glGenTextures -o $@ $(GLYPH_PROBE_SRC) tests/egl_default_display.c -lEGL -lGLESv2 -pthread -lm

.PHONY: check-glyph-gles
# Explicit host-only Mesa software check; no display or TV hardware window.
check-glyph-gles: $(BUILD)/tests/glyph_gles
	@EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 $(BUILD)/tests/glyph_gles

$(BUILD)/tests/glyph_quad: tests/glyph_quad.c common/glyph_quad.c include/openaurora/glyph_quad.h include/openaurora/font_draw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-glyph-quad
check-glyph-quad: $(BUILD)/tests/glyph_quad
	@$(BUILD)/tests/glyph_quad
check: check-glyph-quad

# Explicit targets pending production module integration. Bootstrap is EGL-free;
# the separately loaded renderer owns Mali for the child's entire lifetime.
$(BUILD)/oa-scene-broker: tools/display/oa-scene-broker.c common/scene_broker.c common/scene_broker_wire.c common/scene_broker_transport.c include/openaurora/scene_renderer.h include/openaurora/scene_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -ldl

$(BUILD)/scene-renderer.so: $(call pic_objs,scene-renderer,tools/display/scene-renderer.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(EGL_PROBE_LIBS) -pthread -lm
$(eval $(call emit_pics,scene-renderer,tools/display/scene-renderer.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c,-idirafter /usr/include -pthread,include/openaurora/scene_renderer.h include/openaurora/scene_broker.h include/openaurora/scene_gles.h include/openaurora/glyph_quad.h include/openaurora/ui_anim.h))

$(BUILD)/egl-window-probe: tools/display/egl-window-probe.c common/scene_gles.c common/glyph_quad.c common/ui_anim.c include/openaurora/scene_gles.h include/openaurora/glyph_quad.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $(filter %.c,$^) $(EGL_PROBE_LIBS) -pthread -lm

$(BUILD)/scene-window-probe: tools/display/egl-window-probe.c tools/display/scene-probe-client.c tools/display/scene-probe-client.h common/scene_client.c common/scene_broker_wire.c common/scene_broker_transport.c include/openaurora/scene_client.h include/openaurora/scene_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DOA_WINDOW_BROKER $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/scene-module-probe: tools/display/egl-window-probe.c tools/display/scene-module-client.c tools/display/scene-probe-client.h common/scene_broker_transport.c core/host.h include/openaurora/services.h include/openaurora/scene_broker.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -DOA_WINDOW_BROKER $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

$(BUILD)/cec-probe: tools/cec/cec-probe.c common/cec_packet.c common/cec.c include/openaurora/vendor_cec.h include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/audio-state: tools/audio/audio-state.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/amp-probe: tools/audio/amp-probe.c common/amp_i2c.c include/openaurora/amp_i2c.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/audio-netlink-probe: tools/audio/audio-netlink-probe.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_netlink.h include/openaurora/audio_client.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

# Optional diagnostic: ALSA development headers are required on the build host.
# -idirafter keeps the target compiler's libc headers ahead of host headers.
$(BUILD)/mixer-probe: tools/audio/mixer-probe.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $< \
	  $(ALSA_DIAGNOSTIC_LIBS)

$(BUILD)/svg-animations: tools/re/svg-animations.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/svg-names: tools/re/svg-names.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/svg-rects: tools/re/svg-rects.c common/svg_pack.c common/binsvg.c include/openaurora/svg_pack.h include/openaurora/binsvg.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

$(BUILD)/tests/svg_animations: tests/svg_animations.c tools/re/svg-animations.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/tests/svg_names: tests/svg_names.c tools/re/svg-names.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

check-svg: $(BUILD)/svg-animations $(BUILD)/svg-names $(BUILD)/tests/svg_animations $(BUILD)/tests/svg_names
	@$(BUILD)/tests/svg_animations
	@$(BUILD)/tests/svg_names

$(BUILD)/tests/vendor_grid: tests/vendor_grid.c common/vendor_grid.c include/openaurora/vendor_grid.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-vgrid: $(BUILD)/tests/vendor_grid
	@$(BUILD)/tests/vendor_grid

$(BUILD)/tests/fbregion_read: tests/fbregion_read.c tools/mcu/fbregion.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl

$(BUILD)/tests/plat_drm_gate: tests/plat_drm_gate.c modules/plat/plat.c include/openaurora/module.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_PLAT_TEST $(CFLAGS) $(LDFLAGS) -o $@ tests/plat_drm_gate.c
.PHONY: check-plat-drm-gate
check-plat-drm-gate: $(BUILD)/tests/plat_drm_gate
	@$(BUILD)/tests/plat_drm_gate
check: check-plat-drm-gate

check-display: $(BUILD)/tests/display_present $(BUILD)/tests/display_binding $(BUILD)/tests/display_resume_frame $(BUILD)/tests/fbregion_read
	@$(BUILD)/tests/display_present
	@$(BUILD)/tests/display_binding
	@$(BUILD)/tests/display_resume_frame
	@$(BUILD)/tests/fbregion_read

# Production display code through the real loader; only fixture DSOs wrap I/O.
$(BUILD)/display-fixture/mod_display_fb.so: $(call pic_objs,display-fixture__mod_display_fb,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c tests/display_dso_observer.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) -lm -pthread $(foreach call,open close system mmap munmap msync ioctl,-Wl,--wrap=$(call))
$(eval $(call emit_pics,display-fixture__mod_display_fb,modules/display/display.c common/ui_anim.c common/canvas.c common/fb_snapshot.c tests/display_dso_observer.c,-DOA_DISPLAY_NO_PLAT -pthread,include/openaurora/module.h include/openaurora/services.h include/openaurora/fb_snapshot.h include/openaurora/ui_anim.h include/openaurora/canvas.h))

$(BUILD)/tests/display_lifecycle: tests/display_lifecycle.c core/host.h include/openaurora/fb_snapshot.h include/openaurora/services.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -Wl,--export-dynamic -o $@ $(filter %.c %.o,$^) $(LDLIBS)

.PHONY: check-display-lifecycle
check-display-lifecycle: $(BUILD)/tests/display_lifecycle $(BUILD)/display-fixture/mod_display_fb.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/display_lifecycle $(abspath $(BUILD)/display-fixture) $(abspath $(BUILD)/modules/mod_ticker.so)
check: check-display-lifecycle

check-runtime: all
	@BUILD=$(BUILD) CC='$(CC)' CFLAGS='$(CFLAGS)' sh tests/runtime_check.sh

check-ui-slide: $(BUILD)/tests/ui_slide $(BUILD)/tests/mod_fake_clock.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/ui_nav_display.so
	@mkdir -p $(BUILD)/tests/ui-slide/modules $(BUILD)/tests/ui-slide/ui
	@cp $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-slide/modules/
	@mv -f $(BUILD)/tests/ui-slide/modules/ui_nav_display.so $(BUILD)/tests/ui-slide/modules/mod_nav_display.so
	@printf 'ui.slide_box = 100\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-slide/ui\n' > $(BUILD)/tests/ui-slide/ui.cfg
	@$(BUILD)/tests/ui_slide $(BUILD)/tests/ui-slide/modules $(BUILD)/tests/ui-slide/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-slide/ui

$(BUILD)/tests/ui_slide: tests/ui_slide.c tests/ui_nav_trace.h include/openaurora/ui_anim.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_slide.c $(filter %.o,$^) $(LDLIBS)

check-ui-rebind: $(BUILD)/tests/ui_rebind $(BUILD)/tests/mod_fake_clock.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ticker.so
	@mkdir -p $(BUILD)/tests/ui-rebind/modules $(BUILD)/tests/ui-rebind/ui
	@cp $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ticker.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-rebind/modules/
	@mv -f $(BUILD)/tests/ui-rebind/modules/ui_nav_display.so $(BUILD)/tests/ui-rebind/modules/mod_nav_display.so
	@printf 'ui.slide_box = 100\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-rebind/ui\n' > $(BUILD)/tests/ui-rebind/ui.cfg
	@$(BUILD)/tests/ui_rebind $(BUILD)/tests/ui-rebind/modules $(BUILD)/tests/ui-rebind/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-rebind/ui

$(BUILD)/tests/ui_rebind: tests/ui_rebind.c tests/ui_nav_trace.h include/openaurora/ui_anim.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_rebind.c $(filter %.o,$^) $(LDLIBS)

check-ui-rail-hl: $(BUILD)/tests/ui_rail_hl $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/mod_patch_fixture.so
	@mkdir -p $(BUILD)/tests/ui-rail-hl/modules $(BUILD)/tests/ui-rail-hl/ui
	@cp $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/mod_patch_fixture.so $(BUILD)/tests/ui-rail-hl/modules/
	@mv -f $(BUILD)/tests/ui-rail-hl/modules/ui_nav_display.so $(BUILD)/tests/ui-rail-hl/modules/mod_nav_display.so
	@printf 'ui.rail_hl = 1\nui.slide_box = 0\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-rail-hl/ui\n' > $(BUILD)/tests/ui-rail-hl/ui.cfg
	@$(BUILD)/tests/ui_rail_hl $(BUILD)/tests/ui-rail-hl/modules $(BUILD)/tests/ui-rail-hl/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-rail-hl/ui

$(BUILD)/tests/ui_rail_hl: tests/ui_rail_hl.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_rail_hl.c $(filter %.o,$^) $(LDLIBS)

$(BUILD)/tests/ui_anim_bootstrap: tests/ui_anim_bootstrap.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_anim_bootstrap.c $(filter %.o,$^) $(LDLIBS)

.PHONY: check-ui-anim-bootstrap
check-ui-anim-bootstrap: $(BUILD)/tests/ui_anim_bootstrap $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_theme.so $(BUILD)/tests/mod_fake_clock.so
	@mkdir -p $(BUILD)/tests/ui-anim-bootstrap/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_theme.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui-anim-bootstrap/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-anim-bootstrap/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_anim_bootstrap $(BUILD)/tests/ui-anim-bootstrap/modules
check: check-ui-anim-bootstrap

$(BUILD)/tests/launcher_anim: tests/launcher_anim.c common/launcher_anim.c include/openaurora/launcher_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/launcher_anim.c common/launcher_anim.c

.PHONY: check-launcher-anim
check-launcher-anim: $(BUILD)/tests/launcher_anim
	@$(BUILD)/tests/launcher_anim
check: check-launcher-anim

$(BUILD)/tests/menu_model: tests/menu_model.c common/menu_model.c common/launcher_anim.c common/json.c menus/sub_launcher.json include/openaurora/menu_model.h include/openaurora/launcher_anim.h include/openaurora/json.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/menu_model.c common/menu_model.c common/launcher_anim.c common/json.c

.PHONY: check-menu-model
check-menu-model: $(BUILD)/tests/menu_model
	@$(BUILD)/tests/menu_model $(CURDIR)/menus/sub_launcher.json
check: check-menu-model

LANGPROFILE ?= ../artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin

$(BUILD)/tests/langprofile: tests/langprofile.c common/langprofile.c include/openaurora/langprofile.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/langprofile.c common/langprofile.c

.PHONY: check-langprofile
check-langprofile: $(BUILD)/tests/langprofile
	@test -f $(LANGPROFILE) || { echo "missing $(LANGPROFILE); the real langprofile is required" >&2; exit 2; }
	@$(BUILD)/tests/langprofile $(LANGPROFILE)
check: check-langprofile

SUBFONT ?= ../artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data

$(BUILD)/tests/sub_text_locales: tests/sub_text_locales.c common/langprofile.c common/font_pack.c common/font_bitmap.c common/font_text.c common/font_draw.c common/font_box.c common/canvas.c common/text_origin.c include/openaurora/langprofile.h include/openaurora/font_pack.h include/openaurora/font_bitmap.h include/openaurora/font_text.h include/openaurora/font_draw.h include/openaurora/font_box.h include/openaurora/canvas.h include/openaurora/text_origin.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/sub_text_locales.c common/langprofile.c common/font_pack.c common/font_bitmap.c common/font_text.c common/font_draw.c common/font_box.c common/canvas.c common/text_origin.c -lm

.PHONY: check-sub-text-locales
check-sub-text-locales: $(BUILD)/tests/sub_text_locales
	@test -f $(LANGPROFILE) && test -f $(SUBFONT).txt && test -f $(SUBFONT).bin || { echo "missing langprofile/font pack inputs" >&2; exit 2; }
	@$(BUILD)/tests/sub_text_locales $(LANGPROFILE) $(SUBFONT).txt $(SUBFONT).bin
check: check-sub-text-locales

$(BUILD)/tests/rtl_order: tests/rtl_order.c common/langprofile.c common/rtl_order.c include/openaurora/langprofile.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/rtl_order.c common/langprofile.c common/rtl_order.c

.PHONY: check-rtl-order
check-rtl-order: $(BUILD)/tests/rtl_order
	@test -f $(LANGPROFILE) || { echo "missing $(LANGPROFILE); the real langprofile is required" >&2; exit 2; }
	@$(BUILD)/tests/rtl_order $(LANGPROFILE)
check: check-rtl-order

$(BUILD)/tests/i18n_service: tests/i18n_service.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/i18n_service.c $(filter %.o,$^) $(LDLIBS)

SETTINGSDB ?= ../artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
AURORA_ELF ?= ../artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf

$(BUILD)/tests/picture_settings: tests/picture_settings.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/picture_settings.c $(filter %.o,$^) $(LDLIBS) $(SQLITE_LIBS)

.PHONY: check-picture-object
check-picture-object:
	@test -f $(AURORA_ELF) || { echo "missing $(AURORA_ELF); the shipped Aurora ELF is required" >&2; exit 2; }
	@test -f $(SETTINGSDB) || { echo "missing $(SETTINGSDB); the real settingsDB is required" >&2; exit 2; }
	OA_AURORA_ELF=$(abspath $(AURORA_ELF)) OA_SETTINGSDB=$(abspath $(SETTINGSDB)) \
	    $(CURDIR)/tools/qemu-picture-object-check.sh
check: check-picture-object

.PHONY: check-picture-settings
check-picture-settings: $(BUILD)/tests/picture_settings $(BUILD)/modules/mod_settings.so
	@test -f $(SETTINGSDB) || { echo "missing $(SETTINGSDB); the real settingsDB is required" >&2; exit 2; }
	@$(BUILD)/tests/picture_settings $(SETTINGSDB) $(abspath $(BUILD)/modules)
check: check-picture-settings

.PHONY: check-settings-quiesce
check-settings-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_settings.so
	@test -f $(SETTINGSDB) || { echo "missing $(SETTINGSDB); the real settingsDB is required" >&2; exit 2; }
	node tests/settings_quiesce.cjs $(abspath $(BUILD)) $(abspath $(SETTINGSDB))
check: check-settings-quiesce

.PHONY: check-i18n-service
check-i18n-service: $(BUILD)/tests/i18n_service $(BUILD)/modules/mod_i18n.so
	@test -f $(LANGPROFILE) || { echo "missing $(LANGPROFILE); the real langprofile is required" >&2; exit 2; }
	@mkdir -p $(BUILD)/tests/i18n-service/modules
	@cp $(BUILD)/modules/mod_i18n.so $(BUILD)/tests/i18n-service/modules/
	@$(BUILD)/tests/i18n_service $(BUILD)/tests/i18n-service/modules $(LANGPROFILE)
check: check-i18n-service

.PHONY: check-i18n-quiesce
check-i18n-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_i18n.so
	@test -f $(LANGPROFILE) || { echo "missing $(LANGPROFILE); the real langprofile is required" >&2; exit 2; }
	node tests/i18n_quiesce.cjs $(abspath $(BUILD)) $(abspath $(LANGPROFILE))
check: check-i18n-quiesce

$(BUILD)/tests/ui_menu_service: tests/ui_menu_service.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_menu_service.c $(filter %.o,$^) $(LDLIBS)

.PHONY: check-ui-menu-quiesce
check-ui-menu-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_menu.so
	node tests/ui_menu_quiesce.cjs $(abspath $(BUILD)) $(abspath $(CURDIR)/menus/sub_launcher.json)
check: check-ui-menu-quiesce

.PHONY: check-ui-menu-service
check-ui-menu-service: $(BUILD)/tests/ui_menu_service $(BUILD)/modules/mod_ui_menu.so
	@mkdir -p $(BUILD)/tests/ui-menu/modules
	@cp $(BUILD)/modules/mod_ui_menu.so $(BUILD)/tests/ui-menu/modules/
	@$(BUILD)/tests/ui_menu_service $(BUILD)/tests/ui-menu/modules $(CURDIR)/menus/sub_launcher.json
check: check-ui-menu-service

$(BUILD)/tests/ui_menu_action: tests/ui_menu_action.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_menu_action.c $(filter %.o,$^) $(LDLIBS)

$(BUILD)/tests/mod_source_fake.so: $(call pic_objs,tests__mod_source_fake,modules/source/source.c tests/source_fake.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -ldl -pthread
$(eval $(call emit_pics,tests__mod_source_fake,modules/source/source.c tests/source_fake.c,-pthread,include/openaurora/module.h include/openaurora/services.h include/openaurora/source_hal.h,PIC_CPPFLAGS_SOURCE_FAKE))

$(BUILD)/tests/source_path: tests/source_path.c modules/source/source.c tests/source_fake.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

.PHONY: check-source-path
check-source-path: $(BUILD)/tests/source_path
	@$(BUILD)/tests/source_path
check: check-source-path

$(BUILD)/tests/audio_source_path: tests/audio_source_path.c modules/audio_source/audio_source.c tests/audio_source_fake.c common/audio_curve.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

.PHONY: check-audio-source
check-audio-source: $(BUILD)/tests/audio_source_path
	@$(BUILD)/tests/audio_source_path
check: check-audio-source

.PHONY: check-vin-real
check-vin-real:
	tools/qemu-vin-real-check.sh
check: check-vin-real

.PHONY: check-source-fake-ipc
check-source-fake-ipc: $(BUILD)/$(PROJECT) $(BUILD)/tests/mod_source_fake.so $(BUILD)/modules/mod_plat.so
	node tests/source_fake_ipc.cjs $(abspath $(BUILD))
check: check-source-fake-ipc

.PHONY: check-ui-engine-quiesce
check-ui-engine-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_engine.so $(BUILD)/tests/ui_nav_display.so
	node tests/ui_engine_quiesce.cjs $(abspath $(BUILD)) $(abspath $(CURDIR)/..)
check: check-ui-engine-quiesce

.PHONY: check-ui-vendor-quiesce
check-ui-vendor-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_menu.so $(BUILD)/modules/mod_i18n.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui_nav_display.so
	node tests/ui_vendor_quiesce.cjs $(abspath $(BUILD)) $(abspath $(CURDIR)/..)
check: check-ui-vendor-quiesce

.PHONY: check-ui-menu-action
check-ui-menu-action: $(BUILD)/tests/ui_menu_action $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_menu.so $(BUILD)/tests/mod_fake_clock.so
	@mkdir -p $(BUILD)/tests/ui-menu-action/modules
	@cp $(BUILD)/modules/mod_ui_engine.so $(BUILD)/modules/mod_ui_vendor.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ui_menu.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui-menu-action/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-menu-action/modules/mod_nav_display.so
	@$(BUILD)/tests/ui_menu_action $(BUILD)/tests/ui-menu-action/modules
check: check-ui-menu-action

$(BUILD)/tests/mod_fake_clock.so: $(call pic_objs,tests__mod_fake_clock,tests/mod_fake_clock.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)
$(eval $(call emit_pics,tests__mod_fake_clock,tests/mod_fake_clock.c,,include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/mod_sim_display.so: $(call pic_objs,tests__mod_sim_display,tools/display/sim-display.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
$(eval $(call emit_pics,tests__mod_sim_display,tools/display/sim-display.c,,include/openaurora/module.h include/openaurora/services.h))

$(BUILD)/tests/mod_patch_fixture.so: $(call pic_objs,tests__mod_patch_fixture,tests/mod_patch_fixture.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)
$(eval $(call emit_pics,tests__mod_patch_fixture,tests/mod_patch_fixture.c,,include/openaurora/module.h include/openaurora/services.h))

check: all check-safety check-reload check-phases check-power check-ui-clock check-ui-nav check-ui-sources check-ui-skin check-ui-settings check-ui-settings-panel check-ui-settings-rows check-ui-settings-text check-ui-slide check-ui-rebind check-ui-rail-hl check-cec-audio check-audio-session check-cec-audio-policy check-cec-packet check-cec-device check-cec-owner check-audio-curve check-audio-curve-module check-audio-netlink check-audio-client check-audio-owner check-control check-oa-client check-display check-svg check-vgrid check-runtime $(BUILD)/tests/mod_recovery_fixture.so $(BUILD)/tests/mod_failed_init.so $(BUILD)/tests/registry
	@BUILD=$(BUILD) sh tests/run.sh
	@$(BUILD)/tests/registry $(BUILD)/modules $(BUILD)/tests/mod_failed_init.so
	@BUILD=$(BUILD) python3 tests/recovery.py

clean:
	rm -rf $(BUILD) build-arm build-tv

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILD)/$(PROJECT) $(DESTDIR)$(BINDIR)/$(PROJECT)
	install -m 755 $(BUILD)/oa-supervise $(DESTDIR)$(BINDIR)/oa-supervise
	install -m 755 $(BUILD)/oa-ir-blast $(DESTDIR)$(BINDIR)/oa-ir-blast
	install -m 755 $(BUILD)/oa-audio-broker $(DESTDIR)$(BINDIR)/oa-audio-broker
	install -m 755 $(MOD_SO) $(DESTDIR)$(LIBDIR)/

.PHONY: install-core
install-core: all-core
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILD)/$(PROJECT) $(DESTDIR)$(BINDIR)/$(PROJECT)
	install -m 755 $(BUILD)/oa-supervise $(DESTDIR)$(BINDIR)/oa-supervise
	install -m 755 $(BUILD)/oa-ir-blast $(DESTDIR)$(BINDIR)/oa-ir-blast
	install -m 755 $(BUILD)/oa-audio-broker $(DESTDIR)$(BINDIR)/oa-audio-broker
	install -m 755 $(CORE_SO) $(DESTDIR)$(LIBDIR)/

-include $(CORE_DEP) $(wildcard $(BUILD)/pic/*.d) $(wildcard $(BUILD)/pic/*/*.d) $(wildcard $(BUILD)/pic/*/*/*.d) $(wildcard $(BUILD)/pic/*/*/*/*.d) $(wildcard $(BUILD)/pic/*/*/*/*/*.d)

$(BUILD)/tests/audio_amp: tests/audio_amp.c common/audio_amp.c include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-audio-amp
check-audio-amp: $(BUILD)/tests/audio_amp
	@$(BUILD)/tests/audio_amp
check: check-audio-amp

$(BUILD)/tests/audio_amp_profile: tests/audio_amp_profile.c common/audio_amp_profile.c common/audio_amp.c common/profile.c include/openaurora/audio_amp.h include/openaurora/profile.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-audio-amp-profile
check-audio-amp-profile: $(BUILD)/tests/audio_amp_profile
	@$(BUILD)/tests/audio_amp_profile
check: check-audio-amp-profile

$(BUILD)/modules/mod_audio_amp_profile.so: $(call pic_objs,modules__mod_audio_amp_profile,modules/audio_amp_profile/audio_amp_profile.c common/audio_amp_profile.c common/profile.c)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ -pthread $(LDFLAGS)
$(eval $(call emit_pics,modules__mod_audio_amp_profile,modules/audio_amp_profile/audio_amp_profile.c common/audio_amp_profile.c common/profile.c,-pthread,include/openaurora/audio_amp.h include/openaurora/profile.h include/openaurora/services.h include/openaurora/module.h))

$(BUILD)/tests/amp_profile_module: tests/amp_profile_module.c common/profile.c core/host.h include/openaurora/services.h include/openaurora/audio_amp.h include/openaurora/profile.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)
.PHONY: check-amp-profile-quiesce
check-amp-profile-quiesce: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_audio_amp_profile.so
	node tests/amp_profile_quiesce.cjs $(abspath $(BUILD))
check: check-amp-profile-quiesce

.PHONY: check-amp-profile-module
check-amp-profile-module: $(BUILD)/tests/amp_profile_module $(BUILD)/modules/mod_audio_amp_profile.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/amp_profile_module $(BUILD)/modules
check: check-amp-profile-module

.PHONY: check-amp-profile-ipc
check-amp-profile-ipc: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_audio_amp_profile.so
	node tests/amp_profile_ipc.cjs $(abspath $(BUILD))
check: check-amp-profile-ipc

$(BUILD)/tests/amp_init: tests/amp_init.c common/amp_init.c common/audio_amp.c include/openaurora/amp_init.h include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-amp-init
check-amp-init: $(BUILD)/tests/amp_init
	@$(BUILD)/tests/amp_init
check: check-amp-init

$(BUILD)/tests/amp_owner: tests/amp_owner.c common/amp_owner.c common/amp_init.c common/audio_amp.c include/openaurora/amp_owner.h include/openaurora/amp_init.h include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread -Wl,--wrap=pthread_cond_wait -Wl,--wrap=pthread_tryjoin_np
.PHONY: check-amp-owner
check-amp-owner: $(BUILD)/tests/amp_owner
	@$(BUILD)/tests/amp_owner
check: check-amp-owner

$(BUILD)/tests/amp_owner_jobs: tests/amp_owner_jobs.c common/amp_owner.c common/amp_init.c common/audio_amp.c include/openaurora/amp_owner.h include/openaurora/amp_init.h include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread
.PHONY: check-amp-owner-jobs
check-amp-owner-jobs: $(BUILD)/tests/amp_owner_jobs
	@$(BUILD)/tests/amp_owner_jobs
check: check-amp-owner-jobs

$(BUILD)/tests/amp_i2c: tests/amp_i2c.c common/amp_i2c.c include/openaurora/amp_i2c.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-amp-i2c
check-amp-i2c: $(BUILD)/tests/amp_i2c
	@$(BUILD)/tests/amp_i2c
check: check-amp-i2c

$(BUILD)/tests/amp_gpio_query: tests/amp_gpio_query.c common/amp_gpio_query.c include/openaurora/amp_gpio_query.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
$(BUILD)/amp-gpio-probe: tools/audio/amp-gpio-probe.c common/amp_gpio_query.c include/openaurora/amp_gpio_query.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-amp-gpio-query
check-amp-gpio-query: $(BUILD)/tests/amp_gpio_query
	@$(BUILD)/tests/amp_gpio_query
check: check-amp-gpio-query

$(BUILD)/tests/amp_stbc: tests/amp_stbc.c common/amp_stbc.c include/openaurora/amp_stbc.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-appmgr-entry
check-appmgr-entry:
	@sh tests/appmgr_entry.sh
check: check-appmgr-entry

.PHONY: check-amp-stbc
check-amp-stbc: $(BUILD)/tests/amp_stbc
	@$(BUILD)/tests/amp_stbc
check: check-amp-stbc

$(BUILD)/tests/amp_board: tests/amp_board.c common/amp_board.c common/amp_init.c common/audio_amp.c include/openaurora/amp_board.h include/openaurora/amp_init.h include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-amp-board
check-amp-board: $(BUILD)/tests/amp_board
	@$(BUILD)/tests/amp_board
check: check-amp-board

$(BUILD)/tests/amp_vendor: tests/amp_vendor.c common/amp_vendor.c common/amp_board.c common/amp_init.c common/audio_amp.c common/amp_i2c.c include/openaurora/amp_vendor.h include/openaurora/amp_board.h include/openaurora/amp_init.h include/openaurora/audio_amp.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)
.PHONY: check-amp-vendor
check-amp-vendor: $(BUILD)/tests/amp_vendor
	@$(BUILD)/tests/amp_vendor
check: check-amp-vendor

.PHONY: check-audio-amp-module
check-audio-amp-module: $(BUILD)/$(PROJECT) $(BUILD)/modules/mod_audio_amp.so $(BUILD)/tests/mod_plat_stub.so
	node tests/audio_amp_ipc.cjs $(abspath $(BUILD))
check: check-audio-amp-module

$(BUILD)/tests/audio_amp_jobs: tests/audio_amp_jobs.c modules/audio_amp/audio_amp.c common/amp_owner.c common/amp_init.c common/amp_vendor.c common/amp_board.c common/audio_amp.c common/amp_i2c.c common/audio_amp_profile.c common/profile.c include/openaurora/amp_owner.h include/openaurora/amp_init.h include/openaurora/audio_amp.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ \
	  tests/audio_amp_jobs.c common/amp_owner.c common/amp_init.c \
	  common/amp_vendor.c common/amp_board.c common/audio_amp.c \
	  common/amp_i2c.c common/audio_amp_profile.c common/profile.c -pthread
.PHONY: check-audio-amp-jobs
check-audio-amp-jobs: $(BUILD)/tests/audio_amp_jobs
	@$(BUILD)/tests/audio_amp_jobs
check: check-audio-amp-jobs

.PHONY: all check check-safety check-reload check-phases check-power check-ui-clock check-ui-nav check-cec-audio check-cec-packet check-cec-device check-audio-curve check-audio-curve-module check-audio-netlink check-audio-client check-control check-oa-client check-display check-svg check-runtime clean install arm tv vendor
