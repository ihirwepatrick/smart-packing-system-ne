# Smart Parking System — Architecture Diagram

Paste the block below into [Mermaid Live Editor](https://mermaid.live) or any Mermaid-supported viewer.

```mermaid
flowchart TB
    USER(["Console User"])

    subgraph UI["① Presentation Layer"]
        direction LR
        MENU["Menu Controller<br/>main · printMenu · handlers"]
        VALID["Input Validator<br/>plate · slot · zone · rate"]
        MENU --> VALID
    end

    subgraph CORE["② Application Layer — SmartParkingSystem"]
        direction LR
        SLOT["Configure Slots<br/>addSlot()"]
        ENTRY["Register Entry<br/>registerEntry()"]
        EXIT["Process Exit<br/>processExit()"]
        REPORT["Reports<br/>list · history · revenue"]
        RATE["Update Rates<br/>updateRate()"]
    end

    subgraph DATA["③ Data Layer — In-Memory STL Containers"]
        direction LR
        DS1[("slots_<br/>unordered_map<br/>Slot ID → ParkingSlot")]
        DS2[("activeVehicles_<br/>unordered_map<br/>Plate → ParkedVehicle")]
        DS3[("availableSlotIds_<br/>unordered_map<br/>Type → deque of Slot IDs")]
        DS4[("history_<br/>vector<br/>ParkingTransaction")]
    end

    subgraph SVC["④ Service Layer"]
        direction LR
        RATES["ParkingRateManager<br/>default & updated tariffs"]
        IFACE["IFeeCalculator"]
        FEES["StandardFeeCalculator"]
        FEES -.->|implements| IFACE
    end

  USER --> MENU
  VALID --> SLOT & ENTRY & EXIT & REPORT & RATE

  SLOT --> DS1 & DS3
  ENTRY --> DS1 & DS2 & DS3
  EXIT --> DS1 & DS2 & DS3 & DS4
  EXIT --> FEES
  REPORT --> DS1 & DS2 & DS4
  RATE --> RATES
  FEES --> RATES
```

## Layer summary

| Layer | Components | Role |
|-------|------------|------|
| ① Presentation | Menu Controller, Input Validator | User input, menu navigation, validation |
| ② Application | SmartParkingSystem operations | Business rules: slots, entry, exit, reports, rates |
| ③ Data | 4 STL containers | Fast in-memory storage — no database |
| ④ Service | Rate manager + fee calculator | Tariffs and parking fee logic (polymorphism) |

## Data flow (quick read)

1. **User** picks a menu option → **Input Validator** checks input.
2. **SmartParkingSystem** runs the matching operation.
3. Operations **read/write** the in-memory maps and vector.
4. On **exit**, **StandardFeeCalculator** uses **ParkingRateManager** rates to compute the fee, then saves a **ParkingTransaction** to history.
