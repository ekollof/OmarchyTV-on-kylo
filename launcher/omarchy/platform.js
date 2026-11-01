/*
 * OmarchyTV -> Vestel/Novatek MB230 port shim (loads before data.js/app.js).
 *
 * The upstream app is the webOS/Tizen build of OmarchyTV (MIT, gitlab.com/piermark/omarchytv).
 * On this TV the launcher is a plain web page served by busybox httpd on 127.0.0.1:8181 and the
 * only way to drive the set is the TV's own key path (`/cgi-bin/act` -> DIAL on 56789).
 *
 * Two hooks are used (small patches in app.js):
 *   window.OMARCHY_VESTEL_APPS   replaces the LG app list
 *   window.OMARCHY_VESTEL_INPUTS replaces the TV input list
 *   window.OMARCHY_LAUNCH(app)   intercepts launchApp()
 */
(function () {
  "use strict";

  window.OMARCHY_PLATFORM = "vestel";
  // hide menu rows that need webOS services / an AI backend (Cinemarchy, plugins, Ambilight)
  window.OMARCHY_HIDE_UNSUPPORTED = true;

  // 12h/24h: the browser reports en-US here (so upstream's "auto" picked 12 h), but the set is in
  // a 24 h region. Decide from the timezone, then from how the locale actually renders 13:00.
  window.OMARCHY_PREFERS_24H = (function () {
    try {
      var tz = "";
      try { tz = (Intl.DateTimeFormat().resolvedOptions().timeZone || ""); } catch (e1) {}
      // this browser reports a bare "CET" (not "Europe/Amsterdam"), so accept the legacy zone names
      if (/^(CET|CEST|MET|MEST|EET|EEST|WET|WEST|GMT|UTC|Europe\/|Africa\/|Asia\/|Australia\/)/.test(tz)) return true;
      if (/^America\/(New_York|Chicago|Denver|Los_Angeles|Toronto|Vancouver|Phoenix|Anchorage|Halifax|St_Johns)/.test(tz)) return false;
      return !/am|pm/i.test(new Date(2019, 0, 1, 13, 0, 0).toLocaleTimeString());
    } catch (e) {
      return true;   // 24 h is the safer default for this set
    }
  })();

  // OmarchyTV is laid out for 1920x1080 CSS px; this TV's browser gives us 1280x720 CSS
  // (1920x1080 device px at DPR 1.5) and ignores the meta scale. Transform the body instead:
  // a transformed ancestor also becomes the containing block for position:fixed children.
  (function () {
    // device px / DPR = CSS px of the panel (1920/1.5 = 1280 here); the meta viewport is
    // unreliable on this browser, so derive the factor from the panel itself.
    var cssW = (window.screen && window.screen.width ? window.screen.width : 1920) / (window.devicePixelRatio || 1);
    var scale = Math.min(cssW / 1920, 1);
    var st = document.createElement("style");
    st.textContent =
      "html,body{margin:0!important;padding:0!important;overflow:hidden!important;background:#000}" +
      // this set has no Bluetooth audio, no battery, and the SpaceX pill is a placeholder countdown
      "[data-action=bt],[data-action=batt],#spacex-pill{display:none!important}" +
      // no AI backend: the Grok companion chip would only show "not available"
      "#grokbot-companion{display:none!important}";
    (document.head || document.documentElement).appendChild(st);
  })();

  function act(query) {
    // fire-and-forget: some actions (tv, sources) close this page, so never wait for a reply
    var i = new Image();
    i.src = "/cgi-bin/act?" + query + "&t=" + new Date().getTime();
  }

  // What this TV can actually do, mapped onto the app's tile model.
  // Default weather location for this set. Upstream ships the author's town (Alessandria, IT), so
  // seed the saved config when it still holds that default -- no app.js change needed.
  window.OMARCHY_VESTEL_WEATHER = { city: "Heiloo", lat: 52.60252, lon: 4.68815 };

  // Host running tools/pcstatus.py (the Omarchy laptop) -- powers the "omarchy · pc" and AI panels
  window.OMARCHY_VESTEL_PC = "http://127.0.0.1:8181/cgi-bin/stats";
  (function () {
    try {
      var K = "omarchytv_cfg_2110", W = window.OMARCHY_VESTEL_WEATHER;
      var c = {}; try { c = JSON.parse(localStorage.getItem(K) || "{}") || {}; } catch (e2) {}
      // only replace a value that was never chosen by the user (upstream default or our old guess)
      var KNOWN_DEFAULTS = ["", "Alessandria", "Amsterdam"];
      var KNOWN_PC = ["", "pc", "omarchy-pc", "192.168.178.167:8765"];
      // no AI backend on this TV: keep that panel hidden (upstream would show fake $ figures).
      // Merge the full default set -- writing a partial object would hide every panel.
      c.widgets = Object.assign({ term: true, weather: true, pc: true, ai: true, deploy: false }, c.widgets || {});
      c.widgets.ai = false;
      // PC status / AI panels: point at the host running tools/pcstatus.py unless the user set one
      if (KNOWN_PC.indexOf(c.pcHost || "") >= 0 && window.OMARCHY_VESTEL_PC) {
        c.pcHost = window.OMARCHY_VESTEL_PC;
      }
      // Dutch UI for this set unless a language was chosen in the app (upstream has no nl)
      if (!c.lang || c.lang === "auto") c.lang = "nl";

      // dev/test hook: ?theme=<id> forces a theme (used to restore the default after testing)
      var mTheme = /[?&]theme=([a-z0-9-]+)/.exec(location.search);
      if (mTheme) { c.theme = mTheme[1]; c.wallpaper = ""; c.wallpaperIdx = 0; }   // "" = follow the theme

      // dev/test hook: ?devsaver=<minutes> shortens the idle timers, ?devsaver=off restores them
      var mSaver = /[?&]devsaver=([0-9.]+)/.exec(location.search);
      if (mSaver) { c.saverIdleMin = parseFloat(mSaver[1]); c.offIdleMin = 0; }
      if (/[?&]devsaver=off/.test(location.search)) { c.saverIdleMin = 2.5; c.offIdleMin = 0; }
      var mOff = /[?&]devoff=([0-9.]+)/.exec(location.search);
      if (mOff) { c.offIdleMin = parseFloat(mOff[1]); c.saverIdleMin = 0; }
      if (KNOWN_DEFAULTS.indexOf(c.weatherCity || "") >= 0) {
        c.weatherCity = W.city; c.weatherLat = W.lat; c.weatherLon = W.lon;
      }
      localStorage.setItem(K, JSON.stringify(c));
    } catch (e) {}
  })();

  window.OMARCHY_VESTEL_APPS = [
    { id: "tv", name: "TV", appId: "vestel.tv", brand: "tv", sub: "stock TV" },
    { id: "sources", name: "Sources", appId: "vestel.sources", brand: "browser",
      bg: "linear-gradient(135deg, #132a3f 0%, #0b1420 100%)", fg: "#38bdf8", sub: "HDMI · AV · VGA" },
    { id: "portal", name: "Stock portal", appId: "vestel.portal", brand: "settings",
      bg: "linear-gradient(135deg, #2a2438 0%, #15121c 100%)", fg: "#c0caf5", sub: "apps & instellingen" }
  ];

  window.OMARCHY_VESTEL_INPUTS = [
    { id: "livetv", name: "TV", sub: "tuner", appId: "vestel.tv" },
    { id: "hdmi1", name: "HDMI 1", sub: "input", appId: "vestel.sources" },
    { id: "hdmi2", name: "HDMI 2", sub: "input", appId: "vestel.sources" },
    { id: "hdmi3", name: "HDMI 3", sub: "input", appId: "vestel.sources" }
  ];

  // Boot handshake + heartbeat for the hook's watchdog (same contract as the mock page):
  //   /cgi-bin/act?a=boot-ok  -> /conf/launcher_ok (the opener stops retrying)
  //   /cgi-bin/act?a=alive    -> /conf/launcher_alive (tells a future watchdog we own the screen)
  (function () {
    var ping = function (q) { var i = new Image(); i.src = "/cgi-bin/act?" + q + "&t=" + new Date().getTime(); };
    if (/[?&]boot=/.test(location.search)) ping("a=boot-ok");
    setInterval(function () { ping("a=alive"); }, 5000);
  })();

  // "Turn off after idle" (System menu): the power key suspends this set even with a page in front
  // (<command>Standby</command> is ignored, and a live page blocks the no-signal timer). The hook runs a
  // resume watcher that reopens the launcher after wake, because the page does not survive suspend.
  window.OMARCHY_STANDBY = function () { act("a=sleep"); };

  window.OMARCHY_LAUNCH = function (app) {
    var id = String((app && (app.appId || app.id)) || "");
    if (id === "vestel.tv" || id === "livetv" || id === "tv") { act("a=tv"); return; }        // EXIT -> stock UI
    if (id === "vestel.sources" || /^hdmi/.test(id)) { act("a=sources"); return; }              // native Bronlijst
    act("a=apps");                                                                              // stock portal
  };
})();
