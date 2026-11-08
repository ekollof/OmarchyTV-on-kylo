-- OmarchyTV native widget panes (editable).
--
-- The ui.lua runtime is sandboxed: no shell, no sockets, no framebuffer or
-- device access. Regular-file reads (including /proc), os.date/time/clock,
-- and oa.fetch(http/https, callback) are available. Writes stay under
-- ui.lua.data. The native skin renders up to 3 panes in the home widget row;
-- native status boxes fill any slots a script leaves unused.
--
-- oa.pane{ id="...", title="...", lines={ ... } }
--   id      stable key, <= 31 bytes (defaults to title)
--   title   <= 23 bytes
--   lines   <= 8; each is a string or
--           { label="<=15 bytes", value="<=63 bytes", tone="up"|"down" }
-- A missing pane is not an error; call oa.pane again to update it.

local function first_line(path)
  local f = io.open(path, "r")
  if not f then return nil end
  local s = f:read("*l")
  f:close()
  return s
end

local function memory_used()
  local f = io.open("/proc/meminfo", "r")
  if not f then return "--" end
  local total, available = 0, 0
  for line in f:lines() do
    local key, value = line:match("^(%w+):%s+(%d+)")
    if key == "MemTotal" then total = tonumber(value) end
    if key == "MemAvailable" then available = tonumber(value) end
  end
  f:close()
  if not total or total == 0 then return "--" end
  return string.format("%d%% used", math.floor((total - available) * 100 / total))
end

local function uptime()
  local s = first_line("/proc/uptime")
  local secs = s and tonumber(s:match("^(%d+)")) or nil
  if not secs then return "--" end
  return string.format("%dh %02dm", math.floor(secs / 3600),
                       math.floor((secs % 3600) / 60))
end

function oa_paint(skin)
  if skin ~= "omarchy" then return end

  local load = first_line("/proc/loadavg") or "--"
  oa.pane{ id = "system", title = "system", lines = {
    { label = "load",   value = load:sub(1, 40) },
    { label = "memory", value = memory_used() },
    { label = "uptime", value = uptime() },
  } }

  oa.pane{ id = "clock", title = "clock", lines = {
    { label = "date", value = os.date("%a %d %b") },
    { label = "time", value = os.date("%H:%M:%S") },
    { label = "zone", value = os.date("%Z") },
  } }

  -- Weather / RSS / local service example. Uncomment and set a URL you trust.
  -- oa.fetch is asynchronous: the callback runs on a later loop turn, and the
  -- next paint publishes the result.
  --
  -- if not oa.state.wx_pending then
  --   oa.state.wx_pending = true
  --   oa.fetch("https://example.invalid/weather.txt", function(body, err)
  --     oa.state.wx_pending = nil
  --     if body then oa.state.wx = body:sub(1, 4000) end
  --   end)
  -- end
  -- if oa.state.wx then
  --   oa.pane{ id = "weather", title = "weather", lines = {
  --     oa.state.wx:sub(1, 63),
  --   } }
  -- end
end
