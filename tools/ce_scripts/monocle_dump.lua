--[[
  monocle_dump.lua — Monocle RE Diagnostic Tool

  Run in CE Lua console AFTER freecam script is enabled.
  Dumps monocle object internals: vtable, flags, parameters, pointers.

  Usage: Execute in CE Lua Engine window (Ctrl+Alt+L)
  Requires: freecam script enabled (provides fc_mono_this symbol)
]]

local function addr(sym)
  local ok, a = pcall(getAddress, sym)
  if not ok or not a or a == 0 then return nil end
  return a
end

local function hex(v)
  if not v then return "nil" end
  return string.format("%08X", v)
end

local function readDword(a)
  if not a then return nil end
  return readInteger(a)
end

local function readByte(a)
  if not a then return nil end
  return readBytes(a, 1)
end

print("==========================================")
print("  Monocle Diagnostic Dump")
print("==========================================")

-- Get monocle this pointer (saved by Hook 3)
local pThis = addr("fc_mono_this")
if not pThis then
  print("[!] fc_mono_this symbol not found. Enable freecam script first.")
  return
end

local this = readDword(pThis)
if not this or this == 0 then
  print("[!] fc_mono_this is null. Enter gameplay and wait a frame.")
  return
end

print(string.format("\n[OBJECT] this = %s", hex(this)))

-- Read vtable pointer
local vtable = readDword(this)
print(string.format("[VTABLE] vtable ptr = %s", hex(vtable)))

if vtable and vtable ~= 0 then
  -- Dump first 20 vtable entries
  print("\n--- VTable Entries ---")
  for i = 0, 19 do
    local entry = readDword(vtable + i * 4)
    local marker = ""
    if i == 16 then marker = "  *** MOUSE ROTATION HANDLER ***" end
    if i == 3 then marker = "  (vtable[0x0C/4] = component process)" end
    if i == 4 then marker = "  (vtable[0x10/4] = component finalize)" end
    if i == 11 then marker = "  (vtable[0x2C/4] = component update)" end
    print(string.format("  vtable[%2d] (+%02X) = %s%s", i, i*4, hex(entry), marker))
  end

  -- vtable[16] is the key function
  local mouseFunc = readDword(vtable + 16 * 4)
  if mouseFunc then
    local exeBase = getAddress("Spiderwick.exe")
    if exeBase then
      print(string.format("\n[KEY] vtable[16] = %s (Spiderwick.exe+%X)", hex(mouseFunc), mouseFunc - exeBase))
    end
  end
end

-- Monocle flags
print("\n--- Monocle Flags ---")
local b8 = readByte(this + 0xB8)
local b9 = readByte(this + 0xB9)
local bb = readByte(this + 0xBB)
print(string.format("  this+0xB8 (transition) = %s", b8 and tostring(b8) or "nil"))
print(string.format("  this+0xB9 (active)     = %s", b9 and tostring(b9) or "nil"))
print(string.format("  this+0xBB (init)       = %s", bb and tostring(bb) or "nil"))

-- pAnotherCamera pointer at this+0x78
local pAnother = readDword(this + 0x78)
print(string.format("\n--- Monocle Pointers ---"))
print(string.format("  this+0x78 (pAnotherCamera/this[0x1E]) = %s", hex(pAnother)))

-- Scale at this+0x90
local scale = readFloat(this + 0x90)
print(string.format("  this+0x90 (scale) = %s", scale and string.format("%.4f", scale) or "nil"))

-- Internal state at this+0x38 (16 floats, saved monocle output)
print("\n--- Internal State (this+0x38, 16 floats) ---")
for i = 0, 15 do
  local v = readFloat(this + 0x38 + i * 4)
  if v and math.abs(v) > 0.0001 then
    print(string.format("  this+0x%02X [%2d] = %.6f", 0x38 + i*4, i, v))
  end
end

-- this[1] (offset +0x04) — saved by Path 1 init
local this1 = readDword(this + 0x04)
print(string.format("\n  this+0x04 (this[1], saved state) = %s", hex(this1)))

