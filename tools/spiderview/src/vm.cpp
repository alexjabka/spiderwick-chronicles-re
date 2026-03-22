// ============================================================================
// vm.cpp — Kallis VM: SCT script bytecode interpreter
// ============================================================================
// Implements the Stormfront "Kallis" VM as reversed from the Spiderwick
// Chronicles (2008) engine. See vm.h for architecture overview.

#include "vm.h"
#include "formats.h"  // DecompressPCW for ZWD loading
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

// ============================================================================
// Name hash — engine's algorithm: hash += char + (hash << (char & 7))
// ============================================================================

uint32_t KallisVM::HashName(const char* name) {
    uint32_t hash = 0;
    for (const char* p = name; *p; ++p) {
        hash += (uint8_t)*p + (hash << ((uint8_t)*p & 7));
    }
    return hash;
}

// ============================================================================
// Logging
// ============================================================================

void KallisVM::Log(const char* func, const char* detail) {
    if ((int)execLog.size() < maxLogEntries) {
        execLog.push_back({func, detail});
    }
}

// ============================================================================
// Stack operations (public — used by native function stubs)
// ============================================================================

int32_t KallisVM::PopInt() {
    if (stackTop <= 0) { Log("PopInt", "STACK UNDERFLOW"); return 0; }
    return (int32_t)stack[--stackTop];
}

float KallisVM::PopFloat() {
    if (stackTop <= 0) { Log("PopFloat", "STACK UNDERFLOW"); return 0.0f; }
    return AsFloat(stack[--stackTop]);
}

void KallisVM::PopVec3(float out[3]) {
    // VM pops Z, Y, X (reverse order on stack)
    out[2] = PopFloat();
    out[1] = PopFloat();
    out[0] = PopFloat();
}

uint32_t KallisVM::PopObjRef() {
    if (stackTop <= 0) return 0;
    return stack[--stackTop];
}

void KallisVM::PushInt(int32_t v) {
    if (stackTop >= STACK_SIZE) { Log("PushInt", "STACK OVERFLOW"); return; }
    stack[stackTop++] = (uint32_t)v;
}

void KallisVM::PushFloat(float v) {
    if (stackTop >= STACK_SIZE) { Log("PushFloat", "STACK OVERFLOW"); return; }
    stack[stackTop++] = AsUint(v);
}

void KallisVM::PushVec3(const float v[3]) {
    PushFloat(v[0]);
    PushFloat(v[1]);
    PushFloat(v[2]);
}

void KallisVM::PushBool(bool v) {
    PushInt(v ? 1 : 0);
}

// ============================================================================
// SCT Loading
// ============================================================================

bool KallisVM::LoadSCT(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    sctData.resize(size);
    fread(sctData.data(), 1, size, f);
    fclose(f);
    return LoadSCTFromBuffer(sctData.data(), (uint32_t)size);
}

bool KallisVM::LoadSCTFromBuffer(const uint8_t* data, uint32_t size) {
    if (size < 52) return false;

    // Validate header
    if (memcmp(data, "SCT\0", 4) != 0) return false;
    SCTHeader hdr;
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.version != 13) {
        Log("LoadSCT", "Wrong version (expected 13)");
        return false;
    }

    // Keep copy if not already stored
    if (sctData.empty()) {
        sctData.assign(data, data + size);
        data = sctData.data();
    }

    printf("[VM] SCT loaded: %u bytes, NTV=0x%X, VTL=0x%X, StrResolve=0x%X\n",
           size, hdr.ntvOffset, hdr.vtlOffset, hdr.stringResolveOff);

    // Parse sections
    ParseStringResolve(data, hdr.stringResolveOff, size);
    ParseNTV(data, hdr.ntvOffset);
    ParseVTL(data, hdr.vtlOffset);

    // Register built-in native function stubs
    RegisterBuiltinNatives();

    loaded = true;
    printf("[VM] Loaded: %d functions, %d classes, %d name mappings\n",
           (int)functions.size(), (int)classes.size(), (int)nameResolve.size());
    return true;
}

// ============================================================================
// Load SCT from ZWD archive — scan decompressed blob for SCT magic
// ============================================================================

bool KallisVM::LoadSCTFromZWD(const char* zwdPath) {
    std::vector<uint8_t> blob;
    if (!DecompressPCW(zwdPath, blob)) {
        printf("[VM] Failed to decompress ZWD: %s\n", zwdPath);
        return false;
    }
    printf("[VM] Decompressed ZWD: %u bytes from %s\n", (uint32_t)blob.size(), zwdPath);

    // Scan for SCT\0 magic followed by version 13
    for (uint32_t i = 0; i + 52 < (uint32_t)blob.size(); i++) {
        if (blob[i] == 'S' && blob[i+1] == 'C' && blob[i+2] == 'T' && blob[i+3] == 0) {
            uint32_t ver = blob[i+4] | (blob[i+5]<<8) | (blob[i+6]<<16) | (blob[i+7]<<24);
            if (ver == 13) {
                uint32_t dataSize = blob[i+8] | (blob[i+9]<<8) | (blob[i+10]<<16) | (blob[i+11]<<24);
                uint32_t sctTotalSize = dataSize + 52; // header is 52 bytes, dataSize is the rest
                if (i + sctTotalSize <= (uint32_t)blob.size()) {
                    printf("[VM] Found SCT at ZWD offset 0x%X, size %u bytes\n", i, sctTotalSize);
                    return LoadSCTFromBuffer(blob.data() + i, sctTotalSize);
                }
            }
        }
    }

    printf("[VM] No SCT found in ZWD\n");
    return false;
}

// ============================================================================
// Parse NTV / Function Table
// ============================================================================

