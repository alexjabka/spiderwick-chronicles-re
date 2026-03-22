--[[
  wheel_scan.lua — Find mouse wheel delta address

  Monitors 72FB00-72FD00 area for float/int values that change
  when scrolling the mouse wheel. Run in CE Lua console.

  Usage:
    1. Execute this script
    2. Scroll mouse wheel up/down in game
    3. Check CE output for addresses that changed
    4. Stop after a few seconds (auto-stops after 5 sec)
]]

local SCAN_START = 0x72FB00
local SCAN_END   = 0x72FD00
local STEP       = 4

-- Snapshot current values
local snapshot = {}
for addr = SCAN_START, SCAN_END, STEP do
  snapshot[addr] = readInteger(addr)
end

print("[WHEEL] Initial snapshot taken. Scroll the mouse wheel NOW!")
print("[WHEEL] Monitoring for 5 seconds...")

local changed = {}
local ticks = 0

local timer = createTimer(getMainForm())
timer.Interval = 100
timer.OnTimer = function()
  ticks = ticks + 1

  for addr = SCAN_START, SCAN_END, STEP do
    local cur = readInteger(addr)
    if cur and cur ~= snapshot[addr] then
      if not changed[addr] then
        changed[addr] = { old = snapshot[addr], new = cur, count = 1 }
      else
        changed[addr].new = cur
        changed[addr].count = changed[addr].count + 1
      end
    end
  end

  if ticks >= 50 then  -- 5 seconds
    timer.Destroy()
    print("\n[WHEEL] Scan complete. Changed addresses:")
    print("--------------------------------------------------")

    -- Filter out mouse XY deltas (known) and high-frequency noise
    local found = false
    for addr = SCAN_START, SCAN_END, STEP do
      if changed[addr] then
        local c = changed[addr]
        local skip = false
        -- Skip known addresses
        if addr == 0x72FC10 or addr == 0x72FC14 then skip = true end

        if not skip then
          local fOld = readFloat(addr) or 0
          print(string.format("  %08X: %08X → %08X  (as float: %.4f)  changes: %d",
            addr, c.old or 0, c.new or 0, fOld, c.count))
          found = true
        end
      end
    end

    if not found then
      print("  No changes detected (besides mouse XY).")
      print("  Wheel delta might be outside 72FB00-72FD00 range.")
    end
    print("--------------------------------------------------")
  end
end
timer.Enabled = true