-- Monocle config struct at static address 72FC18
print("\n--- Monocle Config (static 72FC18) ---")
local cfgBase = 0x72FC18
-- Try reading. May fail if address not mapped
local ok1, _ = pcall(function()
  local flag0E = readByte(0x72FC0E)
  print(string.format("  72FC0E (flag)       = %s", flag0E and tostring(flag0E) or "nil"))

  local cfg1C = readDword(0x72FC1C)
  print(string.format("  72FC1C (saved)      = %s", hex(cfg1C)))

  local zoom = readFloat(0x72FC30)
  print(string.format("  72FC30 (zoom param) = %s", zoom and string.format("%.2f", zoom) or "nil"))

  local zspeed = readFloat(0x72FC38)
  print(string.format("  72FC38 (zoom speed) = %s", zspeed and string.format("%.2f", zspeed) or "nil"))

  local flag4E = readByte(0x72FC4E)
  print(string.format("  72FC4E (flag)       = %s", flag4E and tostring(flag4E) or "nil"))
end)
if not ok1 then print("  (could not read static config — addresses may differ)") end

-- pAnotherCamera details (if valid)
if pAnother and pAnother ~= 0 then
  print(string.format("\n--- pAnotherCamera (%s) ---", hex(pAnother)))
  -- Path 3 reads v6[0x0E], v6[0x0F], v6[0x16], v6[0x17], v6[0x18]
  -- These are float array indices, so offsets are index*4
  local v6_0E = readFloat(pAnother + 0x0E * 4)  -- +0x38
  local v6_0F = readFloat(pAnother + 0x0F * 4)  -- +0x3C
  local v6_16 = readFloat(pAnother + 0x16 * 4)  -- +0x58
  local v6_17 = readFloat(pAnother + 0x17 * 4)  -- +0x5C
  local v6_18 = readFloat(pAnother + 0x18 * 4)  -- +0x60
  print(string.format("  v6[0x0E] (+0x38) = %s", v6_0E and string.format("%.4f", v6_0E) or "nil"))
  print(string.format("  v6[0x0F] (+0x3C) = %s", v6_0F and string.format("%.4f", v6_0F) or "nil"))
  print(string.format("  v6[0x16] (+0x58) = %s", v6_16 and string.format("%.4f", v6_16) or "nil"))
  print(string.format("  v6[0x17] (+0x5C) = %s", v6_17 and string.format("%.4f", v6_17) or "nil"))
  print(string.format("  v6[0x18] (+0x60) = %s", v6_18 and string.format("%.4f", v6_18) or "nil"))

  -- Compute eye as Path 3 does
  if v6_0E and v6_16 and scale then
    local eyeX = v6_0E * scale + v6_16
    local eyeY = v6_0F * scale + v6_17
    local this84 = readFloat(this + 0x84)
    local eyeZ = v6_18 + (this84 or 0)
    print(string.format("  Computed monocle eye = (%.2f, %.2f, %.2f)", eyeX, eyeY, eyeZ))
    print(string.format("  this+0x84 (Z offset) = %s", this84 and string.format("%.4f", this84) or "nil"))
  end
end

-- Camera manager relationship
local pCam = addr("pCamStruct")
if pCam then
  local camBase = readDword(pCam)
  if camBase and camBase ~= 0 then
    local camMgr = camBase - 0x480
    print(string.format("\n--- Camera Manager ---"))
    print(string.format("  pCamStruct     = %s", hex(camBase)))
    print(string.format("  cam manager    = %s (pCamStruct - 0x480)", hex(camMgr)))
    print(string.format("  mono this      = %s", hex(this)))
    if camMgr == this then
      print("  >>> mono this == cam manager (SAME OBJECT)")
    else
      print(string.format("  >>> mono this != cam manager (offset = %+d)", this - camMgr))
    end

    -- Component array at camMgr+0x88
    local compArray = readDword(camMgr + 0x88)
    local compCount = readDword(camMgr + 0xC8)
    print(string.format("  Component array (+0x88) = %s", hex(compArray)))
    print(string.format("  Component count (+0xC8) = %s", compCount and tostring(compCount) or "nil"))

    if compArray and compCount and compCount > 0 and compCount < 20 then
      print("\n--- Camera Components ---")
      for i = 0, compCount - 1 do
        local comp = readDword(compArray + i * 4)
        if comp and comp ~= 0 then
          local compVtable = readDword(comp)
          print(string.format("  component[%d] = %s  vtable = %s", i, hex(comp), hex(compVtable)))
        end
      end
    end
  end
end

print("\n==========================================")
print("  Dump complete. Key info for IDA:")
print("  1. Decompile vtable[16] function")
print("  2. Find where it reads mouse dx/dy")
print("  3. Find the input source structure")
print("==========================================")