void KallisVM::ParseNTV(const uint8_t* base, uint32_t offset) {
    if (memcmp(base + offset, "NTV\0", 4) != 0) {
        Log("ParseNTV", "Bad NTV magic");
        return;
    }
    uint32_t count = ReadU32(base + offset + 4);
    printf("[VM] Global NTV: %u raw entries\n", count);

    // NTV entries are triplets: (nameHash, bcOffset, nativeSlot)
    // Each triplet occupies 3 sequential DWORDs in the function table.
    // The engine indexes by raw DWORD position, so we store ALL entries
    // to keep indices aligned with the engine's function table.
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* entry = base + offset + 8 + i * 4;
        uint32_t val = ReadU32(entry);

        VMFunction func;
        func.nameHash = 0;

        // Triplet pattern: [0]=hash, [1]=bcOffset, [2]=0xFFFFFFFF
        int tri = i % 3;
        if (tri == 0) {
            func.nameHash = val;
            auto it = nameResolve.find(val);
            if (it != nameResolve.end()) func.name = it->second;
            func.type = VMFuncType::Native; // hash entry — native placeholder
        } else if (tri == 1) {
            // Bytecode offset
            if (val > 0 && val + 12 < (uint32_t)sctData.size()) {
                func.type = VMFuncType::Bytecode;
                func.bcOffset = val;
                func.bcPtr = base + val + 12;
            }
        } else {
            // Native slot (0xFFFFFFFF = unresolved)
            func.type = VMFuncType::None;
        }

        functions.push_back(func);
        if (func.nameHash != 0)
            funcByHash[func.nameHash] = (int)functions.size() - 1;
    }
    printf("[VM] Function table after global NTV: %d entries\n", (int)functions.size());
}

// ============================================================================
// Parse VTL Class Table
// ============================================================================

void KallisVM::ParseVTL(const uint8_t* base, uint32_t offset) {
    if (offset + 8 > (uint32_t)sctData.size()) return;
    if (memcmp(base + offset, "VTL\0", 4) != 0) {
        Log("ParseVTL", "Bad VTL magic");
        return;
    }
    uint32_t count = ReadU32(base + offset + 4);
    printf("[VM] VTL: %u classes\n", count);

    // Read class offsets
    std::vector<uint32_t> classOffsets;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t off = ReadU32(base + offset + 8 + i * 4);
        classOffsets.push_back(off);
    }

    // Parse each class
    for (uint32_t i = 0; i < count; i++) {
        uint32_t nextOff = (i + 1 < count) ? classOffsets[i + 1] : (uint32_t)sctData.size();
        ParseClassEntry(base, classOffsets[i], nextOff);
    }
}

void KallisVM::ParseClassEntry(const uint8_t* base, uint32_t offset, uint32_t nextOffset) {
    if (offset + 32 > (uint32_t)sctData.size()) return;

    VMClassDef cls;
    cls.nameHash   = ReadU32(base + offset + 0);
    cls.parentHash = ReadU32(base + offset + 4);

    // Resolve class name
    auto it = nameResolve.find(cls.nameHash);
    if (it != nameResolve.end()) cls.name = it->second;

    uint32_t steCount    = ReadU32(base + offset + 8);
    uint32_t steArrayOff = ReadU32(base + offset + 16); // pointer to STE offset array
    uint32_t ntvOff      = ReadU32(base + offset + 24); // pointer to embedded NTV

    // Parse STE array: steCount offsets pointing to STE structures
    if (steArrayOff > 0 && steArrayOff + steCount * 4 <= (uint32_t)sctData.size()) {
        for (uint32_t s = 0; s < steCount; s++) {
            uint32_t steOff = ReadU32(base + steArrayOff + s * 4);
            if (steOff + 12 > (uint32_t)sctData.size()) continue;

            VMClassDef::STE ste;
            // STE structure: "STE\0" + nameHash(4) + methodCount(4) + offsets[]
            if (memcmp(base + steOff, "STE\0", 4) == 0) {
                ste.nameHash = ReadU32(base + steOff + 4);
                uint32_t mcount = ReadU32(base + steOff + 8);
                for (uint32_t m = 0; m < mcount && steOff + 12 + (m+1)*4 <= (uint32_t)sctData.size(); m++) {
                    uint32_t mOff = ReadU32(base + steOff + 12 + m * 4);
                    ste.methodOffsets.push_back(mOff);

                    // Register STE method as function table entry (extends global table)
                    VMFunction func;
                    if (mOff > 0 && mOff + 12 < (uint32_t)sctData.size()) {
                        func.type = VMFuncType::Bytecode;
                        func.bcOffset = mOff;
                        func.bcPtr = base + mOff + 12;
                    }
                    functions.push_back(func);
                }
            }
            cls.states.push_back(ste);
        }
    }

    // Parse embedded NTV (per-class method names) — also extends global function table
    if (ntvOff > 0 && ntvOff + 8 <= (uint32_t)sctData.size()) {
        if (memcmp(base + ntvOff, "NTV\0", 4) == 0) {
            uint32_t ntvCount = ReadU32(base + ntvOff + 4);
            // Append ALL raw entries to global function table (keeps index alignment)
            for (uint32_t m = 0; m < ntvCount; m++) {
                const uint8_t* e = base + ntvOff + 8 + m * 4;
                if (e + 4 > base + sctData.size()) break;
                uint32_t val = ReadU32(e);

                VMFunction func;
                int tri = m % 3;
                if (tri == 0) {
                    func.nameHash = val;
                    auto it2 = nameResolve.find(val);
                    if (it2 != nameResolve.end()) func.name = it2->second;
                    func.type = VMFuncType::Native;
                } else if (tri == 1) {
                    if (val > 0 && val + 12 < (uint32_t)sctData.size()) {
                        func.type = VMFuncType::Bytecode;
                        func.bcOffset = val;
                        func.bcPtr = base + val + 12;
                    }
                } else {
                    func.type = VMFuncType::None;
                }

                functions.push_back(func);
                if (func.nameHash != 0)
                    funcByHash[func.nameHash] = (int)functions.size() - 1;

                // Also track as class method
                if (tri == 0) {
                    VMClassDef::MethodEntry me;
                    me.nameHash = val;
                    if (m + 1 < ntvCount) {
                        uint32_t bcVal = ReadU32(base + ntvOff + 8 + (m+1) * 4);
                        me.bcOffset = bcVal;
                    }
                    me.isNative = (m + 2 < ntvCount &&
                        ReadU32(base + ntvOff + 8 + (m+2) * 4) == 0xFFFFFFFF);
                    cls.methods.push_back(me);
                }
            }
        }
    }

    int idx = (int)classes.size();
    classes.push_back(cls);
    classByHash[cls.nameHash] = idx;

    if (!cls.name.empty()) {
        printf("[VM]   Class '%s' (hash=%08X): %d states, %d methods\n",
               cls.name.c_str(), cls.nameHash, (int)cls.states.size(), (int)cls.methods.size());
    }
}

