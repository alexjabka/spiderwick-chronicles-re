-- Sector Write Monitor
-- Sets hardware write breakpoint on camera_obj+0x788
-- Logs every instruction that writes to the sector field
-- Run this, then walk around / use freecam, check output

local base = getAddress("Spiderwick.exe")
local camObj = readInteger(base + 0x32F670)

if not camObj or camObj == 0 then
  print("[MONITOR] ERROR: camera_obj not found")
  return
end

local sectorAddr = camObj + 0x788
print(string.format("[MONITOR] camera_obj = 0x%X", camObj))
print(string.format("[MONITOR] Watching sector writes at 0x%X", sectorAddr))
print("[MONITOR] Walk around normally, then try freecam. Watch for writes.")
print("[MONITOR] Press F9 to stop monitoring.")

-- Counter for throttling output
local hitCount = 0
local lastAddr = 0

debug_setBreakpoint(sectorAddr, 4, bptWrite, function(bp)
  hitCount = hitCount + 1
  local eip = EIP or 0
  local sector = readInteger(sectorAddr) or -1

  -- Only print when writer changes or every 100th hit
  if eip ~= lastAddr or hitCount % 100 == 0 then
    print(string.format("[MONITOR] #%d Write from 0x%X sector=%d",
      hitCount, eip, sector))
    lastAddr = eip
  end

  debug_continueFromBreakpoint(co_run)
  return 1
end)

-- F9 to stop
createHotkey(function()
  debug_removeBreakpoint(sectorAddr)
  print(string.format("[MONITOR] Stopped after %d hits", hitCount))
end, VK_F9)
