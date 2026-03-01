# System Architecture Diagrams

## Hardware Architecture

### Physical Setup

```mermaid
graph TB
    subgraph Teddy Bear
        MCU[MCU Microcontroller]
        FLASH[Flash Memory<br/>Encrypted Data]
    end
    
    subgraph FPGA Board - SPI Pass-Through with Monitoring
        FPGA[FPGA<br/>Altera/Avalon SPI Core]
        MOSI_PT[MOSI Pass-Through]
        MISO_MOD[MISO Modifier<br/>Patch Injection]
        BUFFER[Transaction Buffer]
    end
    
    subgraph Raspberry Pi 3
        SPI_MASTER[SPI Master Interface]
        APP[rebear Application]
    end
    
    MCU -->|MOSI| MOSI_PT
    MOSI_PT -->|Pass-Through| FLASH
    FLASH -->|MISO Original| MISO_MOD
    MISO_MOD -->|MISO Patched/Original| MCU
    
    MOSI_PT -->|Record Transactions| BUFFER
    MISO_MOD -->|Apply Patches| MISO_MOD
    
    SPI_MASTER <-->|SPI Commands| FPGA
    BUFFER -->|Transaction Data| SPI_MASTER
    SPI_MASTER -->|Patch Config| MISO_MOD
    APP -->|Control| SPI_MASTER
```

### SPI Bus Topology - Pass-Through Architecture

```
MCU ←→ FPGA ←→ Flash Memory
       (Man-in-the-Middle)

Detailed Signal Flow:

MCU MOSI ──────────► FPGA ──────────► Flash MOSI
                      │ (Monitor & Record)
                      │
MCU MISO ◄──────────┐ FPGA ◄────────── Flash MISO
                    │  │
                    │  └─ Patch Logic
                    │     (Modify MISO if patch matches)
                    │
                    └─ Original or Patched Data

MCU SCK ────────────► FPGA ────────────► Flash SCK
                      │ (Pass-Through)
                      
MCU CS ─────────────► FPGA ─────────────► Flash CS
                      │ (Pass-Through)


Separate SPI Bus for Pi ←→ FPGA Control:

Pi MOSI ◄──────────► FPGA SPI Slave
Pi MISO ◄──────────► FPGA SPI Slave
Pi SCK  ◄──────────► FPGA SPI Slave
Pi CS   ◄──────────► FPGA SPI Slave
```

**Key Points:**
- FPGA is spliced between MCU and Flash (man-in-the-middle)
- MOSI: Pass-through with monitoring (records addresses)
- MISO: Pass-through with optional modification (patches data)
- Flash memory is NEVER modified - only the data stream is altered
- Patches are applied in real-time by modifying MISO signal

## Software Architecture

### Component Hierarchy

```mermaid
graph TB
    subgraph User Interface Layer
        GUI[Qt GUI Application<br/>rebear-gui]
        CLI[Command Line Utility<br/>rebear-cli]
    end
    
    subgraph Application Layer
        PM[PatchManager]
        TM[TransactionManager]
    end
    
    subgraph Core Library Layer
        SPI[SPIProtocol]
        TRANS[Transaction]
        PATCH[Patch]
        ESC[EscapeCodec]
    end
    
    subgraph Hardware Layer
        SPIDEV[Linux spidev Driver]
        HW[FPGA Hardware]
    end
    
    GUI --> PM
    GUI --> TM
    CLI --> PM
    CLI --> TM
    
    PM --> PATCH
    PM --> SPI
    TM --> TRANS
    TM --> SPI
    
    SPI --> ESC
    SPI --> TRANS
    SPI --> PATCH
    
    SPI --> SPIDEV
    SPIDEV --> HW
```

### Class Relationships

