--[[
  Auto Yaw/Pitch Finder

  Scans camera object + its components for float values that change
  when you rotate the camera. Finds yaw/pitch automatically.

  Usage:
  1. In normal game mode (NO freecam), stand still
  2. Execute this script in CE Lua Engine
  3. When prompted, ROTATE camera with mouse (left-right for yaw)
  4. Wait for results — script shows which values changed

  Requires: "Camera Struct" script enabled (pCamStruct symbol).
]]

local pCam = readInteger(getAddress("pCamStruct"))
if not pCam or pCam == 0 then
  print("ERROR: pCamStruct not found. Enable Camera Struct script first.")
  return
end

-- Camera object base (ECX at sub_5299A0 + 0x480 = pCamStruct)
local obj = pCam - 0x480

print(string.format("[YawFinder] pCamStruct = %08X", pCam))
print(string.format("[YawFinder] Camera object = %08X", obj))

-- Collect all addresses to monitor
local addrs = {}

-- Main camera struct (pCamStruct offsets 0 to 0x400)
for off = 0, 0x400, 4 do
  table.insert(addrs, {addr = pCam + off, label = string.format("pCam+%03X", off)})
end

-- Camera manager object (offsets 0 to 0x200)
for off = 0, 0x200, 4 do
  table.insert(addrs, {addr = obj + off, label = string.format("camObj+%03X", off)})
end

-- Camera components (from pseudocode: this[0x22] = array, this[0x32] = count)
local count = readInteger(obj + 0xC8) or 0
if count > 0 and count < 20 then
  print(string.format("[YawFinder] Found %d camera components", count))
  for i = 0, count - 1 do
    local comp = readInteger(obj + 0x88 + i * 4)
    if comp and comp > 0x10000 then
      print(string.format("  Component %d at %08X", i, comp))
      for off = 0, 0x200, 4 do
        table.insert(addrs, {addr = comp + off,
          label = string.format("comp[%d]+%03X (%08X)", i, off, comp + off)})
      end
    end
  end
else
  print("[YawFinder] No components found (count=" .. tostring(count) .. ")")
  print("  Trying pCamStruct-0x480 might be wrong. Scanning wider area.")
  -- Scan around pCamStruct more broadly
  for off = -0x600, -0x100, 4 do
    table.insert(addrs, {addr = pCam + off, label = string.format("pCam%+04X", off)})
  end
end

-- Take snapshot 1
local snap1 = {}
for _, a in ipairs(addrs) do
  local ok, v = pcall(readFloat, a.addr)
  if ok then snap1[a.addr] = v end
end

print(string.format("\n[YawFinder] Snapshot taken. Monitoring %d addresses.", #addrs))
print("[YawFinder] >>> ROTATE camera with mouse NOW! Results in 4 seconds... <<<")

local t = createTimer(nil)
t.Interval = 4000
t.OnTimer = function(timer)
  timer.Enabled = false
  timer.Destroy()

  -- Collect candidates: changed, angle-like values
  local candidates = {}
  for _, a in ipairs(addrs) do
    local v1 = snap1[a.addr]
    local ok, v2 = pcall(readFloat, a.addr)
    if ok and v1 and v2 then
      local delta = math.abs(v2 - v1)
      -- Filter: changed noticeably, value looks like an angle or small float
      if delta > 0.05 and math.abs(v2) < 2000 and delta < 500 then
        table.insert(candidates, {
          label = a.label,
          addr = a.addr,
          v1 = v1,
          v2 = v2,
          delta = delta
        })
      end
    end
  end

  -- Sort by delta (most changed first)
  table.sort(candidates, function(a, b) return a.delta > b.delta end)

  print("\n========== YAW/PITCH CANDIDATES ==========")
  if #candidates == 0 then
    print("  No changes detected!")
    print("  Did you rotate the camera with mouse?")
  else
    -- Show top 30
    local show = math.min(#candidates, 30)
    for i = 1, show do
      local c = candidates[i]
      print(string.format("  %-28s [%08X]  %.4f → %.4f  (Δ %.4f)",
        c.label, c.addr, c.v1, c.v2, c.delta))
    end
    print(string.format("\n  %d candidates total (showing top %d)", #candidates, show))
  end
  print("===========================================")
  print("Look for a value that changes SMOOTHLY with rotation")
  print("(not position, not matrix elements — a clean angle)")
end
t.Enabled = true
