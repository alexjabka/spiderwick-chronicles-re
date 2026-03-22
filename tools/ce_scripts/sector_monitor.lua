--[[
  Sector State Array Monitor
  ==========================
  Monitors 0x01C8E82C — 12 bytes per sector, sector count from 0x0133FEF0.
  Takes snapshots every 500ms, highlights changes between frames.

  Walk between rooms and watch which DWORDs change per sector.
  Press "Stop" in Lua Engine window or run sector_monitor_stop() to stop.

  Usage: paste in CE Lua Engine (Ctrl+Alt+L), click Execute.
  Requires: game running, indoors (sector count > 0).
]]

local STATE_ARRAY = 0x01C8E82C
local SECTOR_COUNT_ADDR = 0x0133FEF0
local BITMASK_ADDR = 0x01340080
local INTERVAL_MS = 500

-- Previous snapshot for diff
local prev = {}
local snapCount = 0

local function readSectors()
  local count = readInteger(SECTOR_COUNT_ADDR) or 0
  if count <= 0 or count > 64 then return nil, 0 end

  local data = {}
  for i = 0, count - 1 do
    local base = STATE_ARRAY + i * 12
    data[i] = {
      readInteger(base),
      readInteger(base + 4),
      readInteger(base + 8)
    }
  end
  return data, count
end

local function formatDword(val)
  if val == nil then return "????????" end
  return string.format("%08X", val)
end

local function printSnapshot(data, count, bitmask)
  snapCount = snapCount + 1
  local hasChanges = false

  print(string.format("\n=== Snapshot #%d | Bitmask: %08X | Sectors: %d ===",
    snapCount, bitmask, count))
  print("Sector |   +0x00    |   +0x04    |   +0x08    |")
  print("-------|------------|------------|------------|")

  for i = 0, count - 1 do
    local d = data[i]
    local p = prev[i]
    local marks = {"", "", ""}

    if p then
      for j = 1, 3 do
        if d[j] ~= p[j] then
          marks[j] = " *CHG*"
          hasChanges = true
        end
      end
    end

    print(string.format("  [%2d] | %s%s | %s%s | %s%s |",
      i,
      formatDword(d[1]), marks[1],
      formatDword(d[2]), marks[2],
      formatDword(d[3]), marks[3]
    ))
  end

  if snapCount > 1 and not hasChanges then
    print("  (no changes from previous snapshot)")
  end
end

-- Timer-based monitor
local monTimer = nil

function sector_monitor_stop()
  if monTimer then
    monTimer.Enabled = false
    monTimer.Destroy()
    monTimer = nil
    print("\n[SectorMon] Stopped.")
  else
    print("[SectorMon] Not running.")
  end
end

-- Stop previous instance if re-running
sector_monitor_stop()

print("[SectorMon] Starting sector state monitor...")
print("[SectorMon] Walk between rooms and watch for *CHG* markers.")
print("[SectorMon] Call sector_monitor_stop() to stop.\n")

-- Initial snapshot
local data, count = readSectors()
if not data then
  print("[SectorMon] ERROR: sector count is 0 or invalid. Are you indoors?")
  return
end

local bitmask = readInteger(BITMASK_ADDR) or 0
printSnapshot(data, count, bitmask)
prev = data

-- Start timer
monTimer = createTimer(nil)
monTimer.Interval = INTERVAL_MS
monTimer.OnTimer = function(timer)
  local data, count = readSectors()
  if not data then return end

  local bitmask = readInteger(BITMASK_ADDR) or 0

  -- Only print if something changed
  local changed = false
  for i = 0, count - 1 do
    local d = data[i]
    local p = prev[i]
    if not p then
      changed = true
      break
    end
    for j = 1, 3 do
      if d[j] ~= p[j] then
        changed = true
        break
      end
    end
    if changed then break end
  end

  -- Also check bitmask change
  if not changed then
    local prevBitmask = readInteger(BITMASK_ADDR)
    -- always print on bitmask change would need storing prev bitmask
    -- for now, just skip if no sector data changed
  end

  if changed then
    printSnapshot(data, count, bitmask)
    prev = data
  end
end
monTimer.Enabled = true