```mermaid
classDiagram
    class EscapeCodec {
        +encode(data) vector~uint8_t~
        +decode(data) vector~uint8_t~
        +needsEscape(byte) bool
    }
    
    class Transaction {
        +uint32_t address
        +uint32_t count
        +uint16_t timestamp
        +fromBytes(data) Transaction
        +toBytes() vector~uint8_t~
        +toString() string
    }
    
    class Patch {
        +uint8_t id
        +uint32_t address
        +array~uint8_t,8~ data
        +bool enabled
        +toBytes() vector~uint8_t~
        +fromBytes(data) Patch
        +toString() string
    }
    
    class SPIProtocol {
        -int fd_
        -EscapeCodec codec_
        +open(device, speed) bool
        +close() void
        +clearTransactions() bool
        +readTransaction() optional~Transaction~
        +setPatch(patch) bool
        +clearPatches() bool
    }
    
    class PatchManager {
        -map~uint8_t,Patch~ patches_
        +addPatch(patch) bool
        +removePatch(id) bool
        +getPatches() vector~Patch~
        +applyAll(spi) bool
        +saveToFile(filename) bool
        +loadFromFile(filename) bool
    }
    
    SPIProtocol --> EscapeCodec : uses
    SPIProtocol --> Transaction : creates
    SPIProtocol --> Patch : sends
    PatchManager --> Patch : manages
    PatchManager --> SPIProtocol : uses
```

## Data Flow Diagrams

### Transaction Monitoring Flow

```mermaid
sequenceDiagram
    participant MCU as Teddy Bear MCU
    participant Flash as Flash Memory
    participant FPGA as FPGA
    participant Pi as Raspberry Pi
    participant App as Application
    
    MCU->>Flash: SPI Read Request (addr, count)
    FPGA->>FPGA: Record transaction
    Flash->>MCU: Data response
    
    App->>Pi: Poll for transactions
    Pi->>FPGA: Send 0x01 (Read Transaction)
    FPGA->>Pi: Return 8 bytes (addr, count, time)
    Pi->>Pi: Decode escape sequences
    Pi->>App: Transaction object
    App->>App: Display/analyze transaction
```

### Patch Application Flow

```mermaid
sequenceDiagram
    participant User as User
    participant App as Application
    participant Pi as Raspberry Pi
    participant FPGA as FPGA
    participant MCU as Teddy Bear MCU
    participant Flash as Flash Memory
    
    User->>App: Create patch (id, addr, data)
    App->>App: Validate patch
    App->>Pi: Send patch command
    Pi->>Pi: Encode escape sequences
    Pi->>FPGA: Send 0x02 + 12 bytes
    FPGA->>FPGA: Store patch in slot
    
    Note over FPGA,Flash: Later, when MCU reads...
    
    MCU->>FPGA: SPI Read from address (MOSI)
    FPGA->>Flash: Pass-through MOSI
    Flash->>FPGA: Return data (MISO)
    FPGA->>FPGA: Check if address matches patch
    alt Patch matches
        FPGA->>FPGA: Replace MISO data with patch
        FPGA->>MCU: Modified MISO (patched data)
    else No patch
        FPGA->>MCU: Original MISO (Flash data)
    end
    MCU->>MCU: Process received data
    
    Note over Flash: Flash memory is NEVER modified
```

### Escape Encoding Flow

```mermaid
graph LR
    A[Original Data] --> B{Contains 0x4a<br/>or 0x4d?}
    B -->|No| C[Send as-is]
    B -->|Yes| D[Apply Escape]
    D --> E[0x4a → 0x4d 0x6a]
    D --> F[0x4d → 0x4d 0x6d]
    E --> G[Encoded Data]
    F --> G
    C --> G
    G --> H[Transmit via SPI]
```

## GUI Layout Design

### Main Window Structure

```mermaid
graph TB
    subgraph Main Window
        MB[Menu Bar]
        TB[Toolbar]
        
        subgraph Central Widget
            subgraph Top Split
                TV[Transaction Viewer<br/>QTableView]
                AV[Address Visualizer<br/>Custom Widget]
            end
            
            subgraph Bottom
                PE[Patch Editor<br/>QTableView + Controls]
            end
        end
        
        SB[Status Bar]
    end
    
    MB --> TB
    TB --> TV
    TB --> AV
    TV --> PE
    AV --> PE
    PE --> SB
```

### Transaction Viewer Widget

```mermaid
graph TB
    subgraph TransactionViewer
        TV[QTableView]
        TM[TransactionModel<br/>QAbstractTableModel]
        
        subgraph Controls
            AS[Auto-scroll checkbox]
            CLR[Clear button]
            EXP[Export button]
            SRCH[Search field]
        end
        
        subgraph Columns
            C1[Timestamp ms]
            C2[Address hex]
            C3[Count bytes]
        end
    end
    
    TV --> TM
    TM --> C1
    TM --> C2
    TM --> C3
    Controls --> TV
```

