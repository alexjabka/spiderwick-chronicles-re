# DBDB -- Binary Database Format

**Status:** Fully reversed from IDA analysis of `ClDatabaseAsset` class and binary verification of all 28 DBDB files.

---

## Header (0x20 bytes)

```c
struct DBDBHeader {
    char     magic[4];           // +0x00: "DBDB" (0x42444244 LE)
    uint32_t version;            // +0x04: always 4 in shipped game
    uint32_t totalSize;          // +0x08: size of DBDB data (excluding trailing 0xFF padding)
    uint32_t recordCount;        // +0x0C: number of records
    uint32_t recordDataOffset;   // +0x10: offset to record data (always 0x20 = header size)
    uint32_t stringRefCount;     // +0x14: number of field values that reference the data blob
    uint32_t dataBlobOffset;     // +0x18: offset to data blob (strings + structured data)
    uint32_t reserved;           // +0x1C: always 0
};
```

**Key relationships:**
- `recordDataOffset` is always `0x20` (records immediately follow header)
- `dataBlobOffset` marks where record data ends and the data blob begins
- `totalSize` may be less than file size; trailing bytes are `0xFF` array sentinel padding
- Engine reads record count from `buffer + 0x0C` (`sub_424A20` at `0x424A20`)
- Engine reads record data at `buffer + *(buffer + 0x10)` (loader functions)

## Records

Variable-length records are packed sequentially starting at `recordDataOffset` (0x20):

```c
struct DBDBRecord {
    uint32_t fieldCount;                    // number of fields in this record
    struct { uint32_t hash; uint32_t value; } fields[fieldCount];
};
// Next record follows immediately (no alignment padding)
```

- **fieldCount** varies per record (not all records have the same fields)
- **hash**: field name hash computed by `HashString()` (see below)
- **value**: interpreted by type:
  - If `value >= dataBlobOffset && value < totalSize` -> offset into data blob
  - Otherwise -> IEEE 754 float (cast to int/bool by engine as needed)

**Field lookup** (`sub_428030` at `0x428030`, Kallis VM): linear scan through `fields[]` comparing hashes.
Fields are NOT sorted; order varies between records.

## Field Value Types

The engine uses typed accessor functions that all call `sub_428030` (FindField) internally:

| Function | Address | Returns | Cast |
|----------|---------|---------|------|
| `DBDB_GetString` | `0x4281F0` | `buffer + *(field+4)` | string pointer |
| `DBDB_GetInt` | `0x4280C0` | `(int)*(float*)(field+4)` | float-to-int |
| `DBDB_GetFloat` | `0x428130` | `*(float*)(field+4)` | direct float |
| `DBDB_GetBool` | `0x428160` | `*(DWORD*)(field+4) != 0` | nonzero test |
| `DBDB_FindField` | `0x428030` | pointer to `{hash,value}` pair | raw lookup |

**String values**: value is an absolute offset from the start of the DBDB buffer; the engine returns `buffer_base + value`.

**Array values** (structured data in blob): some fields reference inline array structures in the data blob:
```c
struct InlineArray {
    uint32_t count;
    uint32_t ptr;       // -1 (sentinel) on disk; patched at load time to &data[0]
    uint32_t data[count]; // may contain offsets that get rebased
};
```
Array fixup function: `sub_428090` at `0x428090` -- replaces sentinel with pointer to inline data
and adds base offset to each element.

## Data Blob

Located at `dataBlobOffset` through `totalSize`. Contains:
1. **Null-terminated ASCII strings** -- referenced by field values as offsets from buffer start
2. **Inline arrays** -- structured data with count + sentinel + elements
3. **Mixed data** -- floats, sub-structures referenced by array fields

After `totalSize`, files may contain `0xFF` padding bytes (uninitialized array pointer space).

## HashString Algorithm

```c
uint32_t HashString(const char* s) {
    uint32_t hash = 0;
    for (const char* p = s; *p; p++) {
        uint8_t c = (uint8_t)*p;
        hash = (hash + c + ((hash << (c & 7)) & 0xFFFFFFFF)) & 0xFFFFFFFF;
    }
    return hash;
}
```

