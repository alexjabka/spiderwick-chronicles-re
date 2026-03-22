#pragma once
// ============================================================================
// vm.h — Kallis VM: SCT script bytecode interpreter for SpiderView
// ============================================================================
//
// Implements the Stormfront Studios "Kallis" scripting VM as reversed from
// the Spiderwick Chronicles (2008) engine binary. Executes SCT v13 bytecode
// to drive prop placement, animation, effects (falling leaves, fog, etc).
//
// Architecture:
//   Stack-based interpreter with 64 opcodes, 4 register banks.
//   Instructions: 1/3/5 bytes. Opcode byte: bits[7:2]=op, bits[1:0]=bank.
//   Two stacks: eval stack (grows up), call frame stack (linked list).
//   Native functions registered by name hash, called from bytecode.
//
// Reference:
//   VMInterpreter @ 0x52D9C0, VMLoadScript_EAC @ 0x52C2A0
//   VM_INTERPRETER.md, VM_BYTECODE_FORMAT.md, KALLIS_VM.md

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// ============================================================================
// SCT file format structures
// ============================================================================

struct SCTHeader {
    char     magic[4];           // "SCT\0"
    uint32_t version;            // 13
    uint32_t dataSize;           // file data size
    uint32_t ntvOffset;          // → NTV string/function table
    uint32_t funcTableOffset;    // → function table (same as NTV)
    uint32_t vtlOffset;          // → VTL class table
    uint32_t stringResolveOff;   // → string resolve table
    uint32_t typeListOffset;     // → type list
    uint32_t saveDataOffset;     // → save state area
    uint32_t reserved;
    uint32_t saveStateSize;
    uint16_t flags;
    uint16_t padding;
};

// ============================================================================
// VM function entry — bytecode or native
// ============================================================================

enum class VMFuncType { None, Bytecode, Native };

// Native function signature: receives VM pointer for stack access
class KallisVM;
using VMNativeFunc = std::function<void(KallisVM& vm)>;

struct VMFunction {
    VMFuncType  type = VMFuncType::None;
    uint32_t    nameHash = 0;
    std::string name;              // human-readable (from string resolve)

    // Bytecode function
    uint32_t    bcOffset = 0;      // file offset to 12-byte header
    const uint8_t* bcPtr = nullptr; // pointer to instruction start (header+12)
    uint32_t    bcSize = 0;        // estimated bytecode length

    // Native function
    VMNativeFunc nativeFunc;
};

// ============================================================================
// VM class definition (from VTL)
// ============================================================================

struct VMClassDef {
    uint32_t    nameHash = 0;
    uint32_t    parentHash = 0;
    std::string name;

    // State table entries: each STE has a list of method bytecode offsets
    struct STE {
        uint32_t nameHash = 0;
        std::vector<uint32_t> methodOffsets; // file offsets to bytecode
    };
    std::vector<STE> states;

    // Per-class NTV: method name hashes for this class
    // Triplets: (hash, bcOffset, nativeSlot)
    struct MethodEntry {
        uint32_t nameHash = 0;
        uint32_t bcOffset = 0;    // 0 = no bytecode
        bool     isNative = false; // true if 0xFFFFFFFF slot
    };
    std::vector<MethodEntry> methods;
};

// ============================================================================
// VM object instance — runtime object linked to scene
// ============================================================================

struct VMObject {
    int         classIndex = -1;   // → KallisVM::classes
    std::string name;
    uint32_t    nameHash = 0;

    // Object fields (variable-size memory block, like engine's object+offset)
    std::vector<uint8_t> fields;
    static constexpr int FIELD_SIZE = 4096; // large enough for engine object fields

    // Link to scene object
    int         sceneObjectIndex = -1;

    // State
    bool        active = true;
    float       position[3] = {0, 0, 0};
    float       rotation[3] = {0, 0, 0};
    int         sectorId = 0;
};

// ============================================================================
// VM execution state (per call frame)
// ============================================================================

struct VMCallFrame {
    const uint8_t* code = nullptr; // bytecode base pointer
    uint32_t  ip = 0;             // instruction pointer (offset from code)
    uint32_t  frameBase = 0;      // eval stack index where frame starts
    int       objectIndex = -1;   // VMObject this frame operates on
};

// ============================================================================
// Opcode definitions
// ============================================================================