// ============================================================================
// Parse String Resolve Table
// ============================================================================

void KallisVM::ParseStringResolve(const uint8_t* base, uint32_t offset, uint32_t fileSize) {
    if (offset == 0 || offset + 8 > fileSize) return;

    // Pairs of (nameHash, stringOffset) until we hit a boundary
    uint32_t pos = offset;
    int count = 0;
    while (pos + 8 <= fileSize) {
        uint32_t nameHash = ReadU32(base + pos);
        uint32_t strOff   = ReadU32(base + pos + 4);
        if (nameHash == 0 && strOff == 0 && count > 0) break;
        if (strOff > 0 && strOff < fileSize) {
            // Read null-terminated string
            const char* s = (const char*)(base + strOff);
            size_t maxLen = fileSize - strOff;
            size_t len = strnlen(s, maxLen);
            if (len > 0 && len < 256) {
                nameResolve[nameHash] = std::string(s, len);
                count++;
            }
        }
        pos += 8;
    }
    printf("[VM] String resolve: %d name mappings\n", count);
}

// ============================================================================
// Object management
// ============================================================================

int KallisVM::CreateObject(const char* className, const char* objName) {
    uint32_t classHash = HashName(className);
    auto it = classByHash.find(classHash);
    int classIdx = (it != classByHash.end()) ? it->second : -1;

    VMObject obj;
    obj.classIndex = classIdx;
    obj.name = objName ? objName : "";
    obj.nameHash = objName ? HashName(objName) : 0;
    obj.fields.resize(VMObject::FIELD_SIZE, 0);

    int idx = (int)objects.size();
    objects.push_back(obj);
    return idx;
}

VMObject* KallisVM::GetObject(int index) {
    if (index < 0 || index >= (int)objects.size()) return nullptr;
    return &objects[index];
}

// ============================================================================
// Function calling
// ============================================================================

bool KallisVM::CallFunction(int objIndex, const char* funcName) {
    return CallFunctionByHash(objIndex, HashName(funcName));
}

bool KallisVM::CallFunctionByHash(int objIndex, uint32_t nameHash) {
    // Find function by hash
    auto it = funcByHash.find(nameHash);
    if (it == funcByHash.end()) return false;

    int funcIdx = it->second;
    VMFunction& func = functions[funcIdx];

    if (func.type == VMFuncType::Native && func.nativeFunc) {
        currentObject = objIndex;
        func.nativeFunc(*this);
        totalCallsMade++;
        return true;
    }

    if (func.type == VMFuncType::Bytecode && func.bcPtr) {
        currentObject = objIndex;

        // Set up register banks
        VMObject* obj = GetObject(objIndex);
        if (obj && !obj->fields.empty()) {
            regBank[0] = obj->fields.data();
        } else {
            static uint8_t dummyBank[512] = {0};
            regBank[0] = dummyBank;
        }
        regBank[1] = (uint8_t*)&stack[stackTop]; // frame base
        regBank[2] = regBank[0]; // alias
        regBank[3] = regBank[0]; // alias

        // Push frame
        VMCallFrame frame;
        frame.code = func.bcPtr;
        frame.ip = 0;
        frame.frameBase = stackTop;
        frame.objectIndex = objIndex;
        callStack.push_back(frame);

        // Execute
        uint32_t codeSize = 0;
        if (func.bcSize > 0) {
            codeSize = func.bcSize;
        } else {
            // Estimate: up to 64KB or end of SCT data
            codeSize = std::min((uint32_t)65536,
                               (uint32_t)(sctData.data() + sctData.size() - func.bcPtr));
        }
        bool ok = Execute(func.bcPtr, codeSize);

        callStack.pop_back();
        totalCallsMade++;
        return ok;
    }

    return false;
}

// ============================================================================
// Call bytecode directly by file offset (for STE methods without name hashes)
// ============================================================================

bool KallisVM::CallBytecodeAt(int objIndex, uint32_t fileOffset) {
    if (fileOffset + 12 >= (uint32_t)sctData.size()) return false;

    const uint8_t* bcPtr = sctData.data() + fileOffset + 12; // skip 12-byte header
    uint32_t maxSize = (uint32_t)(sctData.data() + sctData.size() - bcPtr);

    currentObject = objIndex;
    VMObject* obj = GetObject(objIndex);
    if (obj && !obj->fields.empty()) {
        regBank[0] = obj->fields.data();
    } else {
        static uint8_t dummyBank[512] = {0};
        regBank[0] = dummyBank;
    }
    regBank[1] = (uint8_t*)&stack[stackTop];
    regBank[2] = regBank[0];
    regBank[3] = regBank[0];

    VMCallFrame frame;
    frame.code = bcPtr;
    frame.ip = 0;
    frame.frameBase = stackTop;
    frame.objectIndex = objIndex;
    callStack.push_back(frame);

    bool ok = Execute(bcPtr, std::min(maxSize, (uint32_t)65536));

    callStack.pop_back();
    totalCallsMade++;
    return ok;
}