### Patch Editor Widget

```mermaid
graph TB
    subgraph PatchEditor
        PV[QTableView]
        PM[PatchModel<br/>QAbstractTableModel]
        
        subgraph Controls
            ADD[Add Patch]
            EDIT[Edit Patch]
            REM[Remove Patch]
            CLR[Clear All]
            APPLY[Apply All]
        end
        
        subgraph Columns
            C1[ID 0-15]
            C2[Address hex]
            C3[Data 8 bytes hex]
            C4[Status]
        end
        
        subgraph Dialog
            DLG[Patch Edit Dialog<br/>ID, Address, Hex Editor]
        end
    end
    
    PV --> PM
    PM --> C1
    PM --> C2
    PM --> C3
    PM --> C4
    ADD --> DLG
    EDIT --> DLG
    DLG --> PM
```

## State Diagrams

### Application Connection State

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    Disconnected --> Connecting : User clicks Connect
    Connecting --> Connected : SPI open success
    Connecting --> Error : SPI open failed
    Error --> Disconnected : User acknowledges
    Connected --> Monitoring : Start polling
    Monitoring --> Connected : Stop polling
    Connected --> Disconnected : User clicks Disconnect
    Monitoring --> Disconnected : Connection lost
```

### Patch Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Created : User creates patch
    Created --> Validated : Check ID, address
    Validated --> Stored : Add to PatchManager
    Stored --> Applied : Send to FPGA
    Applied --> Active : FPGA confirms
    Active --> Triggered : MCU reads address
    Triggered --> Active : Continue monitoring
    Active --> Removed : User removes patch
    Removed --> [*]
    
    Validated --> Error : Validation failed
    Applied --> Error : SPI error
    Error --> [*]
```

## Memory Layout

### Flash Memory Address Space

**Important**: The external Flash chip contains ONLY audio data and bookkeeping information. The MCU has its own internal program Flash that is not accessible via this SPI bus.

```
0x000000 ┌─────────────────────┐
         │                     │
         │   Header/Index?     │
         │   (Bookkeeping)     │
         │   (Encrypted)       │
         │                     │
0x001000 ├─────────────────────┤
         │                     │
         │   Audio Story 1     │
         │   (Encrypted)       │
         │                     │
0x010000 ├─────────────────────┤
         │                     │
         │   Audio Story 2     │
         │   (Encrypted)       │
         │                     │
0x020000 ├─────────────────────┤
         │                     │
         │   Audio Story 3     │
         │   (Encrypted)       │
         │                     │
         │        ...          │
         │                     │
0x100000 ├─────────────────────┤
         │                     │
         │   More Audio Data   │
         │   (Stories)         │
         │   (Encrypted)       │
         │                     │
0xFFFFFF └─────────────────────┘

Note: MCU program code is in separate internal Flash (not accessible)
```

### Transaction Buffer in FPGA

```
┌──────────────────────────────┐
│ Transaction 0 (8 bytes)      │
├──────────────────────────────┤
│ Transaction 1 (8 bytes)      │
├──────────────────────────────┤
│ Transaction 2 (8 bytes)      │
├──────────────────────────────┤
│          ...                 │
├──────────────────────────────┤
│ Transaction N-1 (8 bytes)    │
└──────────────────────────────┘
         ↑
    Read Pointer
    (advances on 0x01 command)
```

### Patch Storage in FPGA

```
Slot 0:  [ID=0] [Addr: 24-bit] [Data: 8 bytes] [Valid: 1-bit]
Slot 1:  [ID=1] [Addr: 24-bit] [Data: 8 bytes] [Valid: 1-bit]
Slot 2:  [ID=2] [Addr: 24-bit] [Data: 8 bytes] [Valid: 1-bit]
...
Slot 15: [ID=15] [Addr: 24-bit] [Data: 8 bytes] [Valid: 1-bit]
```

## Timing Diagrams

### SPI Transaction Timing