enum VMOpcode : uint8_t {
    OP_RET          = 0x00,
    OP_CALL_SCRIPT  = 0x01,
    OP_CALL_NATIVE  = 0x02,
    OP_CALL_METHOD  = 0x03,
    OP_CALL_STATIC  = 0x04,
    OP_CALL_VIRT    = 0x05,
    OP_POP_N        = 0x06,
    OP_PUSH_N       = 0x07,
    OP_PUSH_FRAME   = 0x08,
    OP_SAVE_STATE   = 0x09,
    OP_COPY_TO_STK  = 0x0A,
    OP_COPY_FROM    = 0x0B,
    OP_PUSH_IMM     = 0x0C,
    OP_LOAD_OFFSET  = 0x0D,
    OP_STORE_OFF    = 0x0E,
    OP_ADD_I        = 0x0F,
    OP_SUB_I        = 0x10,
    OP_MUL_I        = 0x11,
    OP_DIV_I        = 0x12,
    OP_MOD_I        = 0x13,
    OP_NEG_I        = 0x14,
    OP_INC_I        = 0x15,
    OP_DEC_I        = 0x16,
    OP_ADD_F        = 0x17,
    OP_SUB_F        = 0x18,
    OP_MUL_F        = 0x19,
    OP_DIV_F        = 0x1A,
    OP_DEFAULT      = 0x1B, // NOP
    OP_NEG_F        = 0x1C,
    OP_INC_F        = 0x1D,
    OP_DEC_F        = 0x1E,
    OP_NOT          = 0x1F,
    OP_AND          = 0x20,
    OP_OR           = 0x21,
    OP_NEQ_I        = 0x22,
    OP_EQ_I         = 0x23,
    OP_LT_I         = 0x24,
    OP_LE_I         = 0x25,
    OP_GT_I         = 0x26,
    OP_GE_I         = 0x27,
    OP_NEQ_F        = 0x28,
    OP_EQ_F         = 0x29,
    OP_LT_F         = 0x2A,
    OP_LE_F         = 0x2B,
    OP_GT_F         = 0x2C,
    OP_GE_F         = 0x2D,
    OP_JNZ          = 0x2E,
    OP_JZ           = 0x2F,
    OP_JMP          = 0x30,
    OP_SET_EXCEPT   = 0x31,
    OP_F2I          = 0x32,
    OP_I2F          = 0x33,
    OP_ADDR_CALC    = 0x34,
    OP_SET_DEBUG    = 0x35,
    OP_NOP5A        = 0x36,
    OP_NOP1         = 0x37,
    OP_BREAKPOINT   = 0x38,
    OP_TRACE        = 0x39,
    OP_LOAD         = 0x3A,
    OP_STORE        = 0x3B,
    OP_LOAD_IND     = 0x3C,
    OP_STORE_IND    = 0x3D,
    OP_CALL_OBJ     = 0x3E,
    OP_NOP5B        = 0x3F,
};

// Instruction sizes indexed by opcode
static const int VM_OPCODE_SIZE[64] = {
    5,5,5,5,5,5,3,3, // 00-07: RET,CALLS,POP_N,PUSH_N
    1,5,5,5,5,5,5,1, // 08-0F: PUSH_FRAME,SAVE_STATE,COPY,PUSH_IMM,LOAD/STORE_OFF,ADD_I
    1,1,1,1,1,1,1,1, // 10-17: SUB_I..ADD_F
    1,1,1,1,1,1,1,1, // 18-1F: SUB_F..NOT
    1,1,1,1,1,1,1,1, // 20-27: AND..GE_I
    1,1,1,1,1,1,3,3, // 28-2F: NEQ_F..JZ
    3,3,1,1,3,5,5,1, // 30-37: JMP,SET_EXCEPT,F2I,I2F,ADDR_CALC,SET_DEBUG,NOP5A,NOP1
    1,5,3,3,3,3,5,5, // 38-3F: BREAKPOINT,TRACE,LOAD,STORE,LOAD_IND,STORE_IND,CALL_OBJ,NOP5B
};

// ============================================================================
// KallisVM — top-level VM that owns all state
// ============================================================================

class KallisVM {
public:
    // --- Loading ---
    bool LoadSCT(const char* path);
    bool LoadSCTFromBuffer(const uint8_t* data, uint32_t size);
    bool LoadSCTFromZWD(const char* zwdPath); // scan decompressed ZWD for embedded SCT