// ============================================================================
// Per-frame tick
// ============================================================================

void KallisVM::Tick(float dt) {
    if (!loaded) return;

    // Push dt onto stack so Tick handlers can access it
    for (int i = 0; i < (int)objects.size(); i++) {
        if (!objects[i].active) continue;
        // Try calling "Tick" on each active object
        // (Only if the class has a Tick handler)
        PushFloat(dt);
        CallFunction(i, "Tick");
    }
}

// ============================================================================
// Bytecode Interpreter
// ============================================================================

bool KallisVM::Execute(const uint8_t* code, uint32_t codeSize) {
    uint32_t ip = 0;
    int maxOps = 100000; // safety limit per call

    while (ip < codeSize && maxOps-- > 0) {
        uint8_t raw = code[ip];
        uint8_t op  = raw >> 2;
        uint8_t bank = raw & 3;
        int size = (op < 64) ? VM_OPCODE_SIZE[op] : 1;

        if (ip + size > codeSize) {
            Log("Execute", "IP past end of code");
            break;
        }

        const uint8_t* args = code + ip + 1;
        totalOpsExecuted++;

        switch (op) {

        // ---- Return ----
        case OP_RET: {
            uint16_t popCount = ReadU16(args);
            uint16_t retCount = ReadU16(args + 2);
            // Pop args, keep return values
            if (stackTop >= (int)popCount) {
                // Save return values
                uint32_t retVals[16];
                int nret = std::min((int)retCount, 16);
                for (int r = 0; r < nret; r++) {
                    retVals[r] = stack[stackTop - retCount + r];
                }
                stackTop -= popCount;
                // Push return values back
                for (int r = 0; r < nret; r++) {
                    if (stackTop < STACK_SIZE) stack[stackTop++] = retVals[r];
                }
            }
            return true;
        }

        // ---- Calls ----
        case OP_CALL_NATIVE: {
            uint16_t funcIdx  = ReadU16(args);
            uint16_t argCount = ReadU16(args + 2);
            DispatchNative(funcIdx, argCount);
            ip += size;
            break;
        }

        case OP_CALL_METHOD: {
            uint16_t funcIdx  = ReadU16(args);
            uint16_t argCount = ReadU16(args + 2);
            DispatchMethod(funcIdx, argCount);
            ip += size;
            break;
        }

        case OP_CALL_SCRIPT: {
            uint16_t funcIdx  = ReadU16(args);
            // uint16_t selfRef  = ReadU16(args + 2);
            // TODO: recursive call to another bytecode function
            // For now, skip
            char buf[64]; snprintf(buf, sizeof(buf), "CALL_SCRIPT func=%d (stub)", funcIdx);
            Log("Execute", buf);
            ip += size;
            break;
        }

        case OP_CALL_STATIC:
        case OP_CALL_VIRT:
        case OP_CALL_OBJ: {
            uint16_t funcIdx  = ReadU16(args);
            uint16_t argCount = ReadU16(args + 2);
            DispatchNative(funcIdx, argCount);
            ip += size;
            break;
        }

        // ---- Stack ops ----
        case OP_POP_N: {
            uint16_t count = ReadU16(args);
            stackTop = std::max(0, stackTop - (int)count);
            ip += size;
            break;
        }

        case OP_PUSH_N: {
            uint16_t count = ReadU16(args);
            stackTop = std::min(STACK_SIZE, stackTop + (int)count);
            ip += size;
            break;
        }

        case OP_PUSH_FRAME: {
            // Push current object reference (as byte offset from bank[0])
            PushInt(0); // simplified: push 0 as "self" reference
            ip += size;
            break;
        }

        case OP_PUSH_IMM: {
            // Engine encoding: value = (b2<<24)|(b3<<16)|(b4<<8)|b1
            // But empirically the files use straight LE u32
            uint32_t imm = ReadU32(args);
            if (stackTop < STACK_SIZE) stack[stackTop++] = imm;
            ip += size;
            break;
        }

        // ---- Memory ops ----
        case OP_LOAD: {
            uint16_t offset = ReadU16(args);
            uint8_t* base = regBank[bank];
            if (base && offset + 4 <= VMObject::FIELD_SIZE) {
                uint32_t val;
                memcpy(&val, base + offset, 4);
                PushInt((int32_t)val);
            } else {
                PushInt(0);
            }
            ip += size;
            break;
        }

        case OP_STORE: {
            uint16_t offset = ReadU16(args);
            uint32_t val = (stackTop > 0) ? stack[--stackTop] : 0;
            uint8_t* base = regBank[bank];
            if (base && offset + 4 <= VMObject::FIELD_SIZE) {
                memcpy(base + offset, &val, 4);
            }
            ip += size;
            break;
        }

        case OP_LOAD_IND: {
            uint16_t offset = ReadU16(args);
            uint32_t dynOff = (stackTop > 0) ? stack[--stackTop] : 0;
            uint8_t* base = regBank[bank];
            uint32_t addr = offset + dynOff;
            if (base && addr + 4 <= VMObject::FIELD_SIZE) {
                uint32_t val;
                memcpy(&val, base + addr, 4);
                PushInt((int32_t)val);
            } else {
                PushInt(0);
            }
            ip += size;
            break;
        }

        case OP_STORE_IND: {
            uint16_t offset = ReadU16(args);
            uint32_t dynOff = (stackTop > 0) ? stack[--stackTop] : 0;
            uint32_t val    = (stackTop > 0) ? stack[--stackTop] : 0;
            uint8_t* base = regBank[bank];
            uint32_t addr = offset + dynOff;
            if (base && addr + 4 <= VMObject::FIELD_SIZE) {
                memcpy(base + addr, &val, 4);
            }
            ip += size;
            break;
        }

        case OP_COPY_TO_STK: {
            uint16_t offset = ReadU16(args);
            uint16_t sz     = ReadU16(args + 2);
            uint8_t* base = regBank[bank];
            int words = (sz + 3) / 4;
            if (base) {
                for (int w = 0; w < words && stackTop < STACK_SIZE; w++) {
                    uint32_t val = 0;
                    if (offset + w * 4 + 4 <= VMObject::FIELD_SIZE)
                        memcpy(&val, base + offset + w * 4, 4);
                    stack[stackTop++] = val;
                }
            }
            ip += size;
            break;
        }

        case OP_COPY_FROM: {
            uint16_t offset = ReadU16(args);
            uint16_t sz     = ReadU16(args + 2);
            uint8_t* base = regBank[bank];
            int words = (sz + 3) / 4;
            stackTop -= words;
            if (stackTop < 0) stackTop = 0;
            if (base) {
                for (int w = 0; w < words; w++) {
                    if (offset + w * 4 + 4 <= VMObject::FIELD_SIZE) {
                        uint32_t val = stack[stackTop + w];
                        memcpy(base + offset + w * 4, &val, 4);
                    }
                }
            }
            ip += size;
            break;
        }

        case OP_LOAD_OFFSET: {
            uint16_t offset = ReadU16(args);
            uint16_t sz     = ReadU16(args + 2);
            uint32_t dynOff = (stackTop > 0) ? stack[--stackTop] : 0;
            uint8_t* base = regBank[bank];
            uint32_t addr = offset + dynOff;
            int words = (sz + 3) / 4;
            if (base) {
                for (int w = 0; w < words && stackTop < STACK_SIZE; w++) {
                    uint32_t val = 0;
                    if (addr + w * 4 + 4 <= VMObject::FIELD_SIZE)
                        memcpy(&val, base + addr + w * 4, 4);
                    stack[stackTop++] = val;
                }
            }
            ip += size;
            break;
        }

        case OP_STORE_OFF: {
            uint16_t offset = ReadU16(args);
            uint16_t sz     = ReadU16(args + 2);
            uint32_t dynOff = (stackTop > 0) ? stack[--stackTop] : 0;
            uint8_t* base = regBank[bank];
            uint32_t addr = offset + dynOff;
            int words = (sz + 3) / 4;
            stackTop -= words;
            if (stackTop < 0) stackTop = 0;
            if (base) {
                for (int w = 0; w < words; w++) {
                    if (addr + w * 4 + 4 <= VMObject::FIELD_SIZE) {
                        uint32_t val = stack[stackTop + w];
                        memcpy(base + addr + w * 4, &val, 4);
                    }
                }
            }
            ip += size;
            break;
        }

        // ---- Integer math ----
        case OP_ADD_I: {
            if (stackTop >= 2) {
                int32_t b = (int32_t)stack[--stackTop];
                int32_t a = (int32_t)stack[stackTop - 1];
                stack[stackTop - 1] = (uint32_t)(a + b);
            }
            ip += size;
            break;
        }
        case OP_SUB_I: {
            if (stackTop >= 2) {
                int32_t b = (int32_t)stack[--stackTop];
                int32_t a = (int32_t)stack[stackTop - 1];
                stack[stackTop - 1] = (uint32_t)(a - b);
            }
            ip += size;
            break;
        }
        case OP_MUL_I: {
            if (stackTop >= 2) {
                int32_t b = (int32_t)stack[--stackTop];
                int32_t a = (int32_t)stack[stackTop - 1];
                stack[stackTop - 1] = (uint32_t)(a * b);
            }
            ip += size;
            break;
        }
        case OP_DIV_I: {
            if (stackTop >= 2) {
                int32_t b = (int32_t)stack[--stackTop];
                int32_t a = (int32_t)stack[stackTop - 1];
                stack[stackTop - 1] = (b != 0) ? (uint32_t)(a / b) : 0;
            }
            ip += size;
            break;
        }
        case OP_MOD_I: {
            if (stackTop >= 2) {
                int32_t b = (int32_t)stack[--stackTop];
                int32_t a = (int32_t)stack[stackTop - 1];
                stack[stackTop - 1] = (b != 0) ? (uint32_t)(a % b) : 0;
            }
            ip += size;
            break;
        }
        case OP_NEG_I: {
            if (stackTop >= 1) stack[stackTop-1] = (uint32_t)(-(int32_t)stack[stackTop-1]);
            ip += size;
            break;
        }
        case OP_INC_I: {
            if (stackTop >= 1) stack[stackTop-1]++;
            ip += size;
            break;
        }
        case OP_DEC_I: {
            if (stackTop >= 1) stack[stackTop-1]--;
            ip += size;
            break;
        }

        // ---- Float math ----
        case OP_ADD_F: {
            if (stackTop >= 2) {
                float b = AsFloat(stack[--stackTop]);
                float a = AsFloat(stack[stackTop - 1]);
                stack[stackTop - 1] = AsUint(a + b);
            }
            ip += size;
            break;
        }
        case OP_SUB_F: {
            if (stackTop >= 2) {
                float b = AsFloat(stack[--stackTop]);
                float a = AsFloat(stack[stackTop - 1]);
                stack[stackTop - 1] = AsUint(a - b);
            }
            ip += size;
            break;
        }
        case OP_MUL_F: {
            if (stackTop >= 2) {
                float b = AsFloat(stack[--stackTop]);
                float a = AsFloat(stack[stackTop - 1]);
                stack[stackTop - 1] = AsUint(a * b);
            }
            ip += size;
            break;
        }
        case OP_DIV_F: {
            if (stackTop >= 2) {
                float b = AsFloat(stack[--stackTop]);
                float a = AsFloat(stack[stackTop - 1]);
                stack[stackTop - 1] = (b != 0.0f) ? AsUint(a / b) : 0;
            }
            ip += size;
            break;
        }
        case OP_NEG_F: {
            if (stackTop >= 1) stack[stackTop-1] = AsUint(-AsFloat(stack[stackTop-1]));
            ip += size;
            break;
        }
        case OP_INC_F: {
            if (stackTop >= 1) stack[stackTop-1] = AsUint(AsFloat(stack[stackTop-1]) + 1.0f);
            ip += size;
            break;
        }
        case OP_DEC_F: {
            if (stackTop >= 1) stack[stackTop-1] = AsUint(AsFloat(stack[stackTop-1]) - 1.0f);
            ip += size;
            break;
        }

        // ---- Logic ----
        case OP_NOT: {
            if (stackTop >= 1) stack[stackTop-1] = (stack[stackTop-1] == 0) ? 1 : 0;
            ip += size;
            break;
        }
        case OP_AND: {
            if (stackTop >= 2) {
                uint32_t b = stack[--stackTop];
                uint32_t a = stack[stackTop - 1];
                stack[stackTop - 1] = (a != 0 && b != 0) ? 1 : 0;
            }
            ip += size;
            break;
        }
        case OP_OR: {
            if (stackTop >= 2) {
                uint32_t b = stack[--stackTop];
                uint32_t a = stack[stackTop - 1];
                stack[stackTop - 1] = (a != 0 || b != 0) ? 1 : 0;
            }
            ip += size;
            break;
        }

        // ---- Integer comparisons ----
        case OP_NEQ_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a!=b)?1:0; }
            ip += size; break;
        }
        case OP_EQ_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a==b)?1:0; }
            ip += size; break;
        }
        case OP_LT_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a<b)?1:0; }
            ip += size; break;
        }
        case OP_LE_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a<=b)?1:0; }
            ip += size; break;
        }
        case OP_GT_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a>b)?1:0; }
            ip += size; break;
        }
        case OP_GE_I: {
            if (stackTop >= 2) { int32_t b=(int32_t)stack[--stackTop]; int32_t a=(int32_t)stack[stackTop-1]; stack[stackTop-1]=(a>=b)?1:0; }
            ip += size; break;
        }

        // ---- Float comparisons ----
        case OP_NEQ_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a!=b)?1:0; }
            ip += size; break;
        }
        case OP_EQ_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a==b)?1:0; }
            ip += size; break;
        }
        case OP_LT_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a<b)?1:0; }
            ip += size; break;
        }
        case OP_LE_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a<=b)?1:0; }
            ip += size; break;
        }
        case OP_GT_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a>b)?1:0; }
            ip += size; break;
        }
        case OP_GE_F: {
            if (stackTop >= 2) { float b=AsFloat(stack[--stackTop]); float a=AsFloat(stack[stackTop-1]); stack[stackTop-1]=(a>=b)?1:0; }
            ip += size; break;
        }

        // ---- Branches ----
        case OP_JNZ: {
            int16_t offset = ReadS16(args);
            uint32_t val = (stackTop > 0) ? stack[--stackTop] : 0;
            if (val != 0) {
                ip = (uint32_t)((int32_t)ip + offset);
            } else {
                ip += size;
            }
            break;
        }
        case OP_JZ: {
            int16_t offset = ReadS16(args);
            uint32_t val = (stackTop > 0) ? stack[--stackTop] : 0;
            if (val == 0) {
                ip = (uint32_t)((int32_t)ip + offset);
            } else {
                ip += size;
            }
            break;
        }
        case OP_JMP: {
            int16_t offset = ReadS16(args);
            ip = (uint32_t)((int32_t)ip + offset);
            break;
        }

        // ---- Conversions ----
        case OP_F2I: {
            if (stackTop >= 1) {
                float f = AsFloat(stack[stackTop-1]);
                stack[stackTop-1] = (uint32_t)(int32_t)f;
            }
            ip += size;
            break;
        }
        case OP_I2F: {
            if (stackTop >= 1) {
                int32_t i = (int32_t)stack[stackTop-1];
                stack[stackTop-1] = AsUint((float)i);
            }
            ip += size;
            break;
        }

        // ---- Address calc ----
        case OP_ADDR_CALC: {
            uint16_t offset = ReadU16(args);
            if (stackTop >= 1) {
                // stack[-1] += (regBank[bank] + offset - regBank[0])
                // Simplified: just add the offset
                stack[stackTop - 1] += offset;
            }
            ip += size;
            break;
        }

        // ---- Misc ----
        case OP_SET_EXCEPT: {
            // Store type info (ignore in our simulation)
            ip += size;
            break;
        }
        case OP_SAVE_STATE: {
            // Coroutine save — for now, just return (yields execution)
            Log("Execute", "SAVE_STATE (coroutine yield)");
            return true;
        }

        // ---- Debug / NOP ----
        case OP_SET_DEBUG:
        case OP_NOP5A:
        case OP_NOP5B:
        case OP_TRACE:
            ip += size;
            break;
        case OP_NOP1:
        case OP_BREAKPOINT:
        case OP_DEFAULT:
            ip += size;
            break;

        default:
            // Unknown opcode — skip by size
            ip += size;
            break;
        }
    }

    if (maxOps <= 0) {
        Log("Execute", "Hit execution limit (infinite loop?)");
    }
    return true;
}

