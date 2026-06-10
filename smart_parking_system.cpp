/**
 * Smart Parking Management System - Kigali City
 * Single-file C++ implementation using in-memory data structures.
 *
 * Compile: g++ -std=c++17 -Wall -Wextra -o smart_parking smart_parking_system.cpp
 * Run:     ./smart_parking   (or smart_parking.exe on Windows)
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// ---------------------------------------------------------------------------
// Enumerations & helpers
// ---------------------------------------------------------------------------

enum class VehicleType { Motorcycle, Car, Truck };
enum class SlotStatus { Available, Occupied };

string vehicleTypeToString(VehicleType type) {
    switch (type) {
        case VehicleType::Motorcycle: return "Motorcycle";
        case VehicleType::Car:        return "Car";
        case VehicleType::Truck:      return "Truck";
    }
    return "Unknown";
}

string trimInput(const string& input) {
    size_t start = 0;
    while (start < input.size() && isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    size_t end = input.size();
    while (end > start && isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(start, end - start);
}

optional<VehicleType> parseVehicleType(const string& input) {
    string normalized = trimInput(input);
    if (normalized.empty()) return nullopt;

    for (char& ch : normalized) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }

    if (normalized == "1" || normalized == "m" || normalized == "motorcycle") {
        return VehicleType::Motorcycle;
    }
    if (normalized == "2" || normalized == "c" || normalized == "car") {
        return VehicleType::Car;
    }
    if (normalized == "3" || normalized == "t" || normalized == "truck") {
        return VehicleType::Truck;
    }
    return nullopt;
}

string slotStatusToString(SlotStatus status) {
    return status == SlotStatus::Available ? "Available" : "Occupied";
}

int vehicleTypeIndex(VehicleType type) {
    return static_cast<int>(type);
}

// ---------------------------------------------------------------------------
// DateTime - entry/exit time handling
// ---------------------------------------------------------------------------

class DateTime {
public:
    DateTime(int year, int month, int day, int hour, int minute)
        : year_(year), month_(month), day_(day), hour_(hour), minute_(minute) {}

    static optional<DateTime> parse(const string& input) {
        tm tm{};
        istringstream ss(input);
        ss >> get_time(&tm, "%Y-%m-%d %H:%M");
        if (ss.fail()) return nullopt;
        if (!isValid(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min)) {
            return nullopt;
        }
        return DateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                        tm.tm_hour, tm.tm_min);
    }

    static DateTime now() {
        auto current = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(current);
        tm local{};
#ifdef _WIN32
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        return DateTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                        local.tm_hour, local.tm_min);
    }

    int year() const  { return year_; }
    int month() const { return month_; }
    int day() const   { return day_; }
    int hour() const  { return hour_; }
    int minute() const { return minute_; }

    string toString() const {
        ostringstream oss;
        oss << setfill('0')
            << setw(4) << year_ << '-'
            << setw(2) << month_ << '-'
            << setw(2) << day_ << ' '
            << setw(2) << hour_ << ':'
            << setw(2) << minute_;
        return oss.str();
    }

    bool operator<(const DateTime& other) const {
        return toMinutes() < other.toMinutes();
    }

    long long toMinutes() const {
        return static_cast<long long>(year_) * 525600LL
             + static_cast<long long>(month_) * 43200LL
             + static_cast<long long>(day_) * 1440LL
             + static_cast<long long>(hour_) * 60LL
             + minute_;
    }

    int billableHoursSince(const DateTime& entry) const {
        long long minutes = toMinutes() - entry.toMinutes();
        if (minutes <= 0) return 0;
        return static_cast<int>((minutes + 59) / 60);
    }

    bool sameDayAs(const DateTime& other) const {
        return year_ == other.year() && month_ == other.month() && day_ == other.day();
    }

private:
    int year_, month_, day_, hour_, minute_;

    static bool isValid(int year, int month, int day, int hour, int minute) {
        if (year < 2000 || year > 2100) return false;
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;
        if (hour < 0 || hour > 23) return false;
        if (minute < 0 || minute > 59) return false;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Domain models
// ---------------------------------------------------------------------------

class ParkingSlot {
public:
    ParkingSlot(string id, VehicleType supportedType, string zone)
        : id_(move(id)), supportedType_(supportedType),
          zone_(move(zone)), status_(SlotStatus::Available) {}

    const string& id() const { return id_; }
    VehicleType supportedType() const { return supportedType_; }
    const string& zone() const { return zone_; }
    SlotStatus status() const { return status_; }

    bool isAvailable() const { return status_ == SlotStatus::Available; }
    bool supports(VehicleType vehicleType) const { return vehicleType == supportedType_; }

    void occupy() { status_ = SlotStatus::Occupied; }
    void release() { status_ = SlotStatus::Available; }

    string summary() const {
        ostringstream oss;
        oss << "Slot " << id_ << " | Type: " << vehicleTypeToString(supportedType_)
            << " | Zone: " << zone_ << " | Status: " << slotStatusToString(status_);
        return oss.str();
    }

private:
    string id_;
    VehicleType supportedType_;
    string zone_;
    SlotStatus status_;
};

class ParkedVehicle {
public:
    ParkedVehicle(string plate, VehicleType type, string slotId, DateTime entryTime)
        : plate_(move(plate)), type_(type), slotId_(move(slotId)),
          entryTime_(entryTime) {}

    const string& plate() const { return plate_; }
    VehicleType type() const { return type_; }
    const string& slotId() const { return slotId_; }
    const DateTime& entryTime() const { return entryTime_; }

    string summary() const {
        ostringstream oss;
        oss << plate_ << " (" << vehicleTypeToString(type_) << ") in slot "
            << slotId_ << " since " << entryTime_.toString();
        return oss.str();
    }

private:
    string plate_;
    VehicleType type_;
    string slotId_;
    DateTime entryTime_;
};

class ParkingTransaction {
public:
    ParkingTransaction(string plate, VehicleType type, string slotId,
                       DateTime entryTime, DateTime exitTime,
                       int billableHours, int hourlyRate, int totalFee)
        : plate_(move(plate)), type_(type), slotId_(move(slotId)),
          entryTime_(entryTime), exitTime_(exitTime),
          billableHours_(billableHours), hourlyRate_(hourlyRate), totalFee_(totalFee) {}

    const string& plate() const { return plate_; }
    VehicleType type() const { return type_; }
    const DateTime& exitTime() const { return exitTime_; }
    int totalFee() const { return totalFee_; }

    bool occurredOn(const DateTime& day) const {
        return exitTime_.sameDayAs(day);
    }

    string summary() const {
        ostringstream oss;
        oss << plate_ << " | " << vehicleTypeToString(type_)
            << " | Slot " << slotId_
            << " | Entry: " << entryTime_.toString()
            << " | Exit: " << exitTime_.toString()
            << " | Hours: " << billableHours_
            << " | Rate: " << hourlyRate_ << " RWF/hr"
            << " | Fee: " << totalFee_ << " RWF";
        return oss.str();
    }

private:
    string plate_;
    VehicleType type_;
    string slotId_;
    DateTime entryTime_;
    DateTime exitTime_;
    int billableHours_;
    int hourlyRate_;
    int totalFee_;
};

// ---------------------------------------------------------------------------
// Fee calculation (polymorphism)
// ---------------------------------------------------------------------------

class IFeeCalculator {
public:
    virtual ~IFeeCalculator() = default;
    virtual int calculateFee(VehicleType type, const DateTime& entry,
                             const DateTime& exit) const = 0;
    virtual int billableHours(const DateTime& entry, const DateTime& exit) const = 0;
};

class StandardFeeCalculator : public IFeeCalculator {
public:
    explicit StandardFeeCalculator(const array<int, 3>& rates) : rates_(rates) {}

    int billableHours(const DateTime& entry, const DateTime& exit) const override {
        return exit.billableHoursSince(entry);
    }

    int calculateFee(VehicleType type, const DateTime& entry,
                     const DateTime& exit) const override {
        int hours = billableHours(entry, exit);
        return hours * rates_[vehicleTypeIndex(type)];
    }

private:
    const array<int, 3>& rates_;
};

// ---------------------------------------------------------------------------
// Rate manager
// ---------------------------------------------------------------------------

class ParkingRateManager {
public:
    ParkingRateManager() : hourlyRates_{500, 1000, 2000} {}

    int getRate(VehicleType type) const {
        return hourlyRates_[vehicleTypeIndex(type)];
    }

    bool updateRate(VehicleType type, int newRate) {
        if (newRate <= 0) return false;
        hourlyRates_[vehicleTypeIndex(type)] = newRate;
        return true;
    }

    const array<int, 3>& rates() const { return hourlyRates_; }

    void display() const {
        cout << "Current hourly parking rates:\n";
        cout << "  Motorcycle: " << hourlyRates_[0] << " RWF/hour\n";
        cout << "  Car:        " << hourlyRates_[1] << " RWF/hour\n";
        cout << "  Truck:      " << hourlyRates_[2] << " RWF/hour\n";
    }

private:
    array<int, 3> hourlyRates_;
};

// ---------------------------------------------------------------------------
// Input validation
// ---------------------------------------------------------------------------

class InputValidator {
public:
    static bool isValidPlate(const string& plate) {
        if (plate.empty() || plate.size() > 15) return false;
        for (char ch : plate) {
            if (!isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != ' ') {
                return false;
            }
        }
        return true;
    }

    static bool isValidSlotId(const string& id) {
        return !id.empty() && id.size() <= 20;
    }

    static bool isValidZone(const string& zone) {
        return !zone.empty() && zone.size() <= 50;
    }

    static bool isValidRate(int rate) {
        return rate > 0 && rate <= 1'000'000;
    }
};

// ---------------------------------------------------------------------------
// Operation result
// ---------------------------------------------------------------------------

struct OperationResult {
    bool success{false};
    string message;
};

// ---------------------------------------------------------------------------
// Smart Parking System
// ---------------------------------------------------------------------------

class SmartParkingSystem {
public:
    SmartParkingSystem() {
        feeCalculator_ = make_unique<StandardFeeCalculator>(rateManager_.rates());
    }

    OperationResult addSlot(const string& id, VehicleType type, const string& zone) {
        if (!InputValidator::isValidSlotId(id)) {
            return {false, "Invalid slot ID. Use 1-20 non-empty characters."};
        }
        if (!InputValidator::isValidZone(zone)) {
            return {false, "Invalid zone. Zone cannot be empty."};
        }
        if (slots_.count(id)) {
            return {false, "Slot ID '" + id + "' already exists."};
        }

        slots_.emplace(id, ParkingSlot(id, type, zone));
        availableSlotIds_[type].push_back(id);
        return {true, "Slot configured: " + slots_.at(id).summary()};
    }

    OperationResult registerEntry(const string& plate, VehicleType type,
                                  const DateTime& entryTime) {
        if (!InputValidator::isValidPlate(plate)) {
            return {false, "Invalid plate number. Use alphanumeric characters (max 15)."};
        }
        if (activeVehicles_.count(plate)) {
            return {false, "Vehicle '" + plate + "' is already parked."};
        }

        auto slotId = allocateSlot(type);
        if (!slotId) {
            return {false, "No available slot for " + vehicleTypeToString(type) + "."};
        }

        slots_.at(*slotId).occupy();
        removeFromAvailableIndex(*slotId, type);
        activeVehicles_.emplace(plate, ParkedVehicle(plate, type, *slotId, entryTime));

        return {true, "Entry registered. Allocated slot: " + *slotId
                      + " at " + entryTime.toString()};
    }

    OperationResult processExit(const string& plate, const DateTime& exitTime) {
        auto it = activeVehicles_.find(plate);
        if (it == activeVehicles_.end()) {
            return {false, "Vehicle '" + plate + "' is not currently parked."};
        }

        const ParkedVehicle& vehicle = it->second;
        if (exitTime < vehicle.entryTime()) {
            return {false, "Exit time cannot be before entry time."};
        }

        int hours = feeCalculator_->billableHours(vehicle.entryTime(), exitTime);
        int rate = rateManager_.getRate(vehicle.type());
        int fee = feeCalculator_->calculateFee(vehicle.type(), vehicle.entryTime(), exitTime);

        history_.emplace_back(plate, vehicle.type(), vehicle.slotId(),
                              vehicle.entryTime(), exitTime, hours, rate, fee);

        ParkingSlot& slot = slots_.at(vehicle.slotId());
        slot.release();
        availableSlotIds_[vehicle.type()].push_back(vehicle.slotId());
        activeVehicles_.erase(it);

        ostringstream oss;
        oss << "Exit processed for " << plate << ".\n"
            << "  Duration (billable hours): " << hours << "\n"
            << "  Rate applied: " << rate << " RWF/hour\n"
            << "  Total fee: " << fee << " RWF";
        return {true, oss.str()};
    }

    OperationResult updateRate(VehicleType type, int newRate) {
        if (!InputValidator::isValidRate(newRate)) {
            return {false, "Rate must be a positive integer up to 1,000,000 RWF."};
        }
        if (!rateManager_.updateRate(type, newRate)) {
            return {false, "Failed to update rate."};
        }
        feeCalculator_ = make_unique<StandardFeeCalculator>(rateManager_.rates());
        return {true, vehicleTypeToString(type) + " rate updated to " +
                      to_string(newRate) + " RWF/hour. Completed records unchanged."};
    }

    vector<string> listSlots() const {
        vector<string> result;
        result.reserve(slots_.size());
        for (const auto& [id, slot] : slots_) {
            (void)id;
            result.push_back(slot.summary());
        }
        sort(result.begin(), result.end());
        return result;
    }

    vector<string> listAvailableSlots() const {
        vector<string> result;
        for (const auto& [id, slot] : slots_) {
            if (slot.isAvailable()) result.push_back(slot.summary());
        }
        sort(result.begin(), result.end());
        return result;
    }

    vector<string> listParkedVehicles() const {
        vector<string> result;
        result.reserve(activeVehicles_.size());
        for (const auto& [plate, vehicle] : activeVehicles_) {
            (void)plate;
            result.push_back(vehicle.summary());
        }
        sort(result.begin(), result.end());
        return result;
    }

    vector<string> listHistory() const {
        vector<string> result;
        result.reserve(history_.size());
        for (const auto& tx : history_) {
            result.push_back(tx.summary());
        }
        return result;
    }

    int dailyRevenue(const DateTime& day) const {
        int total = 0;
        for (const auto& tx : history_) {
            if (tx.occurredOn(day)) total += tx.totalFee();
        }
        return total;
    }

    void displayRates() const { rateManager_.display(); }

    void seedDemoData() {
        addSlot("M-01", VehicleType::Motorcycle, "Zone A");
        addSlot("M-02", VehicleType::Motorcycle, "Zone A");
        addSlot("C-01", VehicleType::Car, "Zone B");
        addSlot("C-02", VehicleType::Car, "Zone B");
        addSlot("T-01", VehicleType::Truck, "Zone C");
    }

private:
    unordered_map<string, ParkingSlot> slots_;
    unordered_map<string, ParkedVehicle> activeVehicles_;
    unordered_map<VehicleType, deque<string>> availableSlotIds_;
    vector<ParkingTransaction> history_;

    ParkingRateManager rateManager_;
    unique_ptr<IFeeCalculator> feeCalculator_;

    optional<string> allocateSlot(VehicleType type) const {
        auto it = availableSlotIds_.find(type);
        if (it == availableSlotIds_.end() || it->second.empty()) {
            return nullopt;
        }
        return it->second.front();
    }

    void removeFromAvailableIndex(const string& slotId, VehicleType type) {
        auto& queue = availableSlotIds_[type];
        queue.erase(remove(queue.begin(), queue.end(), slotId), queue.end());
    }
};

// ---------------------------------------------------------------------------
// Console UI helpers
// ---------------------------------------------------------------------------

void clearInputStream() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

string readLine(const string& prompt, const string& hint = "") {
    string value;
    cout << prompt << '\n';
    if (!hint.empty()) {
        cout << "  Hint: " << hint << '\n';
    }
    cout << "> ";
    getline(cin, value);
    return value;
}

int readInt(const string& prompt, int minVal, int maxVal, const string& hint = "") {
    while (true) {
        cout << prompt << '\n';
        cout << "  Hint: Enter a whole number from " << minVal << " to " << maxVal;
        if (!hint.empty()) {
            cout << " (" << hint << ")";
        }
        cout << ".\n> ";
        int value;
        if (cin >> value && value >= minVal && value <= maxVal) {
            clearInputStream();
            return value;
        }
        cout << "Invalid input. Please enter a number between " << minVal
             << " and " << maxVal << ".\n";
        clearInputStream();
    }
}

VehicleType readVehicleTypeInput() {
    while (true) {
        cout << "Vehicle type\n"
             << "  Hint: Type ONE letter or number (no need for the full word):\n"
             << "        1 or M  = Motorcycle\n"
             << "        2 or C  = Car\n"
             << "        3 or T  = Truck\n"
             << "> ";
        string input;
        getline(cin, input);
        auto type = parseVehicleType(input);
        if (type) return *type;
        cout << "Invalid choice. Enter 1/M, 2/C, or 3/T only.\n";
    }
}

DateTime readDateTimeInput(const string& label) {
    while (true) {
        string input = readLine(
            label,
            "Format: YYYY-MM-DD HH:MM (24-hour), e.g. 2026-06-10 14:30");
        auto dt = DateTime::parse(input);
        if (dt) return *dt;
        cout << "Invalid datetime. Example: 2026-06-10 14:30\n";
    }
}

void printList(const string& title, const vector<string>& items) {
    cout << "\n=== " << title << " ===\n";
    if (items.empty()) {
        cout << "(none)\n";
        return;
    }
    for (const auto& item : items) {
        cout << "  - " << item << '\n';
    }
}

void printMenu() {
    cout << "\n========================================\n"
              << "   KIGALI SMART PARKING SYSTEM\n"
              << "========================================\n"
              << " 1. Configure parking slot\n"
              << " 2. View all parking slots\n"
              << " 3. Register vehicle entry\n"
              << " 4. Process vehicle exit\n"
              << " 5. Update parking rates\n"
              << " 6. View current rates\n"
              << " 7. View available slots\n"
              << " 8. View parked vehicles\n"
              << " 9. View parking history\n"
              << "10. View daily revenue\n"
              << "11. Load demo data\n"
              << " 0. Exit\n"
              << "----------------------------------------\n";
}

void handleAddSlot(SmartParkingSystem& system) {
    cout << "\n--- Configure Parking Slot ---\n";
    string id = readLine(
        "Slot ID",
        "Unique identifier, 1-20 characters (e.g. M-01, C-02, T-01)");
    VehicleType type = readVehicleTypeInput();
    string zone = readLine(
        "Zone / location",
        "Parking area name, 1-50 characters (e.g. Zone A, Downtown, Airport)");
    auto result = system.addSlot(id, type, zone);
    cout << (result.success ? "\n[SUCCESS] " : "\n[ERROR] ") << result.message << '\n';
}

void handleEntry(SmartParkingSystem& system) {
    cout << "\n--- Register Vehicle Entry ---\n";
    string plate = readLine(
        "Plate number",
        "Alphanumeric only, max 15 chars (e.g. RAB123A, RAD456B)");
    VehicleType type = readVehicleTypeInput();
    int choice = readInt(
        "Entry time option",
        1, 2,
        "1 = use current system time, 2 = enter time manually");
    DateTime entry = (choice == 1) ? DateTime::now() : readDateTimeInput("Entry time");
    auto result = system.registerEntry(plate, type, entry);
    cout << (result.success ? "\n[SUCCESS] " : "\n[ERROR] ") << result.message << '\n';
}

void handleExit(SmartParkingSystem& system) {
    cout << "\n--- Process Vehicle Exit ---\n";
    string plate = readLine(
        "Plate number",
        "Enter the plate of a currently parked vehicle (e.g. RAB123A)");
    int choice = readInt(
        "Exit time option",
        1, 2,
        "1 = use current system time, 2 = enter time manually");
    DateTime exitTime = (choice == 1) ? DateTime::now() : readDateTimeInput("Exit time");
    auto result = system.processExit(plate, exitTime);
    cout << (result.success ? "\n[SUCCESS] " : "\n[ERROR] ") << result.message << '\n';
}

void handleUpdateRate(SmartParkingSystem& system) {
    cout << "\n--- Update Parking Rate ---\n";
    cout << "Note: New rates apply only to future exits. History is unchanged.\n";
    VehicleType type = readVehicleTypeInput();
    int rate = readInt(
        "New hourly rate (RWF)",
        1, 1'000'000,
        "positive amount in Rwandan Francs, e.g. 500, 1000, 2000");
    auto result = system.updateRate(type, rate);
    cout << (result.success ? "\n[SUCCESS] " : "\n[ERROR] ") << result.message << '\n';
}

void handleDailyRevenue(SmartParkingSystem& system) {
    cout << "\n--- Daily Revenue Report ---\n";
    int choice = readInt(
        "Date option",
        1, 2,
        "1 = today's revenue, 2 = pick a specific date");
    DateTime day = (choice == 1) ? DateTime::now() : readDateTimeInput("Report date");
    int revenue = system.dailyRevenue(day);
    cout << "\nRevenue on " << day.toString().substr(0, 10)
              << ": " << revenue << " RWF\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    SmartParkingSystem system;

    cout << "Welcome to Kigali Smart Parking Management System\n";
    cout << "Default rates: Motorcycle 500 | Car 1000 | Truck 2000 RWF/hour\n";
    cout << "Partial hours are billed as full hours.\n";

    while (true) {
        printMenu();
        int choice = readInt(
            "Select menu option",
            0, 11,
            "0 = exit, 1-11 = actions shown above");

        switch (choice) {
            case 1:  handleAddSlot(system); break;
            case 2:  printList("All Parking Slots", system.listSlots()); break;
            case 3:  handleEntry(system); break;
            case 4:  handleExit(system); break;
            case 5:  handleUpdateRate(system); break;
            case 6:  system.displayRates(); break;
            case 7:  printList("Available Slots", system.listAvailableSlots()); break;
            case 8:  printList("Parked Vehicles", system.listParkedVehicles()); break;
            case 9:  printList("Parking History", system.listHistory()); break;
            case 10: handleDailyRevenue(system); break;
            case 11:
                system.seedDemoData();
                cout << "\n[SUCCESS] Demo slots loaded (M-01, M-02, C-01, C-02, T-01).\n";
                break;
            case 0:
                cout << "Thank you. Goodbye.\n";
                return 0;
            default:
                cout << "Invalid option.\n";
        }
    }
}