    // --- Execution ---
    bool CallFunction(int objIndex, const char* funcName);
    bool CallFunctionByHash(int objIndex, uint32_t nameHash);
    bool CallBytecodeAt(int objIndex, uint32_t fileOffset); // direct execute by SCT offset
    void Tick(float dt);

    // --- Object management ---
    int  CreateObject(const char* className, const char* objName);
    VMObject* GetObject(int index);

    // --- Stack access (for native functions) ---
    int32_t  PopInt();
    float    PopFloat();
    void     PopVec3(float out[3]);
    uint32_t PopObjRef();
    void     PushInt(int32_t v);
    void     PushFloat(float v);
    void     PushVec3(const float v[3]);
    void     PushBool(bool v);

    // --- Native function registration ---
    void RegisterNative(const char* name, VMNativeFunc func);
    void RegisterNativeByHash(uint32_t hash, VMNativeFunc func);

    // --- Scene link ---
    void* scenePtr = nullptr; // Scene* for native stubs to access

    // --- State ---
    bool loaded = false;
    std::vector<VMFunction>  functions;
    std::vector<VMClassDef>  classes;
    std::vector<VMObject>    objects;

    // String resolve: nameHash → human name
    std::unordered_map<uint32_t, std::string> nameResolve;

    // Character texture associations captured from sauCharacterInit execution
    struct CharTexAssoc {
        uint32_t templateHash = 0;
        uint32_t texAHash = 0;   // PCIM nameHash for diffuse
        uint32_t texBHash = 0;   // PCIM nameHash for alternate
        std::string templateName, texAName, texBName;
    };
    std::vector<CharTexAssoc> charTextures;

    // Execution log (for debugging)
    struct LogEntry {
        std::string funcName;
        std::string detail;
    };
    std::vector<LogEntry> execLog;
    int maxLogEntries = 200;
    int totalOpsExecuted = 0;
    int totalCallsMade = 0;

    // Name hash function (engine's algorithm)
    static uint32_t HashName(const char* name);

    // Logging (public for external callers)
    void Log(const char* func, const char* detail);

private:
    // --- SCT data ---
    std::vector<uint8_t> sctData;

    // --- Eval stack ---
    static constexpr int STACK_SIZE = 16384;
    uint32_t stack[STACK_SIZE] = {0};
    int      stackTop = 0;   // index of next free slot

    // --- Register banks ---
    // Bank 0 = global/object base, Bank 1 = frame, Bank 2/3 = extra
    uint8_t* regBank[4] = {nullptr, nullptr, nullptr, nullptr};

    // --- Call stack ---
    std::vector<VMCallFrame> callStack;
    int currentObject = -1;

    // --- Function lookup ---
    std::unordered_map<uint32_t, int> funcByHash;    // nameHash → functions[] index
    std::unordered_map<uint32_t, int> classByHash;   // nameHash → classes[] index

    // Registered native overrides (by hash)
    std::unordered_map<uint32_t, VMNativeFunc> nativeOverrides;

    // --- Parsing ---
    void ParseNTV(const uint8_t* base, uint32_t offset);
    void ParseVTL(const uint8_t* base, uint32_t offset);
    void ParseStringResolve(const uint8_t* base, uint32_t offset, uint32_t fileSize);
    void ParseClassEntry(const uint8_t* base, uint32_t offset, uint32_t nextOffset);

    // --- Interpreter ---
    bool Execute(const uint8_t* code, uint32_t codeSize);
    void DispatchNative(uint32_t funcIdx, uint32_t argCount);
    void DispatchMethod(uint32_t funcIdx, uint32_t argCount);

    // --- Helpers ---
    uint16_t ReadU16(const uint8_t* p) const { return p[0] | (p[1] << 8); }
    int16_t  ReadS16(const uint8_t* p) const { return (int16_t)(p[0] | (p[1] << 8)); }
    uint32_t ReadU32(const uint8_t* p) const { return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }

    float AsFloat(uint32_t v) const {
        float f; memcpy(&f, &v, 4); return f;
    }
    uint32_t AsUint(float f) const {
        uint32_t v; memcpy(&v, &f, 4); return v;
    }

    // Built-in native stubs
    void RegisterBuiltinNatives();
};
