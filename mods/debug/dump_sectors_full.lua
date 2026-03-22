-- Dump FULL sector objects (0xD0 bytes each)
-- Run in CE Lua console while in-game (inside house)

local sectorSys = readInteger(0xE416C4)
if not sectorSys or sectorSys == 0 then print("[ERR] E416C4 null"); return end

local sectorArr = readInteger(sectorSys + 0x64)
if not sectorArr or sectorArr == 0 then print("[ERR] array null"); return end

local count = readInteger(0x0133FEF0) or 0
print(string.format("system=%08X array=%08X count=%d", sectorSys, sectorArr, count))

for i = 0, math.min(count, 14) - 1 do
  local obj = readInteger(sectorArr + i * 4)
  if obj and obj ~= 0 then
    -- Read name (first 16 bytes)
    local name = readString(obj, 16) or "?"
    print(string.format("\n=== SECTOR [%d] \"%s\" obj=%08X ===", i, name, obj))

    -- Dump all 0xD0 bytes in rows of 32
    for row = 0, 0xCC, 0x20 do
      local line = string.format("  +%03X:", row)
      for col = 0, 0x1C, 4 do
        local off = row + col
        if off < 0xD0 then
          local val = readInteger(obj + off) or 0
          line = line .. string.format(" %08X", val)
        end
      end
      -- Also show as floats for the row
      local fline = "  flt :"
      for col = 0, 0x1C, 4 do
        local off = row + col
        if off < 0xD0 then
          local fval = readFloat(obj + off) or 0
          if math.abs(fval) > 0.001 and math.abs(fval) < 100000 then
            fline = fline .. string.format(" %8.2f", fval)
          else
            fline = fline .. "        -"
          end
        end
      end
      print(line)
      print(fline)
    end
  end
end