// ============================================================================
// Native function dispatch
// ============================================================================

void KallisVM::DispatchNative(uint32_t funcIdx, uint32_t argCount) {
    // Look up in function table
    if (funcIdx < (uint32_t)functions.size()) {
        VMFunction& func = functions[funcIdx];

        // Check for registered override
        auto ovr = nativeOverrides.find(func.nameHash);
        if (ovr != nativeOverrides.end()) {
            ovr->second(*this);
            return;
        }

        if (func.nativeFunc) {
            func.nativeFunc(*this);
            return;
        }

        // Unknown native — just pop args and push 0
        char buf[128];
        snprintf(buf, sizeof(buf), "Unhandled native func=%d hash=%08X args=%d '%s'",
                 funcIdx, func.nameHash, argCount, func.name.c_str());
        Log("DispatchNative", buf);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Native func index %d out of range (%d)", funcIdx, (int)functions.size());
        Log("DispatchNative", buf);
    }

    // Pop args to keep stack balanced
    for (uint32_t i = 0; i < argCount; i++) {
        if (stackTop > 0) stackTop--;
    }
}

void KallisVM::DispatchMethod(uint32_t funcIdx, uint32_t argCount) {
    // Method index may point to a triplet. The hash is at index aligned to triplet[0].
    // Try: funcIdx itself, funcIdx-1 (if we're at the bcOffset slot), funcIdx-2
    for (int delta = 0; delta >= -2; delta--) {
        int tryIdx = (int)funcIdx + delta;
        if (tryIdx < 0 || tryIdx >= (int)functions.size()) continue;
        VMFunction& f = functions[tryIdx];
        if (f.nameHash == 0) continue;

        // Check registered native override
        auto ovr = nativeOverrides.find(f.nameHash);
        if (ovr != nativeOverrides.end()) {
            ovr->second(*this);
            totalCallsMade++;
            return;
        }

        // Check for bytecode at funcIdx+1 (triplet[1] = bcOffset)
        int bcIdx = tryIdx + 1;
        if (bcIdx < (int)functions.size() && functions[bcIdx].type == VMFuncType::Bytecode && functions[bcIdx].bcPtr) {
            // Execute bytecode method
            int savedObj = currentObject;
            VMCallFrame frame;
            frame.code = functions[bcIdx].bcPtr;
            frame.ip = 0;
            frame.frameBase = stackTop;
            frame.objectIndex = currentObject;
            callStack.push_back(frame);
            uint32_t maxSz = (uint32_t)(sctData.data() + sctData.size() - functions[bcIdx].bcPtr);
            Execute(functions[bcIdx].bcPtr, std::min(maxSz, (uint32_t)65536));
            callStack.pop_back();
            currentObject = savedObj;
            totalCallsMade++;
            return;
        }

        // Found a hash but no handler — log it
        char buf[128];
        snprintf(buf, sizeof(buf), "method[%d] hash=%08X '%s' args=%d (no handler)",
                 funcIdx, f.nameHash, f.name.c_str(), argCount);
        Log("Method", buf);
        break;
    }

    // Pop args to keep stack balanced
    for (uint32_t i = 0; i < argCount; i++)
        if (stackTop > 0) stackTop--;
}

