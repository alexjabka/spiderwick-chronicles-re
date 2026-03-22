--[[
  One-shot diagnostic: find who calls sub_5A7DC0 for camera position copy.

  This hooks sub_5A7DC0 entry, checks if ECX (destination) points to
  camera position area (camBase+0x380..0x3C0), logs the return address,
  then auto-removes itself after 3 seconds.

  The hook does NOT block the function — it runs normally. Safe to use.

  Usage: paste in CE Lua Engine (Ctrl+Alt+L), click Execute.
  Requires: "Camera Struct" script enabled (pCamStruct symbol).
]]

-- Install logging hook
local ok = autoAssemble([[
alloc(logHook, 256, Spiderwick.exe+1A7DC0)
alloc(logRet, 16)
registersymbol(logRet)

logRet:
  dd 0          // +0: captured return address
  dd 0          // +4: flag (1 = found)

logHook:
  push eax
  push ebx
  mov eax, ecx                  // eax = destination (this)
  mov ebx, [pCamStruct]         // ebx = camera base
  sub eax, ebx                  // eax = offset from camBase
  cmp eax, 0x380
  jb short logHook_skip         // too low, not position area
  cmp eax, 0x3C0
  ja short logHook_skip         // too high
  // ECX is in camera position range!
  mov eax, [esp+8]              // return address (past our 2 pushes)
  mov [logRet], eax
  mov dword ptr [logRet+4], 1
logHook_skip:
  pop ebx
  pop eax
  // Execute original function entry
  push esi
  mov esi, [esp+8]
  jmp logHook_ret

assert(Spiderwick.exe+1A7DC0, 56 8B 74 24 08)
Spiderwick.exe+1A7DC0:
  jmp logHook
logHook_ret:
]])

if not ok then
  print("[CallerFinder] ERROR: autoAssemble failed. Is Camera Struct script enabled?")
  return
end

print("[CallerFinder] Hook installed. Waiting 3 seconds...")

-- After 3 seconds: read result, remove hook
local t = createTimer(nil)
t.Interval = 3000
t.OnTimer = function(timer)
  timer.Enabled = false
  timer.Destroy()

  local retAddr = readInteger(getAddress("logRet"))
  local flag = readInteger(getAddress("logRet") + 4)

  -- Remove hook, restore original bytes
  autoAssemble([[
Spiderwick.exe+1A7DC0:
  db 56 8B 74 24 08

unregistersymbol(logRet)
dealloc(logRet)
dealloc(logHook)
  ]])

  print("[CallerFinder] Hook removed.")

  if flag == 1 then
    local base = getAddress("Spiderwick.exe")
    local offset = retAddr - base
    print("========================================")
    print(string.format("  CALLER FOUND!"))
    print(string.format("  Return address: %08X", retAddr))
    print(string.format("  Spiderwick.exe+%X", offset))
    print(string.format("  In IDA: go to %08X", retAddr))
    print(string.format("  The 'call sub_5A7DC0' is at %08X", retAddr - 5))
    print(string.format("  (Spiderwick.exe+%X)", offset - 5))
    print("========================================")
  else
    print("[CallerFinder] No camera position copy detected in 3 seconds.")
    print("  Make sure the game is running (not paused) and camera is active.")
  end
end
t.Enabled = true
