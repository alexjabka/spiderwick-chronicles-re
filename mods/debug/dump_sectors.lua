-- Dump sector array from [[E416C4]+0x64]
-- Run in CE Lua console while in-game

local sectorSys = readInteger(0xE416C4)
if not sectorSys or sectorSys == 0 then
  print("[Sectors] ERROR: dword_E416C4 is null")
  return
end

local sectorArr = readInteger(sectorSys + 0x64)
if not sectorArr or sectorArr == 0 then
  print("[Sectors] ERROR: sector array is null")
  return
end

local count = readInteger(0x0133FEF0) or 0
print(string.format("[Sectors] system=%08X  array=%08X  count=%d", sectorSys, sectorArr, count))

for i = 0, count - 1 do
  local obj = readInteger(sectorArr + i * 4)
  if obj and obj ~= 0 then
    -- Read key fields from sector object
    local f3C = readInteger(obj + 0x3C) or 0    -- passed to sub_57F180
    local f4B0 = readInteger(obj + 0x4B0) or 0  -- geometry data (from sub_1EE2000)
    local f888 = readInteger(obj + 0x888) or 0  -- output count
    local f88C = readInteger(obj + 0x88C) or 0  -- output list
    local vtable = readInteger(obj) or 0

    -- Position? (from sub_527EC0: this[427-430] = position at 0x6AC-0x6B8)
    local px = readFloat(obj + 0x6AC) or 0
    local py = readFloat(obj + 0x6B0) or 0
    local pz = readFloat(obj + 0x6B4) or 0

    print(string.format(
      "  [%2d] obj=%08X vt=%08X  +3C=%08X  +4B0=%08X  +888=%d  pos=(%.1f, %.1f, %.1f)",
      i, obj, vtable, f3C, f4B0, f888, px, py, pz
    ))

    -- Dump first 16 dwords to find portal/connection data
    local line = "       "
    for off = 0, 60, 4 do
      line = line .. string.format(" %08X", readInteger(obj + off) or 0)
      if off % 32 == 28 then
        print(line)
        line = "       "
      end
    end
    if #line > 7 then print(line) end
  else
    print(string.format("  [%2d] NULL", i))
  end
end