Found at `0x4318D0` (ClDatabaseAsset constructor hashes "Database").
350+ field names recovered from IDA strings at `0x6235BC-0x6275A0`.

## Engine Class Hierarchy

- **`ClDatabaseAsset`** (vtable `0x627638`, RTTI `.?AVClDatabaseAsset@@`)
  - Constructor: `0x431880` -- fields: `{vtable, bufferPtr, param}`
  - Vtable[0] `0x431820`: Validate (returns true)
  - Vtable[1] `0x431830`: Load (calls `sub_428000` via Kallis VM)
  - Vtable[2] `0x431860`: IsLoaded (returns true)
  - Vtable[3] `0x431870`: Unload (calls `sub_427F50`)

- **DB Schema Descriptors** (in `.rdata`): `{loadFunc, cleanupFunc, "DBName\0", "COL1\0", "COL2\0", ...}`
  - Each registered DB has a schema at a fixed `.rdata` address
  - Registration at init time: `sub_427FA0` + vtable/schema pointer assignment

- **`sub_428000`** (`0x428000`): DBDB loader (Kallis VM thunk via `off_1C89428`)
  - Validates magic "DBDB" and version
  - Stores buffer pointer in DB object's `field_4`
  - Error strings (unreferenced in code, likely debug-only):
    - `"Incorrect Database Version. Got %d, expected &d"` at `0x62440C`
    - `"{rError:} Invalid database magic. Expected {g%s}, found {g%.4s}"` at `0x624480`

## Registered Databases (28 total)

| DB Name | Schema Addr | Loader | Global | Record Count |
|---------|-------------|--------|--------|--------------|
| AdaptiveMusicDB | `0x624E00` | `0x424E00` | -- | 1 |
| AttackDB | -- | -- | -- | 59 |
| AudioDuckingDB | -- | -- | -- | 7 |
| AudioHookDB | -- | -- | -- | 29 |
| AutoTraverseDB | -- | -- | -- | 17 |
| CameraDB | -- | `0x426450` | -- | 20 |
| CharacterMoveDB | -- | -- | -- | 43 |
| ConfigDB | -- | -- | -- | 18 |
| ConversationDB | `0x624308` | `0x4278D0` | `0x72F100` | 216 |
| DifficultyDB | `0x6244D4` | `0x428320` | `0x72F420` | 4 |
| EnemyDB | `0x624544` | `0x428620` | `0x72F438` | 42 |
| FieldGuideDB | `0x625220` | `0x429880` | `0x72F44C` | 34 |
| GuideLayoutDB | `0x625358` | `0x42A300` | `0x72F490` | 12 |
| ItemDB | `0x6253B4` | `0x42ACF0` | `0x72F4A8` | 82 |
| LevelDB | `0x625608` | `0x42B580` | `0x72F4DC` | 22 |
| LinkedAnimDB | `0x625688` | `0x42BFB0` | `0x72F4F0` | 10 |
| PickupDB | `0x625850` | `0x42C760` | `0x72F504` | 6 |
| ProjectileDB | vtable | -- | `0x72F514` | 13 |
| PropDB | `0x626044` | `0x42D400` | `0x72F528` | 10 |
| QuestDB | `0x626218` | `0x42D8B0` | `0x72F53C` | 60 |
| SoundBankDB | vtable | -- | `0x72F550` | 150 |
| SpriteCaptureDB | `0x6263A4` | `0x42E620` | `0x72F568` | 11 |
| SpriteDB | `0x6263F4` | `0x42EA30` | `0x72F578` | 13 |
| SpriteMotionDB | vtable | -- | `0x72F588` | 18 |
| SpritePowerDB | vtable | -- | `0x72F598` | 9 |
| TaskDB | `0x6269A0` | `0x42F9D0` | `0x72F5A8` | 148 |
| TextStyleDB | `0x626C34` | `0x430250` | `0x72F5C4` | -- |
| VoiceLogicDB | `0x626CB4` | `0x4305F0` | `0x72F5D8` | 98 |
| WeaponDB | `0x626DAC` | `0x430A10` | `0x72F5EC` | 16 |

DB registration init code: `0x6149D0-0x614E7A`.

## Tool

`spiderwick_unpack.py` auto-converts DBDB to JSON on extraction.
