-- Debug Camera Activator
-- Scans memory for ClDebugCameraManager singleton by its vtable
-- Then tries to toggle its active state

-- MethodValidator vtable for ClDebugCameraManager at offset +0x30 in the object
local VTABLE_MARKER = 0x006282A0
local VTABLE_OFFSET = 0x30  -- this[12] in DWORD* = offset 48 = 0x30
local STATE_OFFSET = 0x70   -- this[28] in DWORD* = offset 112 = 0x70

local function findDebugCameraManager()
  -- Scan game's .data/.bss segments for the vtable marker
  -- The vtable is at object+0x30
  local base = getAddress("Spiderwick.exe")
  -- Scan writable memory regions
  local results = AOBScan(string.format("A0 82 62 00"), "+W-C")  -- 0x006282A0 little-endian
  if not results then
    print("[DebugCam] Vtable marker not found in memory")
    return nil
  end

  local count = results.Count
  print(string.format("[DebugCam] Found %d vtable matches", count))

  for i = 0, count - 1 do
    local matchAddr = tonumber(results[i], 16)
    -- The vtable is at object + 0x30, so object = matchAddr - 0x30
    local objBase = matchAddr - VTABLE_OFFSET
    local state = readInteger(objBase + STATE_OFFSET)
    print(string.format("[DebugCam]   match %d: vtable@%08X obj@%08X state=%s",
      i, matchAddr, objBase, tostring(state)))
  end

  -- Use first match
  local objAddr = tonumber(results[0], 16) - VTABLE_OFFSET
  results.Destroy()
  return objAddr
end

local obj = findDebugCameraManager()
if obj then
  local state = readInteger(obj + STATE_OFFSET)
  print(string.format("[DebugCam] Object at %08X, state = %d", obj, state))

  if state == -1 or state == 0xFFFFFFFF then
    -- Activate: set state to 0 (controller ID 0)
    writeInteger(obj + STATE_OFFSET, 0)
    print("[DebugCam] ACTIVATED (state set to 0)")
  else
    -- Deactivate: set state back to -1
    writeInteger(obj + STATE_OFFSET, -1)
    print("[DebugCam] DEACTIVATED (state set to -1)")
  end
else
  print("[DebugCam] Manager not found")
end