// ============================================================================
// Native function registration
// ============================================================================

void KallisVM::RegisterNative(const char* name, VMNativeFunc func) {
    RegisterNativeByHash(HashName(name), func);
}

void KallisVM::RegisterNativeByHash(uint32_t hash, VMNativeFunc func) {
    nativeOverrides[hash] = func;
}

// ============================================================================
// Built-in native function stubs
// ============================================================================

void KallisVM::RegisterBuiltinNatives() {
    // sauPrint — log a string
    RegisterNative("sauPrint", [this](KallisVM& vm) {
        // Pop string pointer (in our VM this is just an int)
        int32_t val = vm.PopInt();
        char buf[64]; snprintf(buf, sizeof(buf), "sauPrint(%d)", val);
        vm.Log("sauPrint", buf);
    });

    // sauPrintInt
    RegisterNative("sauPrintInt", [this](KallisVM& vm) {
        int32_t val = vm.PopInt();
        char buf[64]; snprintf(buf, sizeof(buf), "sauPrintInt(%d)", val);
        vm.Log("sauPrintInt", buf);
    });

    // sauPrintFloat
    RegisterNative("sauPrintFloat", [this](KallisVM& vm) {
        float val = vm.PopFloat();
        char buf[64]; snprintf(buf, sizeof(buf), "sauPrintFloat(%g)", val);
        vm.Log("sauPrintFloat", buf);
    });

    // sauSetObjPosition — sets position on current object
    RegisterNative("sauSetObjPosition", [this](KallisVM& vm) {
        float pos[3];
        vm.PopVec3(pos);
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) {
            obj->position[0] = pos[0];
            obj->position[1] = pos[1];
            obj->position[2] = pos[2];
            char buf[128]; snprintf(buf, sizeof(buf), "pos=(%.1f, %.1f, %.1f) obj='%s'",
                pos[0], pos[1], pos[2], obj->name.c_str());
            vm.Log("sauSetObjPosition", buf);
        }
    });

    // sauSetObjRotation — sets rotation on current object
    RegisterNative("sauSetObjRotation", [this](KallisVM& vm) {
        float rot[3];
        vm.PopVec3(rot);
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) {
            obj->rotation[0] = rot[0];
            obj->rotation[1] = rot[1];
            obj->rotation[2] = rot[2];
            char buf[128]; snprintf(buf, sizeof(buf), "rot=(%.1f, %.1f, %.1f) obj='%s'",
                rot[0], rot[1], rot[2], obj->name.c_str());
            vm.Log("sauSetObjRotation", buf);
        }
    });

    // sauSetPlayerPosition
    RegisterNative("sauSetPlayerPosition", [this](KallisVM& vm) {
        float pos[3]; vm.PopVec3(pos);
        char buf[64]; snprintf(buf, sizeof(buf), "player pos=(%.1f, %.1f, %.1f)", pos[0], pos[1], pos[2]);
        vm.Log("sauSetPlayerPosition", buf);
    });

    // sauActivateObj
    RegisterNative("sauActivateObj", [this](KallisVM& vm) {
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) obj->active = true;
        vm.Log("sauActivateObj", "");
    });

    // sauDeactivateObj
    RegisterNative("sauDeactivateObj", [this](KallisVM& vm) {
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) obj->active = false;
        vm.Log("sauDeactivateObj", "");
    });

    // sauSetObjSector
    RegisterNative("sauSetObjSector", [this](KallisVM& vm) {
        int32_t sector = vm.PopInt();
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) obj->sectorId = sector;
    });

    // sauGetObjPosition
    RegisterNative("sauGetObjPosition", [this](KallisVM& vm) {
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) {
            vm.PushVec3(obj->position);
        } else {
            float zero[3] = {0,0,0};
            vm.PushVec3(zero);
        }
    });

    // sauGetObjRotation
    RegisterNative("sauGetObjRotation", [this](KallisVM& vm) {
        uint32_t objRef = vm.PopObjRef();
        (void)objRef;
        VMObject* obj = vm.GetObject(vm.currentObject);
        if (obj) {
            vm.PushVec3(obj->rotation);
        } else {
            float zero[3] = {0,0,0};
            vm.PushVec3(zero);
        }
    });

    // sauRandRange / sauRandRangeInt
    RegisterNative("sauRandRange", [](KallisVM& vm) {
        float hi = vm.PopFloat();
        float lo = vm.PopFloat();
        float r = lo + ((float)rand() / RAND_MAX) * (hi - lo);
        vm.PushFloat(r);
    });

    RegisterNative("sauRandRangeInt", [](KallisVM& vm) {
        int32_t hi = vm.PopInt();
        int32_t lo = vm.PopInt();
        int32_t r = lo + (rand() % std::max(1, hi - lo + 1));
        vm.PushInt(r);
    });

    // sauSin / sauCos / sauTan / sauATan2
    RegisterNative("sauSin", [](KallisVM& vm) {
        float a = vm.PopFloat(); vm.PushFloat(sinf(a));
    });
    RegisterNative("sauCos", [](KallisVM& vm) {
        float a = vm.PopFloat(); vm.PushFloat(cosf(a));
    });
    RegisterNative("sauTan", [](KallisVM& vm) {
        float a = vm.PopFloat(); vm.PushFloat(tanf(a));
    });
    RegisterNative("sauATan2", [](KallisVM& vm) {
        float x = vm.PopFloat();
        float y = vm.PopFloat();
        vm.PushFloat(atan2f(y, x));
    });

    // sauIsVisible — always return true for now
    RegisterNative("sauIsVisible", [](KallisVM& vm) {
        vm.PopObjRef();
        vm.PushBool(true);
    });

    // sauIsHudDisabled — return false
    RegisterNative("sauIsHudDisabled", [](KallisVM& vm) {
        vm.PushBool(false);
    });

    // sauGetHash — compute name hash
    RegisterNative("sauGetHash", [](KallisVM& vm) {
        // In engine this hashes a string; we just return what's on stack
        uint32_t val = vm.PopObjRef();
        vm.PushInt((int32_t)val);
    });

    // sauCharacterInit — capture texture associations (engine replica)
    // Engine pops: TEX_B name, TEX_A name, template name (all as nameHashes)
    // Then hashes the resolved strings → PCIM lookup keys (same hashes)
    RegisterNative("sauCharacterInit", [](KallisVM& vm) {
        uint32_t texBHash     = vm.PopObjRef();
        uint32_t texAHash     = vm.PopObjRef();
        uint32_t templateHash = vm.PopObjRef();

        CharTexAssoc assoc;
        assoc.templateHash = templateHash;
        assoc.texAHash     = texAHash;
        assoc.texBHash     = texBHash;
        auto itT = vm.nameResolve.find(templateHash);
        auto itA = vm.nameResolve.find(texAHash);
        auto itB = vm.nameResolve.find(texBHash);
        if (itT != vm.nameResolve.end()) assoc.templateName = itT->second;
        if (itA != vm.nameResolve.end()) assoc.texAName = itA->second;
        if (itB != vm.nameResolve.end()) assoc.texBName = itB->second;
        vm.charTextures.push_back(assoc);

        char buf[256];
        snprintf(buf, sizeof(buf), "template='%s' texA='%s' texB='%s'",
                 assoc.templateName.c_str(), assoc.texAName.c_str(), assoc.texBName.c_str());
        vm.Log("sauCharacterInit", buf);
    });

    // sauCreateCharacter / sauSpawnObj / sauCreatePickup — stub
    RegisterNative("sauCreateCharacter", [](KallisVM& vm) {
        vm.PopObjRef(); // name hash
        vm.PushInt(0);  // return null object
        vm.Log("sauCreateCharacter", "(stub)");
    });

    RegisterNative("sauSpawnObj", [](KallisVM& vm) {
        vm.Log("sauSpawnObj", "(stub)");
    });

    RegisterNative("sauCreatePickup", [](KallisVM& vm) {
        vm.Log("sauCreatePickup", "(stub)");
    });

    // sauSendTrigger / sauSendEvent — stub
    RegisterNative("sauSendTrigger", [](KallisVM& vm) {
        vm.PopObjRef(); // trigger name/hash
        vm.Log("sauSendTrigger", "(stub)");
    });

    RegisterNative("sauSendEvent", [](KallisVM& vm) {
        vm.PopObjRef();
        vm.Log("sauSendEvent", "(stub)");
    });

    // sauHideSector / sauIsSectorLoaded
    RegisterNative("sauHideSector", [](KallisVM& vm) {
        vm.PopInt(); // sector id
        vm.Log("sauHideSector", "(stub)");
    });

    RegisterNative("sauIsSectorLoaded", [](KallisVM& vm) {
        vm.PopInt();
        vm.PushBool(true);
    });

    // sauTransitionToLevel / sauLevelReset — stub
    RegisterNative("sauTransitionToLevel", [](KallisVM& vm) {
        vm.Log("sauTransitionToLevel", "(stub)");
    });

    RegisterNative("sauLevelReset", [](KallisVM& vm) {
        vm.Log("sauLevelReset", "(stub)");
    });

    // sauRemoveWhenNotVisible — stub
    RegisterNative("sauRemoveWhenNotVisible", [](KallisVM& vm) {
        vm.PopObjRef();
    });

    printf("[VM] Registered %d built-in native stubs\n", (int)nativeOverrides.size());
}
