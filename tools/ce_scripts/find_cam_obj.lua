--[[
  Find camera_obj — the object returned by sub_4368B0.
  Follows the thunk chain sub_4368B0 → sub_447AD6 → nullsub_56
  at RUNTIME (after .kallis patching) to find camera_obj address.

  Then reads camera_obj+0x788 to verify it's a valid sector index.

  Usage: paste in CE Lua Engine, click Execute.
  Requires: game loaded into a level (not main menu).
]]

local base = getAddress("Spiderwick.exe")

-- Follow jmp/call chain and disassemble what we find
local function dumpBytes(addr, count)
  local bytes = readBytes(addr, count, true)
  if not bytes then return "??" end
  local s = ""
  for i = 1, #bytes do s = s .. string.format("%02X ", bytes[i]) end
  return s
end

local function signedDword(val)
  if val > 0x7FFFFFFF then return val - 0x100000000 end
  return val
end

print("=== Thunk chain: sub_4368B0 ===\n")

-- Step 1: sub_4368B0
local addr1 = base + 0x368B0
print(string.format("[1] sub_4368B0 @ %08X: %s", addr1, dumpBytes(addr1, 8)))

local b1 = readBytes(addr1, 1, true)
local addr2 = nil

if b1[1] == 0xE9 then
  addr2 = addr1 + 5 + signedDword(readInteger(addr1 + 1))
  print(string.format("    → JMP %08X", addr2))
elseif b1[1] == 0xE8 then
  addr2 = addr1 + 5 + signedDword(readInteger(addr1 + 1))
  print(string.format("    → CALL %08X", addr2))
else
  print(string.format("    → Unknown opcode %02X, dumping 16 bytes:", b1[1]))
  print("      " .. dumpBytes(addr1, 16))
end

-- Step 2: follow next link
if addr2 then
  print(string.format("\n[2] @ %08X: %s", addr2, dumpBytes(addr2, 8)))
  local b2 = readBytes(addr2, 1, true)
  local addr3 = nil

  if b2[1] == 0xE9 then
    addr3 = addr2 + 5 + signedDword(readInteger(addr2 + 1))
    print(string.format("    → JMP %08X", addr3))
  elseif b2[1] == 0xE8 then
    addr3 = addr2 + 5 + signedDword(readInteger(addr2 + 1))
    print(string.format("    → CALL %08X", addr3))
  elseif b2[1] == 0xFF then
    local modrm = readBytes(addr2 + 1, 1, true)[1]
    if modrm == 0x25 then
      local ptr = readInteger(addr2 + 2)
      local target = readInteger(ptr)
      print(string.format("    → JMP [%08X] = %08X", ptr, target or 0))
      addr3 = target
    else
      print(string.format("    → FF %02X (indirect), dump:", modrm))
      print("      " .. dumpBytes(addr2, 16))
    end
  else
    print(string.format("    → Unknown opcode %02X, dump:", b2[1]))
    print("      " .. dumpBytes(addr2, 16))
  end

  -- Step 3: final function (nullsub_56, patched at runtime)
  if addr3 then
    print(string.format("\n[3] nullsub_56 (patched) @ %08X:", addr3))
    print("    " .. dumpBytes(addr3, 24))

    -- Try to identify common patterns
    local b3 = readBytes(addr3, 2, true)

    if b3[1] == 0xA1 then
      -- mov eax, [imm32]
      local globalAddr = readInteger(addr3 + 1)
      local value = readInteger(globalAddr)
      print(string.format("    → MOV EAX, [%08X]  =  %08X  ← camera_obj!", globalAddr, value or 0))
      print(string.format("\n*** camera_obj = %08X ***", value or 0))
      if value and value ~= 0 then
        local sector = readInteger(value + 0x788)
        print(string.format("*** camera_obj+0x788 = %08X → sector = %s ***",
          value + 0x788,
          sector and string.format("%d", sector) or "nil"))
      end

    elseif b3[1] == 0x8B and (b3[2] == 0x05 or b3[2] == 0x0D or b3[2] == 0x15) then
      -- mov eax/ecx/edx, [imm32]
      local globalAddr = readInteger(addr3 + 2)
      local value = readInteger(globalAddr)
      local regName = ({[0x05]="EAX", [0x0D]="ECX", [0x15]="EDX"})[b3[2]] or "?"
      print(string.format("    → MOV %s, [%08X]  =  %08X", regName, globalAddr, value or 0))

    elseif b3[1] == 0xC3 then
      print("    → RET (still nullsub! .kallis hasn't patched it yet)")
      print("    Make sure you're in-game, not in a menu")

    else
      print("    → Unknown pattern. Disassemble manually in CE:")
      print("    Go to " .. string.format("%08X", addr3) .. " in Memory Viewer")
    end
  end
end

-- Fallback: try calling the function via executeCodeEx
print("\n--- Fallback: calling sub_4368B0 via executeCodeEx ---")
local ok = autoAssemble(string.format([[
alloc(camGetStub, 64)
alloc(camGetResult, 8)
registersymbol(camGetResult)

camGetResult:
  dd 0

camGetStub:
  call %08X
  mov [camGetResult], eax
  ret
]], base + 0x368B0))

if ok then
  executeCodeEx(0, 3000, getAddress("camGetStub"))
  local result = readInteger(getAddress("camGetResult"))

  autoAssemble([[
    unregistersymbol(camGetResult)
    dealloc(camGetResult)
    dealloc(camStub)
  ]])

  if result and result ~= 0 then
    print(string.format("executeCodeEx → camera_obj = %08X", result))
    local sector = readInteger(result + 0x788)
    print(string.format("camera_obj+0x788 = %s", sector and string.format("%d", sector) or "nil"))
  else
    print("executeCodeEx returned 0 or nil")
  end
else
  print("autoAssemble failed for stub")
end

print("\n=== Done. Copy camera_obj address for next step ===")
