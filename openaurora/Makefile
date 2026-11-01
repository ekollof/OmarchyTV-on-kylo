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

CC       ?= cc
CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude -D_GNU_SOURCE
LDFLAGS  ?=
LDLIBS   += -ldl -pthread -lm

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
LIBDIR   ?= $(PREFIX)/lib/$(PROJECT)

BUILD    ?= build
MODULES  := plat ticker ir ui_console panel power display audio settings source ui_fb ui_clock ui_anim audio_curve cec audio_mixer
# only these modules call the vendor HAL, so only they get the dependency
VENDOR_MODULES := plat panel audio source display
# modules that need a third-party library (sqlite) rather than the vendor HAL
SETTINGS_MODULES := settings
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

CORE_SRC := $(wildcard core/*.c)
CORE_OBJ := $(CORE_SRC:core/%.c=$(BUILD)/core/%.o)
CORE_DEP := $(CORE_OBJ:.o=.d)

MOD_SO   := $(foreach m,$(MODULES),$(BUILD)/modules/mod_$(m).so)
MOD_DEP  := $(foreach m,$(MODULES),$(BUILD)/modules/mod_$(m).d)

all: $(BUILD)/$(PROJECT) $(MOD_SO) $(BUILD)/oa-supervise $(BUILD)/oa-audio-broker

$(BUILD)/oa-audio-broker: tools/audio/oa-audio-broker.c common/audio_broker.c common/audio_broker_wire.c common/audio_client.c common/audio_netlink.c include/openaurora/audio_broker.h include/openaurora/audio_client.h include/openaurora/audio_netlink.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/modules/mod_audio_curve.so: modules/audio_curve/audio_curve.c common/audio_curve.c include/openaurora/audio_curve.h include/openaurora/module.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) $(LDFLAGS)

$(BUILD)/oa-supervise: tools/boot/oa-supervise.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/$(PROJECT): $(CORE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CORE_OBJ) $(LDLIBS)

$(BUILD)/core/%.o: core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/modules/mod_ui_anim.so: modules/ui_anim/ui_anim.c common/ui_anim.c include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) $(LDFLAGS) -lm

$(BUILD)/modules/mod_ui_fb.so: modules/ui_fb/ui_fb.c common/ui_anim.c include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) $(LDFLAGS) -lm

$(BUILD)/modules/mod_audio_mixer.so: modules/audio_mixer/audio_mixer.c common/audio_broker_wire.c include/openaurora/module.h include/openaurora/services.h include/openaurora/audio_broker.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) $(LDFLAGS)

$(BUILD)/modules/mod_cec.so: modules/cec/cec.c common/cec_owner.c common/cec_authority.c common/cec_device.c common/cec_packet.c common/cec.c common/cec_policy.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) -pthread $(LDFLAGS)

$(BUILD)/modules/mod_audio.so: modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) -pthread $(LDFLAGS) \
	  $(if $(filter $(VENDOR_MODULES),audio),$(VENDOR_LDFLAGS),)

$(BUILD)/modules/mod_display.so: modules/display/display.c common/ui_anim.c include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) $(LDFLAGS) \
	  $(VENDOR_LDFLAGS) -lm

$(BUILD)/modules/mod_%.so: modules/%/*.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $< $(LDFLAGS) \
	  $(if $(filter $(VENDOR_MODULES),$*),$(VENDOR_LDFLAGS),) \
	  $(if $(filter $(SETTINGS_MODULES),$*),-Ithird_party -L$(VENDOR_LIB) $(SQLITE_LIBS),)

vendor:
	@./scripts/fetch-vendor.sh

arm:
	$(MAKE) TARGET=arm CC=$(CROSS) BUILD=build-arm

# TV build: needs a glibc arm-linux-gnueabihf compiler. musl cannot load a glibc .so.
tv:
	@test -n "$(TV_CROSS)" || { \
	  echo "TV_CROSS is unset."; \
	  echo "The vendor HAL is glibc 2.23; set TV_CROSS to an arm-linux-gnueabihf gcc"; \
	  echo "(glibc <= 2.33). See docs/ARCHITECTURE.md 'Toolchain'."; exit 1; }
	@test -f $(VENDOR_LIB)/libNDAL3.so || { echo "run 'make vendor' first"; exit 1; }
	$(MAKE) TARGET=tv CC=$(TV_CROSS) BUILD=build-tv

$(BUILD)/tests/mod_recovery_fixture.so: tests/recovery_fixture.c include/openaurora/module.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $<

$(BUILD)/tests/mod_failed_init.so: tests/failed_init.c include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $<

$(BUILD)/tests/registry: tests/registry.c $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/tests/mod_post_worker.so: tests/post_worker.c tests/post_worker.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $< -pthread $(LDFLAGS)

$(BUILD)/tests/mod_post_fail_init.so: tests/post_worker.c tests/post_worker.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTEST_FAIL_INIT_POST -fPIC -shared -o $@ $< -pthread $(LDFLAGS)

$(BUILD)/tests/mod_post_fail_start.so: tests/post_worker.c tests/post_worker.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTEST_FAIL_START_POST -fPIC -shared -o $@ $< -pthread $(LDFLAGS)

$(BUILD)/tests/mod_abi_%.so: tests/abi_fixture.c include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(if $(filter v1,$*),-DTEST_ABI_V1,) $(if $(filter v2,$*),-DTEST_ABI_V2,) $(if $(filter extended,$*),-DTEST_ABI_EXTENDED,) -fPIC -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/module_safety: tests/module_safety.c tests/post_worker.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

SAFETY_FIXTURES := mod_post_worker mod_post_fail_init mod_post_fail_start mod_abi_v1 mod_abi_v2 mod_abi_short mod_abi_extended
check-safety: all check-quiesce $(BUILD)/tests/module_safety $(addprefix $(BUILD)/tests/,$(addsuffix .so,$(SAFETY_FIXTURES)))
	@$(BUILD)/tests/module_safety $(BUILD)/modules $(BUILD)/tests

$(BUILD)/tests/mod_quiesce_fixture.so: tests/quiesce_fixture.c tests/quiesce_fixture.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/quiesce: tests/quiesce.c tests/quiesce_fixture.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

.PHONY: check-quiesce
check-quiesce: $(BUILD)/tests/quiesce $(BUILD)/tests/mod_quiesce_fixture.so
	@$(BUILD)/tests/quiesce $(BUILD)/tests

$(BUILD)/tests/mod_reload_fixture.so: tests/reload_fixture.c tests/reload_fixture.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/mod_reload_no_restore.so: tests/reload_fixture.c tests/reload_fixture.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRELOAD_WITHOUT_RESTORE -fPIC -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/mod_reload_renamed.so: tests/reload_fixture.c tests/reload_fixture.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRELOAD_RENAMED -fPIC -shared -o $@ $< $(LDFLAGS)

$(BUILD)/tests/reload: tests/reload.c tests/reload_fixture.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS) -Wl,--wrap=malloc

check-reload: $(BUILD)/tests/reload $(BUILD)/tests/mod_reload_fixture.so $(BUILD)/tests/mod_reload_no_restore.so $(BUILD)/tests/mod_reload_renamed.so
	@$(BUILD)/tests/reload $(BUILD)/tests

$(BUILD)/tests/mod_phase_%.so: tests/phases_fixture.c tests/phases.h include/openaurora/module.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPHASE_ID=$(if $(filter b,$*),1,$(if $(filter c replacement,$*),2,0)) \
	  $(if $(filter no_suspend,$*),-DNO_PHASE_HOOKS,) $(if $(filter replacement,$*),-DPHASE_REPLACEMENT,) \
	  -fPIC -shared -o $@ $< $(LDFLAGS)

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

check-power: $(BUILD)/tests/power_cycle $(addprefix $(BUILD)/tests/mod_phase_,$(addsuffix .so,$(PHASE_FIXTURES)))
	@$(BUILD)/tests/power_cycle $(BUILD)/tests

$(BUILD)/tests/ui_clock: tests/ui_clock.c core/host.h include/openaurora/ui_time.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $< $(filter %.o,$^) $(LDLIBS)

check-ui-clock: $(BUILD)/tests/ui_clock $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/ui_clock $(BUILD)/modules

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

$(BUILD)/tests/cec_owner: tests/cec_owner.c tests/lifecycle_retry.h common/cec_owner.c common/cec_authority.c common/cec_device.c common/cec_packet.c common/cec.c core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-cec-owner: check-cec-async $(BUILD)/tests/cec_owner $(BUILD)/modules/mod_cec.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/cec_owner $(BUILD)/modules

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

check-audio-curve-module: $(BUILD)/tests/audio_curve_module $(BUILD)/modules/mod_audio_curve.so $(BUILD)/modules/mod_ticker.so
	@$(BUILD)/tests/audio_curve_module $(BUILD)/modules

$(BUILD)/tests/audio: tests/audio.c modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c include/openaurora/vendor_audio.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -pthread

$(BUILD)/tests/plat_audio: tests/plat_audio.c modules/plat/plat.c include/openaurora/vendor_audio.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

check-cec-audio: $(BUILD)/tests/cec $(BUILD)/tests/audio $(BUILD)/tests/plat_audio
	@$(BUILD)/tests/cec
	@$(BUILD)/tests/audio
	@$(BUILD)/tests/plat_audio

$(BUILD)/tests/audio-mods/mod_audio.so: modules/audio/audio.c common/audio_owner.c common/audio_client.c common/audio_netlink.c tests/audio_hal_stub.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -UOA_NO_VENDOR $(CFLAGS) -fPIC -shared -MMD -MP -o $@ $(filter %.c,$^) -pthread $(LDFLAGS)

$(BUILD)/tests/audio_session: tests/audio_session.c tests/lifecycle_retry.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-audio-session: $(BUILD)/tests/audio_session $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/modules/mod_plat.so
	@mkdir -p $(BUILD)/tests/audio-mods/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/tests/audio-mods/modules/mod_plat.so
	@cp $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/tests/audio-mods/modules/mod_audio.so
	@printf 'audio.enable = 1\n' > $(BUILD)/tests/audio-mods/audio.cfg
	@$(BUILD)/tests/audio_session $(BUILD)/tests/audio-mods/modules $(BUILD)/tests/audio-mods/audio.cfg

$(BUILD)/tests/cec_audio_policy: tests/cec_audio_policy.c tests/lifecycle_retry.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDLIBS)

check-cec-audio-policy: $(BUILD)/tests/cec_audio_policy $(BUILD)/modules/mod_cec.so $(BUILD)/modules/mod_plat.so $(BUILD)/tests/audio-mods/mod_audio.so
	@mkdir -p $(BUILD)/tests/cec-route/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_cec.so $(BUILD)/tests/cec-route/modules/
	@cp $(BUILD)/tests/audio-mods/mod_audio.so $(BUILD)/tests/cec-route/modules/mod_audio.so
	@printf 'audio.enable = 1\n' > $(BUILD)/tests/cec-route/audio.cfg
	@$(BUILD)/tests/cec_audio_policy $(BUILD)/tests/cec-route/modules $(BUILD)/tests/cec-route/audio.cfg

$(BUILD)/tests/ui_nav_display.so: tests/ui_nav_display.c tests/ui_nav_trace.h include/openaurora/services.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests -fPIC -shared -o $@ tests/ui_nav_display.c $(LDFLAGS)

$(BUILD)/tests/ui_nav: tests/ui_nav.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_nav.c $(filter %.o,$^) $(LDLIBS)

check-ui-nav: check-ui-anim-review $(BUILD)/tests/ui_nav $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so
	@mkdir -p $(BUILD)/tests/ui-nav/modules
	@cp $(BUILD)/modules/mod_plat.so $(BUILD)/modules/mod_ir.so $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ticker.so $(BUILD)/modules/mod_ui_clock.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_settings.so $(BUILD)/tests/ui-nav/modules/
	@cp $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-nav/modules/mod_nav_display.so
	@sqlite3 $(CURDIR)/$(BUILD)/tests/ui-nav/settings.db 'DROP TABLE IF EXISTS Settings; CREATE TABLE Settings (m_current_source INTEGER); INSERT INTO Settings (m_current_source) VALUES (4);'
	@printf 'ir.device =\nui.ok_file =\nui.paint = 1\nui.focus = 2\nui.fps = 0\nsettings.db = $(CURDIR)/$(BUILD)/tests/ui-nav/settings.db\nsettings.readonly = 1\n' > $(BUILD)/tests/ui-nav/ui.cfg
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

$(BUILD)/tests/display_present: tests/display_present.c modules/display/display.c common/ui_anim.c include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm \
	  $(foreach call,open close system mmap munmap msync ioctl,-Wl,--wrap=$(call))

$(BUILD)/fbregion: tools/mcu/fbregion.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Explicit opt-in live fbdev diagnostic; never run by make check. No HAL setup.
$(BUILD)/scene-probe: tools/display/scene-probe.c modules/display/display.c common/ui_anim.c include/openaurora/module.h include/openaurora/services.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -DOA_NO_VENDOR $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^) -lm

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

$(BUILD)/egl-scene-probe: tools/display/egl-scene-probe.c common/scene_gles.c common/ui_anim.c include/openaurora/scene_gles.h include/openaurora/ui_anim.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -idirafter /usr/include $(LDFLAGS) -o $@ $(filter %.c,$^) $(EGL_PROBE_LIBS) -pthread -lm

$(BUILD)/cec-probe: tools/cec/cec-probe.c common/cec_packet.c common/cec.c include/openaurora/vendor_cec.h include/openaurora/cec.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

$(BUILD)/audio-state: tools/audio/audio-state.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/amp-probe: tools/audio/amp-probe.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

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

$(BUILD)/tests/svg_animations: tests/svg_animations.c tools/re/svg-animations.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/tests/svg_names: tests/svg_names.c tools/re/svg-names.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

check-svg: $(BUILD)/svg-animations $(BUILD)/svg-names $(BUILD)/tests/svg_animations $(BUILD)/tests/svg_names
	@$(BUILD)/tests/svg_animations
	@$(BUILD)/tests/svg_names

$(BUILD)/tests/fbregion_read: tests/fbregion_read.c tools/mcu/fbregion.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl

check-display: $(BUILD)/tests/display_present $(BUILD)/tests/fbregion_read
	@$(BUILD)/tests/display_present
	@$(BUILD)/tests/fbregion_read

check-runtime: all
	@BUILD=$(BUILD) CC='$(CC)' CFLAGS='$(CFLAGS)' sh tests/runtime_check.sh

check-ui-slide: $(BUILD)/tests/ui_slide $(BUILD)/tests/mod_fake_clock.so $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/ui_nav_display.so
	@mkdir -p $(BUILD)/tests/ui-slide/modules $(BUILD)/tests/ui-slide/ui
	@cp $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-slide/modules/
	@mv -f $(BUILD)/tests/ui-slide/modules/ui_nav_display.so $(BUILD)/tests/ui-slide/modules/mod_nav_display.so
	@printf 'ui.slide_box = 100\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-slide/ui\n' > $(BUILD)/tests/ui-slide/ui.cfg
	@$(BUILD)/tests/ui_slide $(BUILD)/tests/ui-slide/modules $(BUILD)/tests/ui-slide/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-slide/ui

$(BUILD)/tests/ui_slide: tests/ui_slide.c tests/ui_nav_trace.h include/openaurora/ui_anim.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_slide.c $(filter %.o,$^) $(LDLIBS)

check-ui-rebind: $(BUILD)/tests/ui_rebind $(BUILD)/tests/mod_fake_clock.so $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/modules/mod_ticker.so
	@mkdir -p $(BUILD)/tests/ui-rebind/modules $(BUILD)/tests/ui-rebind/ui
	@cp $(BUILD)/modules/mod_ui_fb.so $(BUILD)/modules/mod_ui_anim.so $(BUILD)/modules/mod_ticker.so $(BUILD)/tests/mod_fake_clock.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-rebind/modules/
	@mv -f $(BUILD)/tests/ui-rebind/modules/ui_nav_display.so $(BUILD)/tests/ui-rebind/modules/mod_nav_display.so
	@printf 'ui.slide_box = 100\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-rebind/ui\n' > $(BUILD)/tests/ui-rebind/ui.cfg
	@$(BUILD)/tests/ui_rebind $(BUILD)/tests/ui-rebind/modules $(BUILD)/tests/ui-rebind/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-rebind/ui

$(BUILD)/tests/ui_rebind: tests/ui_rebind.c tests/ui_nav_trace.h include/openaurora/ui_anim.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_rebind.c $(filter %.o,$^) $(LDLIBS)

check-ui-rail-hl: $(BUILD)/tests/ui_rail_hl $(BUILD)/modules/mod_ui_fb.so $(BUILD)/tests/ui_nav_display.so
	@mkdir -p $(BUILD)/tests/ui-rail-hl/modules $(BUILD)/tests/ui-rail-hl/ui
	@cp $(BUILD)/modules/mod_ui_fb.so $(BUILD)/tests/ui_nav_display.so $(BUILD)/tests/ui-rail-hl/modules/
	@mv -f $(BUILD)/tests/ui-rail-hl/modules/ui_nav_display.so $(BUILD)/tests/ui-rail-hl/modules/mod_nav_display.so
	@printf 'ui.rail_hl = 1\nui.slide_box = 0\nui.paint = 1\nui.fps = 0\nui.ok_file =\nsettings.db =\nui.dir = $(CURDIR)/$(BUILD)/tests/ui-rail-hl/ui\n' > $(BUILD)/tests/ui-rail-hl/ui.cfg
	@$(BUILD)/tests/ui_rail_hl $(BUILD)/tests/ui-rail-hl/modules $(BUILD)/tests/ui-rail-hl/ui.cfg $(CURDIR)/$(BUILD)/tests/ui-rail-hl/ui

$(BUILD)/tests/ui_rail_hl: tests/ui_rail_hl.c tests/ui_nav_trace.h core/host.h $(filter-out $(BUILD)/core/main.o,$(CORE_OBJ))
	$(CC) $(CPPFLAGS) $(CFLAGS) -Icore -Itests $(LDFLAGS) -o $@ tests/ui_rail_hl.c $(filter %.o,$^) $(LDLIBS)

$(BUILD)/tests/mod_fake_clock.so: tests/mod_fake_clock.c include/openaurora/module.h include/openaurora/services.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -MMD -MP -o $@ tests/mod_fake_clock.c $(LDFLAGS) $(LDLIBS)

check: all check-safety check-reload check-phases check-power check-ui-clock check-ui-nav check-ui-slide check-ui-rebind check-ui-rail-hl check-cec-audio check-audio-session check-cec-audio-policy check-cec-packet check-cec-device check-cec-owner check-audio-curve check-audio-curve-module check-audio-netlink check-audio-client check-audio-owner check-control check-display check-svg check-runtime $(BUILD)/tests/mod_recovery_fixture.so $(BUILD)/tests/mod_failed_init.so $(BUILD)/tests/registry
	@BUILD=$(BUILD) sh tests/run.sh
	@$(BUILD)/tests/registry $(BUILD)/modules $(BUILD)/tests/mod_failed_init.so
	@BUILD=$(BUILD) python3 tests/recovery.py

clean:
	rm -rf $(BUILD) build-arm build-tv

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILD)/$(PROJECT) $(DESTDIR)$(BINDIR)/$(PROJECT)
	install -m 755 $(BUILD)/oa-supervise $(DESTDIR)$(BINDIR)/oa-supervise
	install -m 755 $(BUILD)/oa-audio-broker $(DESTDIR)$(BINDIR)/oa-audio-broker
	install -m 755 $(MOD_SO) $(DESTDIR)$(LIBDIR)/

-include $(CORE_DEP) $(MOD_DEP)

.PHONY: all check check-safety check-reload check-phases check-power check-ui-clock check-ui-nav check-cec-audio check-cec-packet check-cec-device check-audio-curve check-audio-curve-module check-audio-netlink check-audio-client check-control check-display check-svg check-runtime clean install arm tv vendor