```
CS   ────┐                                    ┌────
         └────────────────────────────────────┘

SCK  ────┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─
         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘

MOSI ────< CMD >< D0 >< D1 >< D2 >< D3 >< ... >

MISO ────────────< R0 >< R1 >< R2 >< R3 >< ... >
         
         |<-Cmd->|<-------- Data Transfer ----->|
```

### Patch Trigger Timing

```
Time ──────────────────────────────────────────►

MCU Read:     ┌──────┐
              │ Addr │
              └──────┘
                 │
FPGA Match:      ├─ Compare with patches
                 │
                 ▼
Patch Found:   ┌────┐
               │Yes │
               └────┘
                 │
Data Inject:     ├─ Replace data
                 │
                 ▼
MCU Receive:   ┌──────────┐
               │Patch Data│
               └──────────┘
```

## Build System Flow

```mermaid
graph TB
    ROOT[Root CMakeLists.txt]
    
    subgraph Library
        LIB_CMAKE[lib/CMakeLists.txt]
        LIB_SRC[Source Files]
        LIB_HDR[Header Files]
        LIB_OUT[librebear.so]
    end
    
    subgraph CLI
        CLI_CMAKE[cli/CMakeLists.txt]
        CLI_SRC[CLI Source]
        CLI_OUT[rebear-cli]
    end
    
    subgraph GUI
        GUI_CMAKE[gui/CMakeLists.txt]
        GUI_SRC[GUI Source]
        GUI_UI[UI Files]
        GUI_OUT[rebear-gui]
    end
    
    ROOT --> LIB_CMAKE
    ROOT --> CLI_CMAKE
    ROOT --> GUI_CMAKE
    
    LIB_CMAKE --> LIB_SRC
    LIB_CMAKE --> LIB_HDR
    LIB_SRC --> LIB_OUT
    LIB_HDR --> LIB_OUT
    
    CLI_CMAKE --> CLI_SRC
    CLI_CMAKE --> LIB_OUT
    CLI_SRC --> CLI_OUT
    LIB_OUT --> CLI_OUT
    
    GUI_CMAKE --> GUI_SRC
    GUI_CMAKE --> GUI_UI
    GUI_CMAKE --> LIB_OUT
    GUI_SRC --> GUI_OUT
    GUI_UI --> GUI_OUT
    LIB_OUT --> GUI_OUT
```

## Deployment Architecture

```mermaid
graph TB
    subgraph Development Machine
        DEV[Developer]
        SRC[Source Code]
        BUILD[Build System]
    end
    
    subgraph Raspberry Pi 3
        RPI[Raspberry Pi OS]
        DEPS[Dependencies<br/>Qt5, spidev]
        APP[rebear Application]
        SPI_DEV[/dev/spidev0.0]
    end
    
    subgraph FPGA
        FPGA_HW[FPGA Hardware]
        FPGA_FW[Firmware]
    end
    
    DEV --> SRC
    SRC --> BUILD
    BUILD -->|Cross-compile or<br/>Native build| APP
    APP --> RPI
    DEPS --> APP
    APP --> SPI_DEV
    SPI_DEV <-->|SPI Bus| FPGA_HW
    FPGA_FW --> FPGA_HW
```

## Error Handling Flow

```mermaid
graph TB
    START[Operation Start]
    START --> TRY{Try Operation}
    
    TRY -->|Success| SUCCESS[Return Success]
    TRY -->|Failure| CLASSIFY{Classify Error}
    
    CLASSIFY -->|SPI Error| SPI_ERR[Log SPI Error<br/>Set lastError]
    CLASSIFY -->|Validation Error| VAL_ERR[Log Validation Error<br/>Set lastError]
    CLASSIFY -->|System Error| SYS_ERR[Log System Error<br/>Set lastError]
    
    SPI_ERR --> RETRY{Retry?}
    VAL_ERR --> FAIL[Return Failure]
    SYS_ERR --> FAIL
    
    RETRY -->|Yes| BACKOFF[Exponential Backoff]
    RETRY -->|No| FAIL
    BACKOFF --> TRY
    
    SUCCESS --> END[End]
    FAIL --> END
```

This comprehensive set of diagrams should help visualize the entire system architecture, data flows, and component interactions for the teddy bear reverse-engineering project.
