#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <map>
#include <ctime>
#include <vector>
#include <sstream>
#include <array>
#include <cstring>
#include <stdint.h>
#include "sha1.h"
#include <cctype>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "httplib.h"
#include <process.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <mutex>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")

using json = nlohmann::json;

// Debug flag - set to true to enable key press logging
bool DEBUG_KEY_PRESSES = false;

// Helper function to get key name from virtual key code
std::string getKeyName(WORD vk) {
    switch (vk) {
        case VK_RETURN: return "ENTER";
        case VK_ESCAPE: return "ESC";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        case VK_SHIFT: return "SHIFT";
        case VK_MENU: return "ALT";
        default: return "*";  // Mask all other keys
    }
}

// Helper function to log key presses
void logKeyPress(WORD vk, bool isKeyUp = false) {
    if (!DEBUG_KEY_PRESSES) return;
    
    std::string action = isKeyUp ? "Released" : "Pressed";
    std::string keyName = getKeyName(vk);
    std::cout << "[Key] " << action << " " << keyName << std::endl;
}

struct AccountConfig {
    std::string name;
    std::string password;
    std::string totpSecret;
    int profileGroup; // 1-5, which login_w.{N}.bin this account belongs to
    int slot;         // 1-4, the slot within the profile group's login_w.bin
    std::string args;
};

struct AccountDefaults {
    std::string password;
    std::string totpSecret;
    std::string args;
};

struct SavedPreset {
    std::string name;
    std::vector<int> accountIndices; // 1-based indices into accounts array
};

struct GlobalConfig {
    int delay;
    bool POLProxy;
    std::string clientRegion; // "US" or "JP" or "EU"
    std::string polPath;     // Path to POL usr\all directory
    int activeProfileGroup;  // Last profile group loaded into login_w.bin (0 = unknown)
    AccountDefaults defaults; // Default values for new accounts
    std::vector<AccountConfig> accounts;
    std::vector<SavedPreset> presets; // User-saved launch presets
};

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_LENGTH 20

std::string base32_decode(const std::string& input) {
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::vector<uint8_t> output;
    int buffer = 0, bitsLeft = 0;
    for (char c : input) {
        if (c == '=' || c == ' ') break;
        const char* p = strchr(alphabet, toupper(c));
        if (!p) continue;
        buffer <<= 5;
        buffer |= (p - alphabet);
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            output.push_back((buffer >> (bitsLeft - 8)) & 0xFF);
            bitsLeft -= 8;
        }
    }
    return std::string(output.begin(), output.end());
}

// SHA1 implementation public domain (Steve Reid, others)
void sha1(const uint8_t* data, size_t len, uint8_t* out);

void hmac_sha1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t output[20]) {
    const size_t blockSize = 64;
    const size_t hashSize = 20;
    uint8_t k_ipad[blockSize] = { 0 };
    uint8_t k_opad[blockSize] = { 0 };
    uint8_t tk[hashSize] = { 0 };

    if (key_len > blockSize) {
        sha1(key, key_len, tk);
        key = tk;
        key_len = hashSize;
    }

    uint8_t k0[blockSize] = { 0 };
    memcpy(k0, key, key_len);

    for (size_t i = 0; i < blockSize; ++i) {
        k_ipad[i] = k0[i] ^ 0x36;
        k_opad[i] = k0[i] ^ 0x5c;
    }

    std::vector<uint8_t> inner_data;
    inner_data.insert(inner_data.end(), k_ipad, k_ipad + blockSize);
    inner_data.insert(inner_data.end(), data, data + data_len);

    uint8_t inner_hash[hashSize];
    sha1(inner_data.data(), inner_data.size(), inner_hash);

    std::vector<uint8_t> outer_data;
    outer_data.insert(outer_data.end(), k_opad, k_opad + blockSize);
    outer_data.insert(outer_data.end(), inner_hash, inner_hash + hashSize);

    sha1(outer_data.data(), outer_data.size(), output);
}

std::string generate_totp(const std::string& secret_base32) {
    std::string key = base32_decode(secret_base32);
    uint64_t timestep = time(nullptr) / 30;
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = timestep & 0xFF;
        timestep >>= 8;
    }

    uint8_t hash[20];
    hmac_sha1((uint8_t*)key.data(), key.size(), msg, 8, hash);

    int offset = hash[19] & 0x0F;
    int binary =
        ((hash[offset] & 0x7F) << 24) |
        ((hash[offset + 1] & 0xFF) << 16) |
        ((hash[offset + 2] & 0xFF) << 8) |
        (hash[offset + 3] & 0xFF);

    int code = binary % 1000000;
    char buf[7];
    snprintf(buf, sizeof(buf), "%06d", code);
    return std::string(buf);
}

void simulateKey(WORD vk, bool shift = false) {
    INPUT inputs[4] = {};
    int count = 0;
    if (shift) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        logKeyPress(VK_SHIFT);
        count++;
    }
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = vk;
    logKeyPress(vk);
    count++;
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = vk;
    inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
    logKeyPress(vk, true);
    count++;
    if (shift) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        logKeyPress(VK_SHIFT, true);
        count++;
    }
    SendInput(count, inputs, sizeof(INPUT));
    Sleep(30);
}

void copyAndPasteText(HWND hwnd, const std::string& text) {
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Clipboard] Copying and pasting: " << std::string(text.length(), '*') << std::endl;
    }
    
    // Save current clipboard content
    std::string originalClipboard;
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData != NULL) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText != NULL) {
                originalClipboard = pszText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    
    // Set text to clipboard
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, text.length() + 1);
        if (hGlob != NULL) {
            strcpy_s(static_cast<char*>(hGlob), text.length() + 1, text.c_str());
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
    }
    
    // Focus window
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    BringWindowToTop(hwnd);
    
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Timing] Waiting 50ms for window focus..." << std::endl;
    }
    Sleep(50); // Give window time to focus
    
    // Try PostMessage first (asynchronous)
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Paste] Sending PostMessage WM_PASTE" << std::endl;
    }
    PostMessage(hwnd, WM_PASTE, 0, 0);
    
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Timing] Waiting 200ms after PostMessage..." << std::endl;
    }
    Sleep(200);
    
    // Try SendMessage (synchronous)
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Paste] Sending SendMessage WM_PASTE" << std::endl;
    }
    SendMessage(hwnd, WM_PASTE, 0, 0);
    
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Timing] Waiting 200ms after SendMessage..." << std::endl;
    }
    Sleep(200);
    
    // If WM_PASTE didn't work, try keybd_event
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Key] Pressed CTRL (keybd_event)" << std::endl;
        std::cout << "[Key] Pressed V (keybd_event)" << std::endl;
    }
    
    keybd_event(VK_CONTROL, 0, 0, 0);
    Sleep(10);
    keybd_event('V', 0, 0, 0);
    Sleep(10);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    Sleep(10);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Key] Released V (keybd_event)" << std::endl;
        std::cout << "[Key] Released CTRL (keybd_event)" << std::endl;
        std::cout << "[Timing] Waiting 500ms after paste..." << std::endl;
    }
    
    Sleep(500);
}

void sendText(HWND hwnd, const std::string& text, int delay = 50) {
    if (DEBUG_KEY_PRESSES) {
        std::cout << "[Text] Sending " << text.length() << " characters: " << std::string(text.length(), '*') << std::endl;
    }
    for (char c : text) {
        SHORT vk = VkKeyScanA(c);
        if (vk == -1) continue;
        bool shift = (vk & 0x0100) != 0;
        WORD vkCode = vk & 0xFF;
        SetForegroundWindow(hwnd);
        simulateKey(vkCode, shift);
        Sleep(delay);
    }
}

// Forward declarations
void removeHostsEntry();

// Global flag to track if hosts entry was added (so we know to clean it up)
std::atomic<bool> hostsEntryAdded(false);

// Add back the addHostsEntry function
void addHostsEntry(const std::string& ip) {
    std::ifstream in("C:\\Windows\\System32\\drivers\\etc\\hosts");
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("wh000.pol.com") != std::string::npos && line.find("#ffxi-autologin") != std::string::npos)
            return; // Entry already exists
    }
    in.close();

    std::ofstream out("C:\\Windows\\System32\\drivers\\etc\\hosts", std::ios::app);
    out << "\n" << ip << " wh000.pol.com #ffxi-autologin\n";
}

// Define a struct for passing data to EnumWindowsProc
struct WindowSearchData {
    const std::wstring* username;
    HWND* foundHwnd;
};

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    wchar_t title[256];
    GetWindowTextW(hWnd, title, sizeof(title) / sizeof(wchar_t));
    if (wcsncmp(title, L"PlayOnline Viewer", 17) == 0) {
        WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
        // Create a new title with username only
        std::wstring newTitle = L"PlayOnline Viewer - ";
        newTitle += *(data->username);
        SetWindowTextW(hWnd, newTitle.c_str());
        // Bring this window to the front and focus it
        SetForegroundWindow(hWnd);
        SetActiveWindow(hWnd);
        SetFocus(hWnd);
        BringWindowToTop(hWnd);
        *(data->foundHwnd) = hWnd;
        return FALSE;
    }
    return TRUE;
}

std::string readConfigFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Discover the POL usr\all directory path via registry, common paths, or user prompt
std::string discoverPolPath() {
    // Method 1: Check Windows Registry
    // Try HKLM\SOFTWARE\WOW6432Node\PlayOnline\InstallFolder and similar keys
    const char* regPaths[] = {
        "SOFTWARE\\WOW6432Node\\PlayOnline\\InstallFolder",
        "SOFTWARE\\PlayOnline\\InstallFolder",
        "SOFTWARE\\WOW6432Node\\PlayOnlineUS\\InstallFolder",
        "SOFTWARE\\PlayOnlineUS\\InstallFolder",
        "SOFTWARE\\WOW6432Node\\PlayOnlineEU\\InstallFolder",
        "SOFTWARE\\PlayOnlineEU\\InstallFolder",
    };

    for (const char* regPath : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char value[MAX_PATH] = {0};
            DWORD valueSize = sizeof(value);
            DWORD type = 0;
            if (RegQueryValueExA(hKey, "0001", NULL, &type, (LPBYTE)value, &valueSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                std::string polBase = value;
                std::string usrAllPath = polBase + "\\usr\\all";
                if (GetFileAttributesA(usrAllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    std::cout << "POL path found via registry: " << usrAllPath << "\n";
                    return usrAllPath;
                }
            } else {
                RegCloseKey(hKey);
            }
        }
    }

    // Method 2: Check common install paths
    const char* commonPaths[] = {
        "C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all",
        "C:\\Program Files\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all",
        "D:\\Program Files (x86)\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all",
        "D:\\Program Files\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all",
    };

    for (const char* commonPath : commonPaths) {
        if (GetFileAttributesA(commonPath) != INVALID_FILE_ATTRIBUTES) {
            std::cout << "POL path found at common location: " << commonPath << "\n";
            return std::string(commonPath);
        }
    }

    // Method 3: Prompt the user
    std::cout << "\nCould not automatically detect your PlayOnline installation.\n";
    std::cout << "Please enter the full path to your PlayOnline 'usr\\all' directory.\n";
    std::cout << "Example: C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all\n";
    std::cout << "Path: ";
    std::string userPath;
    std::getline(std::cin, userPath);

    // Trim whitespace
    userPath.erase(0, userPath.find_first_not_of(" \t\""));
    userPath.erase(userPath.find_last_not_of(" \t\"") + 1);

    if (GetFileAttributesA(userPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::cout << "Path verified: " << userPath << "\n";
        return userPath;
    }

    std::cerr << "WARNING: Path does not exist or is not accessible: " << userPath << "\n";
    std::cerr << "Storing it anyway. You can update it later in config.json under \"polPath\".\n";
    return userPath;
}

void writeConfigFile(const std::string& path, const GlobalConfig& config) {
    // Backup existing config before overwriting (never destroy data)
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::string backupPath = path + ".bak";
        CopyFileA(path.c_str(), backupPath.c_str(), FALSE);
    }

    json j;
    j["delay"] = config.delay;
    j["clientRegion"] = config.clientRegion;
    j["polPath"] = config.polPath;
    j["activeProfileGroup"] = config.activeProfileGroup;

    // Save defaults
    json defaults;
    defaults["password"] = config.defaults.password;
    defaults["totpSecret"] = config.defaults.totpSecret;
    defaults["args"] = config.defaults.args;
    j["defaults"] = defaults;

    json accounts = json::array();
    for (const auto& account : config.accounts) {
        json acc;
        acc["name"] = account.name;
        acc["password"] = account.password;
        acc["totpSecret"] = account.totpSecret;
        acc["profileGroup"] = account.profileGroup;
        acc["slot"] = account.slot;
        acc["args"] = account.args;
        accounts.push_back(acc);
    }
    j["accounts"] = accounts;

    // Save presets
    json presets = json::array();
    for (const auto& preset : config.presets) {
        json p;
        p["name"] = preset.name;
        p["accounts"] = preset.accountIndices;
        presets.push_back(p);
    }
    j["presets"] = presets;

    std::ofstream file(path);
    file << j.dump(4);
}

GlobalConfig loadConfig(const std::string& path) {
    GlobalConfig config;
    config.POLProxy = true;
    std::string content = readConfigFile(path);
    if (content.empty()) {
        config.clientRegion = "US"; // Default to US
        return config;
    }
    try {
        json j = json::parse(content);
        config.delay = j.value("delay", 3000);
        config.clientRegion = j.value("clientRegion", "US"); // Default to US if not set
        config.polPath = j.value("polPath", ""); // Empty means needs discovery
        config.activeProfileGroup = j.value("activeProfileGroup", 0); // 0 = unknown

        // Load defaults
        if (j.contains("defaults")) {
            auto& d = j["defaults"];
            config.defaults.password = d.value("password", "");
            config.defaults.totpSecret = d.value("totpSecret", "");
            config.defaults.args = d.value("args", "");
        }

        // Load presets
        if (j.contains("presets")) {
            for (const auto& p : j["presets"]) {
                SavedPreset preset;
                preset.name = p.value("name", "");
                if (p.contains("accounts")) {
                    for (const auto& idx : p["accounts"]) {
                        preset.accountIndices.push_back(idx.get<int>());
                    }
                }
                config.presets.push_back(preset);
            }
        }

        if (j.contains("accounts")) {
            for (const auto& acc : j["accounts"]) {
                AccountConfig account;
                account.name = acc.value("name", "");
                account.password = acc.value("password", "");
                account.totpSecret = acc.value("totpSecret", "");
                account.profileGroup = acc.value("profileGroup", 0); // 0 means legacy (needs migration)
                account.slot = acc.value("slot", 1);
                account.args = acc.value("args", "");
                config.accounts.push_back(account);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;

        // Offer to restore from backup
        std::string backupPath = path + ".bak";
        if (GetFileAttributesA(backupPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::cerr << "A backup config exists at: " << backupPath << "\n";
            std::cerr << "Would you like to restore from backup? (y/n): ";
            std::string input;
            std::getline(std::cin, input);
            if (input == "y" || input == "Y") {
                CopyFileA(backupPath.c_str(), path.c_str(), FALSE);
                std::cerr << "Backup restored. Reloading...\n";
                return loadConfig(path); // Recursive reload from restored backup
            }
        }

        config.clientRegion = "US"; // Default to US on error
    }
    return config;
}

// Migrate legacy configs that don't have profileGroup assignments
bool migrateConfig(GlobalConfig& config) {
    // Check if any account is missing profileGroup (value 0 = legacy)
    bool needsMigration = false;
    for (const auto& acc : config.accounts) {
        if (acc.profileGroup == 0) {
            needsMigration = true;
            break;
        }
    }
    if (!needsMigration) return false;

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Config Migration Required\n";
    std::cout << "========================================\n";
    std::cout << "Your config is from an older version and needs profile group assignments.\n";
    std::cout << "Each account needs a profile group (1-5) and slot (1-4).\n\n";
    std::cout << "Would you like to:\n";
    std::cout << "  [A] Auto-assign all accounts to group 1 with sequential slots\n";
    std::cout << "  [M] Manually assign each account\n";
    std::cout << "Choice: ";

    std::string input;
    std::getline(std::cin, input);
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);

    if (input == "a") {
        int slot = 1;
        int group = 1;
        for (auto& acc : config.accounts) {
            if (acc.profileGroup == 0) {
                acc.profileGroup = group;
                acc.slot = slot;
                slot++;
                if (slot > 4) {
                    slot = 1;
                    group++;
                    if (group > 5) {
                        std::cerr << "WARNING: More than 20 accounts detected. Extra accounts assigned to group 5.\n";
                        group = 5;
                    }
                }
            }
        }
        std::cout << "Auto-assigned " << config.accounts.size() << " accounts across " << group << " profile group(s).\n";
    } else {
        for (auto& acc : config.accounts) {
            if (acc.profileGroup == 0) {
                std::cout << "\nAccount: " << acc.name << " (current POL slot: " << acc.slot << ")\n";

                // Profile group
                while (true) {
                    std::cout << "  Profile group (1-5): ";
                    std::getline(std::cin, input);
                    if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                        int pg = std::stoi(input);
                        if (pg >= 1 && pg <= 5) {
                            acc.profileGroup = pg;
                            break;
                        }
                    }
                    std::cout << "  Must be 1-5. Try again.\n";
                }

                // Slot
                while (true) {
                    std::cout << "  Slot within group " << acc.profileGroup << " (1-4): ";
                    std::getline(std::cin, input);
                    if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                        int slot = std::stoi(input);
                        if (slot >= 1 && slot <= 4) {
                            // Check for duplicates
                            bool duplicate = false;
                            for (const auto& other : config.accounts) {
                                if (&other == &acc) continue;
                                if (other.profileGroup == acc.profileGroup && other.slot == slot) {
                                    duplicate = true;
                                    break;
                                }
                            }
                            if (duplicate) {
                                std::cout << "  Group " << acc.profileGroup << " slot " << slot << " is already in use. Try again.\n";
                                continue;
                            }
                            acc.slot = slot;
                            break;
                        }
                    }
                    std::cout << "  Must be 1-4. Try again.\n";
                }
            }
        }
        std::cout << "Manual assignment complete.\n";
    }
    return true; // Config was modified, needs saving
}

// Helper: resolve account field with fallback to defaults
std::string resolvePassword(const AccountConfig& acc, const AccountDefaults& defaults) {
    return acc.password.empty() ? defaults.password : acc.password;
}
std::string resolveTotpSecret(const AccountConfig& acc, const AccountDefaults& defaults) {
    return acc.totpSecret.empty() ? defaults.totpSecret : acc.totpSecret;
}
std::string resolveArgs(const AccountConfig& acc, const AccountDefaults& defaults) {
    return acc.args.empty() ? defaults.args : acc.args;
}

// Helper: prompt for Windower arguments with default option
std::string promptWindowerArgs(const std::string& currentArgs = "") {
    std::string input;
    if (currentArgs.empty()) {
        std::cout << "Windower arguments:\n";
        std::cout << "  [Enter] = none\n";
        std::cout << "  [D]     = Default Profile (-p=\"Default Profile\")\n";
        std::cout << "  Or type custom args (e.g. -p=\"MyProfile\")\n";
        std::cout << "  Choice: ";
    } else {
        std::cout << "Windower arguments (current: " << currentArgs << "):\n";
        std::cout << "  [Enter] = keep current\n";
        std::cout << "  [D]     = Default Profile (-p=\"Default Profile\")\n";
        std::cout << "  [N]     = none (clear)\n";
        std::cout << "  Or type custom args (e.g. -p=\"MyProfile\")\n";
        std::cout << "  Choice: ";
    }
    std::getline(std::cin, input);

    if (input.empty()) {
        return currentArgs; // keep current (or empty if new)
    }

    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

    if (lowerInput == "d") {
        return "-p=\"Default Profile\"";
    }
    if (lowerInput == "n") {
        return "";
    }
    return input; // custom value
}

void setupConfig(GlobalConfig& config) {
    std::cout << "Setting up FFXI autoPOL configuration\n";
    std::string input;
    
    // Client region selection
    while (true) {
        std::cout << "Client region (US/JP/EU, default US): ";
        std::getline(std::cin, input);
        if (input.empty()) {
            config.clientRegion = "US";
            break;
        }
        std::transform(input.begin(), input.end(), input.begin(), ::toupper);
        if (input == "US" || input == "JP" || input == "EU") {
            config.clientRegion = input;
            break;
        }
        std::cout << "Please enter 'US' or 'JP' or 'EU'.\n";
    }
    
    std::cout << "Delay before input starts (in seconds, default 3): ";
    std::getline(std::cin, input);
    if (input.empty()) {
        config.delay = 3000; // Default to 3 seconds if nothing entered
    } else if (std::all_of(input.begin(), input.end(), ::isdigit)) {
        int val = std::stoi(input);
        if (val >= 1 && val <= 20) config.delay = val * 1000;
    }

    // Offer to set account defaults
    std::cout << "\n--- Account Defaults ---\n";
    std::cout << "If your accounts share the same password, TOTP, or Windower args,\n";
    std::cout << "you can set defaults here. Accounts will use these unless overridden.\n\n";
    std::cout << "Set up shared defaults? (y/n): ";
    std::getline(std::cin, input);
    if (input == "y" || input == "Y") {
        std::cout << "Default password (shared across accounts): ";
        std::getline(std::cin, config.defaults.password);
        std::cout << "Default TOTP secret (leave empty if not using): ";
        std::getline(std::cin, config.defaults.totpSecret);
        config.defaults.args = promptWindowerArgs();
        std::cout << "Defaults saved.\n\n";
    }

    int numAccounts = 0;
    while (true) {
        std::cout << "How many characters do you want to set up? ";
        std::getline(std::cin, input);
        if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
            numAccounts = std::stoi(input);
            if (numAccounts > 0) break;
        }
        std::cout << "Please enter a valid number greater than 0.\n";
    }
    for (int i = 0; i < numAccounts; i++) {
        std::cout << "\nSetting up character " << (i + 1) << ":\n";
        AccountConfig account;
        // Name (no spaces)
        while (true) {
            std::cout << "Character name (no spaces, unique): ";
            std::getline(std::cin, account.name);
            if (account.name.find(' ') != std::string::npos || account.name.empty()) {
                std::cout << "Name cannot contain spaces or be empty. Try again.\n";
                continue;
            }
            bool duplicate = false;
            for (const auto& acc : config.accounts) {
                if (_stricmp(acc.name.c_str(), account.name.c_str()) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                std::cout << "Name must be unique. Try again.\n";
                continue;
            }
            break;
        }
        std::cout << "Password (leave empty to use default): ";
        std::getline(std::cin, account.password);
        std::cout << "TOTP Secret (leave empty to use default): ";
        std::getline(std::cin, account.totpSecret);
        // Profile Group (1-5)
        while (true) {
            std::cout << "Profile group (1-5, default 1): ";
            std::getline(std::cin, input);
            if (input.empty()) {
                account.profileGroup = 1;
                break;
            }
            if (std::all_of(input.begin(), input.end(), ::isdigit)) {
                int pg = std::stoi(input);
                if (pg >= 1 && pg <= 5) {
                    account.profileGroup = pg;
                    break;
                }
            }
            std::cout << "Profile group must be 1-5. Try again.\n";
        }
        // Slot (1-4) within the profile group
        while (true) {
            std::cout << "POL Slot number within profile group (1-4): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                int slot = std::stoi(input);
                if (slot >= 1 && slot <= 4) {
                    // Check for duplicate profileGroup+slot
                    bool duplicate = false;
                    for (const auto& acc : config.accounts) {
                        if (acc.profileGroup == account.profileGroup && acc.slot == slot) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (duplicate) {
                        std::cout << "Profile group " << account.profileGroup << " slot " << slot << " is already in use. Try again.\n";
                        continue;
                    }
                    account.slot = slot;
                    break;
                }
            }
            std::cout << "Slot must be 1-4. Try again.\n";
        }
        account.args = promptWindowerArgs();
        config.accounts.push_back(account);
    }
}

int getLoginWValue(const std::string& polPath) {
    std::string loginWPath = polPath + "\\usr\\all\\login_w.bin";
    std::ifstream file(loginWPath, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open login_w.bin at: " << loginWPath << "\n";
        return -1;
    }

    // Seek to offset 0x64
    file.seekg(0x64);
    if (file.fail()) {
        std::cerr << "Failed to seek to offset 0x64 in login_w.bin\n";
        return -1;
    }

    // Read the byte at that offset
    unsigned char value;
    file.read(reinterpret_cast<char*>(&value), 1);
    if (file.fail()) {
        std::cerr << "Failed to read value from login_w.bin\n";
        return -1;
    }

    return value;
}

std::string getPOLPath(DWORD processId) {
    //std::cout << "Looking for POL.exe in process " << processId << "\n";
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return "";
    }

    MODULEENTRY32W moduleEntry;
    moduleEntry.dwSize = sizeof(moduleEntry);
    if (!Module32FirstW(snapshot, &moduleEntry)) {
        std::cerr << "Module32First failed with error: " << GetLastError() << "\n";
        CloseHandle(snapshot);
        return "";
    }

    std::string polPath;
    do {
        std::wstring moduleName = moduleEntry.szModule;
        
        if (_wcsicmp(moduleName.c_str(), L"pol.exe") == 0) {
            // Convert wide string to narrow string
            int size = WideCharToMultiByte(CP_UTF8, 0, moduleEntry.szExePath, -1, NULL, 0, NULL, NULL);
            std::string path(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, moduleEntry.szExePath, -1, &path[0], size, NULL, NULL);
            polPath = path;
            //std::cout << "Found POL.exe at: " << polPath << "\n";
            break;
        }
    } while (Module32NextW(snapshot, &moduleEntry));

    if (polPath.empty()) {
        std::cerr << "POL.exe not found.\n";
    }

    CloseHandle(snapshot);
    return polPath;
}

// Helper to defocus any existing PlayOnline Viewer window
void defocusExistingPOL() {
    HWND fg = GetForegroundWindow();
    wchar_t title[256];
    if (fg && GetWindowTextW(fg, title, 256)) {
        if (wcsncmp(title, L"PlayOnline Viewer", 17) == 0 ||
            wcsncmp(title, L"Final Fantasy XI", 16) == 0) {
            // Set focus to desktop
            HWND desktop = GetDesktopWindow();
            SetForegroundWindow(desktop);
            Sleep(100);
        }
    }
}

// Swap the correct profile group's login_w.bin into place
// Returns true if swap succeeded (or was unnecessary), false on failure
bool swapProfileGroup(int targetGroup, GlobalConfig& config, const std::string& configPath) {
    if (targetGroup < 1 || targetGroup > 5) {
        std::cerr << "Invalid profile group: " << targetGroup << "\n";
        return false;
    }

    // If the target group is already active, no swap needed
    if (config.activeProfileGroup == targetGroup) {
        return true;
    }

    std::string sourcePath = config.polPath + "\\login_w." + std::to_string(targetGroup) + ".bin";
    std::string destPath = config.polPath + "\\login_w.bin";

    // Verify source file exists and is non-zero
    DWORD sourceAttrs = GetFileAttributesA(sourcePath.c_str());
    if (sourceAttrs == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Profile group file not found: " << sourcePath << "\n";
        std::cerr << "You may need to archive your current config first.\n";
        return false;
    }

    // Check file size
    HANDLE hFile = CreateFileA(sourcePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Cannot open profile group file: " << sourcePath << "\n";
        return false;
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0) {
        std::cerr << "Profile group file is empty: " << sourcePath << "\n";
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    // Perform the copy
    if (!CopyFileA(sourcePath.c_str(), destPath.c_str(), FALSE)) {
        DWORD err = GetLastError();
        std::cerr << "Failed to copy profile group file (error " << err << "): " << sourcePath << " -> " << destPath << "\n";
        return false;
    }

    config.activeProfileGroup = targetGroup;
    writeConfigFile(configPath, config);
    std::cout << "Profile group " << targetGroup << " loaded (login_w." << targetGroup << ".bin -> login_w.bin)\n";
    return true;
}

void launchAccount(const AccountConfig& account, const GlobalConfig& config) {
    // Determine port based on client region
    int port = [&]() {
        if (config.clientRegion == "JP") return 51300;
        if (config.clientRegion == "EU") return 51302;
        return 51304; // default (US)
        }();

    // Check if the port can be opened
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bool portAvailable = false;
    if (testSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket for port check.\n";
    } else {
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
        serverAddr.sin_port = htons(port);
        
        // Set socket options before bind
        int opt = 1;
        if (setsockopt(testSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
            std::cerr << "setsockopt SO_REUSEADDR failed\n";
        } else {
            // Try to set SO_EXCLUSIVEADDRUSE to false
            opt = 0;
            if (setsockopt(testSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&opt, sizeof(opt)) < 0) {
                std::cerr << "setsockopt SO_EXCLUSIVEADDRUSE failed\n";
            } else {
                if (bind(testSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                    DWORD error = WSAGetLastError();
                    std::cerr << "POL Redirect won't work: Port " << port << " is already in use (Error: " << error << ")\n";
                } else {
                    portAvailable = true;
                    if (config.POLProxy) {
                        addHostsEntry("127.0.0.1");
                        hostsEntryAdded = true;
                    }
                }
            }
        }
        closesocket(testSocket); // Make sure to close the test socket
    }

    std::cout << "Launching character: " << account.name << std::endl;

    // Defocus any existing POL window before launching new one
    defocusExistingPOL();

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    std::string baseDir = exePath;

    std::string exe = baseDir + "\\Windower.exe";
    bool isWindower = true;
    if (GetFileAttributesA(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        exe = baseDir + "\\pol.exe";
        isWindower = false;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cmdline = exe;
    std::string resolvedArgs = resolveArgs(account, config.defaults);
    if (isWindower && !resolvedArgs.empty()) {
        cmdline += " " + resolvedArgs;
    }

    if (!CreateProcessA(NULL, (LPSTR)cmdline.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to launch, try running as admin.\n";
        return;
    }

    // Convert username to wide string for window title
    std::wstring wUsername(account.name.begin(), account.name.end());
    HWND hwnd = nullptr;
    WindowSearchData searchData = { &wUsername, &hwnd };
    for (int i = 0; i < 60 && !hwnd; ++i) {
        EnumWindows(EnumWindowsProc, (LPARAM)&searchData);
        if (!hwnd) Sleep(500);
    }

    if (!hwnd) {
        std::cerr << "Could not find POL window\n";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return;
    }

    // Get the actual POL.exe path from the running process
    std::string polPath = getPOLPath(pi.dwProcessId);
    if (polPath.empty()) {
        // Try getting the path from the window
        char windowPath[MAX_PATH];
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            if (GetModuleFileNameExA(hProcess, NULL, windowPath, MAX_PATH)) {
                polPath = windowPath;
            }
            CloseHandle(hProcess);
        }
    }

    if (polPath.empty()) {
        std::cerr << "Could not find POL.exe path\n";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return;
    }

    // Get the directory containing POL.exe
    size_t lastSlash = polPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        polPath = polPath.substr(0, lastSlash);
    }

    // Read login_w.bin value
    int loginWValue = getLoginWValue(polPath);
    if (loginWValue == -1) {
        std::cerr << "Failed to read login_w.bin, using default slot selection\n";
    }

    // Wait for the window to have a title bar (WS_CAPTION)
    int waitTitleBar = 0;
    while (!(GetWindowLong(hwnd, GWL_STYLE) & WS_CAPTION) && waitTitleBar < 100) { // up to 10s
        Sleep(100);
        waitTitleBar++;
    }

    // Logging before BlockInput(TRUE)
    DWORD winPid = 0;
    GetWindowThreadProcessId(hwnd, &winPid);
    wchar_t winTitle[256] = {0};
    GetWindowTextW(hwnd, winTitle, 256);

    BlockInput(TRUE);

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    BringWindowToTop(hwnd);
    // Send VK_MENU (Alt) to help force focus
    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    Sleep(config.delay);
    // Extra focus before input
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    BringWindowToTop(hwnd);

    // Check for auto-login status
    std::ifstream loginWFile(polPath + "\\usr\\all\\login_w.bin", std::ios::binary);
    bool autoLoginEnabled = false;
    if (loginWFile) {
        loginWFile.seekg(0x6F);
        unsigned char autoLoginValue;
        loginWFile.read(reinterpret_cast<char*>(&autoLoginValue), 1);
        autoLoginEnabled = (autoLoginValue != 0x00);
        
        // If auto-login is enabled, check if the slot matches
        if (autoLoginEnabled && loginWValue != -1 && loginWValue != account.slot) {
            std::cout << "\nWARNING: Auto-login is enabled for slot " << loginWValue 
                      << " but you selected slot " << account.slot << ".\n  It's recommended to disable auto-login in POL.\n";

            autoLoginEnabled = false;

            // Press ESC to cancel auto-login
            simulateKey(VK_ESCAPE);
            Sleep(1300);
 
        }
    }

    // Adjust slot selection based on login_w.bin value
    if (loginWValue != -1) {
        int targetSlot = account.slot;
        if (targetSlot < loginWValue) {
            // If we want a lower slot than what's in the file, we need to press UP
            int upPresses = loginWValue - targetSlot;
            for (int i = 0; i < upPresses; ++i) {
                simulateKey(VK_UP);
                Sleep(200);
            }
        } else if (targetSlot > loginWValue) {
            // If we want a higher slot than what's in the file, we need to press DOWN
            int downPresses = targetSlot - loginWValue;
            for (int i = 0; i < downPresses; ++i) {
                simulateKey(VK_DOWN);
                Sleep(200);
            }
        }
    } else {
        // Fallback to original slot selection if we couldn't read login_w.bin
        if (account.slot > 1) {
            for (int i = 1; i < account.slot; ++i) {
                simulateKey(VK_DOWN);
                Sleep(200);
            }
        }
    }

    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    BringWindowToTop(hwnd);

    // Skip these returns if auto-login is enabled
    if (!autoLoginEnabled) {
    Sleep(200);
    simulateKey(VK_RETURN);
    Sleep(200);
    simulateKey(VK_RETURN);
    Sleep(300);
    simulateKey(VK_RETURN);
    }

    Sleep(200);
    simulateKey(VK_RETURN);
    Sleep(200);
    simulateKey(VK_RETURN);
    Sleep(300);
    simulateKey(VK_RETURN);
    Sleep(500);
    simulateKey(VK_RETURN);
    Sleep(500);

    // send some backspace keys just incase we press enter a few times on the 0 key
    simulateKey(VK_BACK);
    Sleep(25);
    simulateKey(VK_BACK);
    Sleep(25);
    simulateKey(VK_BACK);
    Sleep(25);
    simulateKey(VK_BACK);
    Sleep(25);

    sendText(hwnd, "a", 5);
    Sleep(25);
    simulateKey(VK_BACK);

    //sendText(hwnd, account.password, 5);
    copyAndPasteText(hwnd, resolvePassword(account, config.defaults));

    Sleep(300);

    simulateKey(VK_RETURN);
    Sleep(500);
    simulateKey(VK_DOWN);
    Sleep(300);

    if (!resolveTotpSecret(account, config.defaults).empty()) {
        simulateKey(VK_RETURN);
        std::string totp = generate_totp(resolveTotpSecret(account, config.defaults));
        sendText(hwnd, totp, 5);
        simulateKey(VK_ESCAPE);
        Sleep(100);
        simulateKey(VK_DOWN);
        Sleep(100);
    }

    simulateKey(VK_RETURN);
    Sleep(50);

    simulateKey(VK_RETURN);
    Sleep(500);

    BlockInput(FALSE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

std::atomic<bool> shouldExit(false);

// Global variable to store the port for the proxy server
std::atomic<int> proxyPort(51304);

// Console control handler for cleanup on termination (Ctrl+C, window close, etc.)
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT || 
        dwCtrlType == CTRL_BREAK_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || 
        dwCtrlType == CTRL_SHUTDOWN_EVENT) {
        shouldExit = true;
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        return TRUE;
    }
    return FALSE;
}

void startProxyServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Server failed to start\n";
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Port creation failed\n";
        WSACleanup();
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    // Set socket options before bind
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt SO_REUSEADDR failed\n";
        closesocket(serverSocket);
        WSACleanup();
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    // Try to set SO_EXCLUSIVEADDRUSE to false
    opt = 0;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt SO_EXCLUSIVEADDRUSE failed\n";
        closesocket(serverSocket);
        WSACleanup();
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(proxyPort.load());

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        DWORD error = WSAGetLastError();
        std::cerr << "Bind failed with error: " << error << " (";
        switch (error) {
            case WSAEADDRINUSE:
                std::cerr << "Port Address already in use";
                break;
            case WSAEACCES:
                std::cerr << "Port Access denied";
                break;
            case WSAEINVAL:
                std::cerr << "Port Invalid argument";
                break;
            default:
                std::cerr << "Port Unknown error";
        }
        std::cerr << ")\n";
        closesocket(serverSocket);
        WSACleanup();
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        // Clean up hosts entry if proxy fails to start
        if (hostsEntryAdded.load()) {
            removeHostsEntry();
            hostsEntryAdded = false;
        }
        shouldExit = true; // Signal main thread to exit
        return;
    }

    while (!shouldExit) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << "\n";
            continue;
        }
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            if (strstr(buffer, "GET /pml/main/index.pml") != nullptr) {
                std::string response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: text/x-playonline-pml;charset=UTF-8\r\n"
                                     "Content-Length: 123\r\n"
                                     "Connection: close\r\n"
                                     "\r\n"
                                     "<pml><head><meta http-equiv=\"Content-Type\" content=\"text/x-playonline-pml;charset=UTF-8\"><title>Fast</title></head><body><timer name=\"fast\" href=\"gameto:1\" enable=\"1\" delay=\"0\"></body></pml>";
                send(clientSocket, response.c_str(), response.length(), 0);
                shouldExit = true;
            } else {
                std::string response = "HTTP/1.1 404 Not Found\r\n"
                                     "Content-Type: text/plain\r\n"
                                     "Content-Length: 13\r\n"
                                     "Connection: close\r\n"
                                     "\r\n"
                                     "Not Found";
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }
        closesocket(clientSocket);
    }
    closesocket(serverSocket);
    WSACleanup();
    
    // Clean up hosts entry when proxy server exits
    if (hostsEntryAdded.load()) {
        removeHostsEntry();
        hostsEntryAdded = false;
    }
}

// Import existing archives — scan for login_w.{N}.bin files and assign accounts
void importArchives(GlobalConfig& config, const std::string& configPath) {
    std::string input;

    std::cout << "\n--- Import Existing Archives ---\n";
    std::cout << "This will let you assign account names to existing login_w.{N}.bin files.\n\n";

    bool anyFound = false;
    for (int i = 1; i <= 5; i++) {
        std::string archivePath = config.polPath + "\\login_w." + std::to_string(i) + ".bin";
        if (GetFileAttributesA(archivePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            continue; // Skip non-existent files
        }

        // Check if this group already has accounts assigned
        bool hasAccounts = false;
        for (const auto& acc : config.accounts) {
            if (acc.profileGroup == i) {
                hasAccounts = true;
                break;
            }
        }

        if (hasAccounts) {
            std::cout << "Archive " << i << ": already has accounts assigned. Skip.\n";
            continue;
        }

        anyFound = true;
        std::cout << "Archive " << i << " (login_w." << i << ".bin) found but has no accounts assigned.\n";
        std::cout << "  Set up accounts for this archive? (y/n): ";
        std::getline(std::cin, input);
        if (input != "y" && input != "Y") continue;

        std::cout << "  How many accounts in this archive? (1-4): ";
        std::getline(std::cin, input);
        if (input.empty() || !std::all_of(input.begin(), input.end(), ::isdigit)) continue;
        int numAccounts = std::stoi(input);
        if (numAccounts < 1 || numAccounts > 4) {
            std::cout << "  Must be 1-4. Skipping.\n";
            continue;
        }

        for (int a = 0; a < numAccounts; a++) {
            std::cout << "\n  Account " << (a + 1) << " in archive " << i << ":\n";
            AccountConfig newAcc;
            newAcc.profileGroup = i;

            std::cout << "    Character name: ";
            std::getline(std::cin, newAcc.name);
            if (newAcc.name.empty()) continue;

            // Check for existing account with same name
            bool exists = false;
            for (auto& acc : config.accounts) {
                if (_stricmp(acc.name.c_str(), newAcc.name.c_str()) == 0) {
                    acc.profileGroup = i;
                    std::cout << "    Account '" << newAcc.name << "' already exists. Updated to group " << i << ".\n";
                    // Still ask for slot
                    while (true) {
                        std::cout << "    POL slot (1-4): ";
                        std::getline(std::cin, input);
                        if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                            int slot = std::stoi(input);
                            if (slot >= 1 && slot <= 4) {
                                acc.slot = slot;
                                break;
                            }
                        }
                        std::cout << "    Must be 1-4.\n";
                    }
                    exists = true;
                    break;
                }
            }
            if (exists) continue;

            // New account — collect details
            while (true) {
                std::cout << "    POL slot (1-4): ";
                std::getline(std::cin, input);
                if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                    int slot = std::stoi(input);
                    if (slot >= 1 && slot <= 4) {
                        // Duplicate check
                        bool dup = false;
                        for (const auto& acc : config.accounts) {
                            if (acc.profileGroup == i && acc.slot == slot) {
                                dup = true;
                                break;
                            }
                        }
                        if (dup) {
                            std::cout << "    Slot " << slot << " already taken in group " << i << ".\n";
                            continue;
                        }
                        newAcc.slot = slot;
                        break;
                    }
                }
                std::cout << "    Must be 1-4.\n";
            }

            std::cout << "    Password: ";
            std::getline(std::cin, newAcc.password);
            std::cout << "    TOTP Secret (leave empty if not using): ";
            std::getline(std::cin, newAcc.totpSecret);
            newAcc.args = promptWindowerArgs();
            config.accounts.push_back(newAcc);
            std::cout << "    Added " << newAcc.name << " -> group " << i << " slot " << newAcc.slot << "\n";
        }
    }

    if (!anyFound) {
        std::cout << "No unassigned archives found. All existing login_w.{N}.bin files already have accounts.\n";
    }

    writeConfigFile(configPath, config);
    std::cout << "\nImport complete.\n";
}

// Archive management: backup current login_w.bin to a numbered slot
void archiveMenu(GlobalConfig& config, const std::string& configPath) {
    std::string input;

    std::cout << "\n========================================\n";
    std::cout << "  Archive Management\n";
    std::cout << "========================================\n";
    std::cout << "Archives store copies of login_w.bin as login_w.{1-5}.bin\n";
    std::cout << "Each archive holds up to 4 POL account slots.\n\n";

    // Show existing archives
    std::cout << "Existing archives:\n";
    bool anyArchiveExists = false;
    for (int i = 1; i <= 5; i++) {
        std::string archivePath = config.polPath + "\\login_w." + std::to_string(i) + ".bin";
        if (GetFileAttributesA(archivePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            anyArchiveExists = true;
            std::cout << "  [" << i << "] login_w." << i << ".bin - ";
            // Show which accounts are in this group
            bool first = true;
            for (const auto& acc : config.accounts) {
                if (acc.profileGroup == i) {
                    if (!first) std::cout << ", ";
                    std::cout << acc.name << " (slot " << acc.slot << ")";
                    first = false;
                }
            }
            if (first) std::cout << "(no accounts assigned)";
            std::cout << "\n";
        } else {
            std::cout << "  [" << i << "] login_w." << i << ".bin - (empty)\n";
        }
    }

    if (config.activeProfileGroup > 0) {
        std::cout << "\nCurrently active: profile group " << config.activeProfileGroup << "\n";
    }

    std::cout << "\nOptions:\n";
    std::cout << "  [S] Save current login_w.bin to an archive slot\n";
    std::cout << "  [I] Import existing archives (from MultiPOL or manual setup)\n";
    std::cout << "  [X] Back to main menu\n";
    std::cout << "Choice: ";
    std::getline(std::cin, input);
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);

    if (input == "x") return;

    if (input == "i") {
        importArchives(config, configPath);
        return;
    }

    if (input != "s") return;

    // Ask which slot to save to
    std::cout << "\nSave current login_w.bin to which slot (1-5)? ";
    std::getline(std::cin, input);
    if (input.empty() || !std::all_of(input.begin(), input.end(), ::isdigit)) {
        std::cout << "Invalid slot. Cancelled.\n";
        return;
    }
    int targetSlot = std::stoi(input);
    if (targetSlot < 1 || targetSlot > 5) {
        std::cout << "Slot must be 1-5. Cancelled.\n";
        return;
    }

    std::string destPath = config.polPath + "\\login_w." + std::to_string(targetSlot) + ".bin";
    std::string sourcePath = config.polPath + "\\login_w.bin";

    // Check if source exists
    if (GetFileAttributesA(sourcePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "No login_w.bin found at: " << sourcePath << "\n";
        return;
    }

    // Check if destination already exists — confirm overwrite
    if (GetFileAttributesA(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::cout << "Archive slot " << targetSlot << " already exists. Overwrite? (y/n): ";
        std::getline(std::cin, input);
        if (input != "y" && input != "Y") {
            std::cout << "Cancelled.\n";
            return;
        }
    }

    // Perform the copy
    if (!CopyFileA(sourcePath.c_str(), destPath.c_str(), FALSE)) {
        DWORD err = GetLastError();
        std::cerr << "Failed to copy (error " << err << "): " << sourcePath << " -> " << destPath << "\n";
        return;
    }

    std::cout << "Saved login_w.bin -> login_w." << targetSlot << ".bin\n";

    // Update active profile group
    config.activeProfileGroup = targetSlot;
    writeConfigFile(configPath, config);

    // Ask about slot assignments for accounts in this group
    std::cout << "\nWould you like to assign accounts to this archive group now? (y/n): ";
    std::getline(std::cin, input);
    if (input != "y" && input != "Y") return;

    std::cout << "\nTip: Slots should ideally be sequential (1,2,3,4) but you can use any combination.\n";
    std::cout << "How many accounts are in this archive? (1-4): ";
    std::getline(std::cin, input);
    if (input.empty() || !std::all_of(input.begin(), input.end(), ::isdigit)) return;
    int numAccounts = std::stoi(input);
    if (numAccounts < 1 || numAccounts > 4) {
        std::cout << "Must be 1-4.\n";
        return;
    }

    for (int i = 0; i < numAccounts; i++) {
        std::cout << "\nAccount " << (i + 1) << " in archive " << targetSlot << ":\n";

        // Find or create the account
        std::cout << "  Character name: ";
        std::string name;
        std::getline(std::cin, name);
        if (name.empty()) continue;

        // Check if account already exists
        AccountConfig* existing = nullptr;
        for (auto& acc : config.accounts) {
            if (_stricmp(acc.name.c_str(), name.c_str()) == 0) {
                existing = &acc;
                break;
            }
        }

        int slot = 0;
        while (true) {
            std::cout << "  POL slot within this archive (1-4): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                slot = std::stoi(input);
                if (slot >= 1 && slot <= 4) {
                    // Check for duplicate in same group
                    bool duplicate = false;
                    for (const auto& acc : config.accounts) {
                        if (&acc == existing) continue;
                        if (acc.profileGroup == targetSlot && acc.slot == slot) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (duplicate) {
                        std::cout << "  Slot " << slot << " already taken in group " << targetSlot << ". Try again.\n";
                        continue;
                    }
                    break;
                }
            }
            std::cout << "  Must be 1-4. Try again.\n";
        }

        if (existing) {
            existing->profileGroup = targetSlot;
            existing->slot = slot;
            std::cout << "  Updated " << name << " -> group " << targetSlot << " slot " << slot << "\n";
        } else {
            // Create new account entry
            AccountConfig newAcc;
            newAcc.name = name;
            newAcc.profileGroup = targetSlot;
            newAcc.slot = slot;
            std::cout << "  Password: ";
            std::getline(std::cin, newAcc.password);
            std::cout << "  TOTP Secret (leave empty if not using): ";
            std::getline(std::cin, newAcc.totpSecret);
            newAcc.args = promptWindowerArgs();
            config.accounts.push_back(newAcc);
            std::cout << "  Added " << name << " -> group " << targetSlot << " slot " << slot << "\n";
        }
    }

    writeConfigFile(configPath, config);
    std::cout << "\nArchive setup complete.\n";
}

bool editConfig(GlobalConfig& config) {
    std::string input;
    while (true) {
        std::cout << "\nEdit Configuration Menu:\n";
        std::cout << "  [E] Edit existing character\n";
        std::cout << "  [A] Add new character\n";
        std::cout << "  [D] Delete character\n";
        std::cout << "  [F] Set account defaults (shared password/TOTP/args)\n";
        std::cout << "  [C] Modify timeout\n";
        std::cout << "  [R] Change client region (US/JP)\n";
        std::cout << "  [X] Exit to selection screen\n";
        std::cout << "Enter option: ";
        std::getline(std::cin, input);
        // Convert input to lowercase
        std::transform(input.begin(), input.end(), input.begin(), ::tolower);
        if (input == "e") {
            if (config.accounts.empty()) {
                std::cout << "No characters to edit.\n";
                continue;
            }
            std::cout << "\nSelect a character to edit:\n";
            for (size_t i = 0; i < config.accounts.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << config.accounts[i].name << " (slot " << config.accounts[i].slot << ")\n";
            }
            std::cout << "Enter number (1-" << config.accounts.size() << "): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                int choice = std::stoi(input);
                if (choice >= 1 && (size_t)choice <= config.accounts.size()) {
                    AccountConfig& acc = config.accounts[choice - 1];
                    std::cout << "Editing character: " << acc.name << "\n";
                    std::cout << "Current name: " << acc.name << "\n";
                    std::cout << "New name (leave empty to keep current): ";
                    std::getline(std::cin, input);
                    if (!input.empty()) {
                        acc.name = input;
                    }
                    std::cout << "Current password: " << acc.password << "\n";
                    std::cout << "New password (leave empty to keep current): ";
                    std::getline(std::cin, input);
                    if (!input.empty()) {
                        acc.password = input;
                    }
                    std::cout << "Current TOTP secret: " << acc.totpSecret << "\n";
                    std::cout << "New TOTP secret (leave empty to keep current): ";
                    std::getline(std::cin, input);
                    if (!input.empty()) {
                        acc.totpSecret = input;
                    }
                    std::cout << "Current slot: " << acc.slot << "\n";
                    std::cout << "New slot (1-4, leave empty to keep current): ";
                    std::getline(std::cin, input);
                    if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                        int slot = std::stoi(input);
                        if (slot >= 1 && slot <= 4) {
                            acc.slot = slot;
                        }
                    }
                    std::cout << "Current Windower arguments: " << acc.args << "\n";
                    acc.args = promptWindowerArgs(acc.args);
                    std::cout << "Character updated.\n";
                }
            }
        } else if (input == "a") {
            AccountConfig newAcc;
            std::cout << "\nAdding new character:\n";
            std::cout << "Character name (no spaces, unique): ";
            std::getline(std::cin, newAcc.name);
            std::cout << "Password: ";
            std::getline(std::cin, newAcc.password);
            std::cout << "TOTP Secret (leave empty if not using): ";
            std::getline(std::cin, newAcc.totpSecret);
            std::cout << "POL Slot number (1-4): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                int slot = std::stoi(input);
                if (slot >= 1 && slot <= 4) {
                    newAcc.slot = slot;
                }
            }
            newAcc.args = promptWindowerArgs();
            config.accounts.push_back(newAcc);
            std::cout << "New character added.\n";
        } else if (input == "d") {
            if (config.accounts.empty()) {
                std::cout << "No characters to delete.\n";
                continue;
            }
            std::cout << "\nSelect a character to delete:\n";
            for (size_t i = 0; i < config.accounts.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << config.accounts[i].name << " (slot " << config.accounts[i].slot << ")\n";
            }
            std::cout << "Enter number (1-" << config.accounts.size() << "): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                int choice = std::stoi(input);
                if (choice >= 1 && (size_t)choice <= config.accounts.size()) {
                    std::cout << "Are you sure you want to delete " << config.accounts[choice - 1].name << "? (y/n): ";
                    std::getline(std::cin, input);
                    if (input == "y" || input == "Y") {
                        config.accounts.erase(config.accounts.begin() + choice - 1);
                        std::cout << "Character deleted.\n";
                    }
                }
            }
        } else if (input == "f") {
            std::cout << "\n--- Account Defaults ---\n";
            std::cout << "These values are used when an account's field is left empty.\n\n";
            std::cout << "Current default password: " << (config.defaults.password.empty() ? "(none)" : "****") << "\n";
            std::cout << "New default password (leave empty to keep, 'clear' to remove): ";
            std::getline(std::cin, input);
            if (input == "clear") {
                config.defaults.password = "";
            } else if (!input.empty()) {
                config.defaults.password = input;
            }
            std::cout << "Current default TOTP: " << (config.defaults.totpSecret.empty() ? "(none)" : "****") << "\n";
            std::cout << "New default TOTP (leave empty to keep, 'clear' to remove): ";
            std::getline(std::cin, input);
            if (input == "clear") {
                config.defaults.totpSecret = "";
            } else if (!input.empty()) {
                config.defaults.totpSecret = input;
            }
            std::cout << "Current default Windower args: " << (config.defaults.args.empty() ? "(none)" : config.defaults.args) << "\n";
            config.defaults.args = promptWindowerArgs(config.defaults.args);
            std::cout << "Defaults updated.\n";
        } else if (input == "c") {
            std::cout << "Current timeout: " << config.delay / 1000 << " seconds\n";
            std::cout << "New timeout (in seconds, 1-20): ";
            std::getline(std::cin, input);
            if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
                int val = std::stoi(input);
                if (val >= 1 && val <= 20) {
                    config.delay = val * 1000;
                    std::cout << "Timeout updated.\n";
                }
            }
        } else if (input == "r") {
            std::cout << "Current client region: " << config.clientRegion << "\n";
            std::cout << "New client region (US/JP): ";
            std::getline(std::cin, input);
            std::transform(input.begin(), input.end(), input.begin(), ::toupper);
            if (input == "US" || input == "JP") {
                config.clientRegion = input;
                std::cout << "Client region updated to " << config.clientRegion << ".\n";
            } else {
                std::cout << "Invalid region. Please enter 'US' or 'JP'.\n";
            }
        } else if (input == "x") {
            return false; // Return to selection screen
        } else {
            std::cout << "Invalid option. Try again.\n";
        }
    }
    return true; // Exit the app
}

// Add back the removeHostsEntry function
void removeHostsEntry() {
    const char* path = "C:\\Windows\\System32\\drivers\\etc\\hosts";
    const char* tmpPath = "C:\\Windows\\System32\\drivers\\etc\\hosts.tmp";

    std::ifstream in(path);
    std::ofstream out(tmpPath);

    std::string line;
    while (std::getline(in, line)) {
        // Trim leading/trailing whitespace
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        // Skip blank or matching lines
        if (trimmed.empty() || trimmed.find("#ffxi-autologin") != std::string::npos)
            continue;

        out << line << "\n";
    }

    in.close();
    out.close();

    DeleteFileA(path);
    MoveFileA(tmpPath, path);
}

// Update main to remove hosts entry before exiting
int main(int argc, char* argv[]) {
    // Check for admin privileges and request elevation if needed
    {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        if (!isAdmin) {
            // Re-launch ourselves with elevation
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);

            // Rebuild command line arguments
            std::string args;
            for (int i = 1; i < argc; i++) {
                if (i > 1) args += " ";
                std::string arg = argv[i];
                if (arg.find(' ') != std::string::npos) {
                    args += "\"" + arg + "\"";
                } else {
                    args += arg;
                }
            }

            SHELLEXECUTEINFOA sei = { sizeof(sei) };
            sei.lpVerb = "runas";
            sei.lpFile = exePath;
            sei.lpParameters = args.c_str();
            sei.nShow = SW_SHOWNORMAL;

            if (ShellExecuteExA(&sei)) {
                return 0; // Original process exits, elevated one takes over
            } else {
                std::cerr << "This application requires administrator privileges.\n";
                std::cerr << "Please right-click and select 'Run as administrator'.\n";
                return 1;
            }
        }
    }

    std::cout << "Original version by: jaku | https://github.com/jaku/FFXI-autoPOL\n";
    std::cout << "Fork by: Makaria       | https://github.com/alicestellar/MakariaAutoPOLFork\n";
    std::cout << "Version: 3.0.0\n";
    DEBUG_KEY_PRESSES = false;
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--debug-keys") {
            DEBUG_KEY_PRESSES = true;
            std::cout << "Key press logging enabled\n";
        }
    }
    
    // Register console control handler to clean up on termination
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    std::string baseDir = exePath;
    std::string configPath = baseDir + "\\config.json";
    GlobalConfig config = loadConfig(configPath);
    bool setupMode = false;
    bool editMode = false;
    std::string characterName;
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--setup") {
            setupMode = true;
        } else if (arg == "--edit") {
            editMode = true;
        } else if (arg == "--character" && i + 1 < argc) {
            characterName = argv[++i];
        } else if (arg == "--no-proxy") {
            config.POLProxy = false;
        } else if (arg == "--windower-arg" && i + 1 < argc) {
            for (auto& acc : config.accounts) {
                if (_stricmp(acc.name.c_str(), characterName.c_str()) == 0) {
                    acc.args = argv[++i];
                    break;
                }
            }
        }
    }

    // Clean up any existing hosts file entries at startup
    if (config.POLProxy) {
        removeHostsEntry();
    }

    // POL Path Discovery: ensure we have a valid polPath
    if (config.polPath.empty() || GetFileAttributesA(config.polPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!config.polPath.empty()) {
            std::cout << "Stored POL path is no longer valid: " << config.polPath << "\n";
            std::cout << "Re-running discovery...\n";
        }
        config.polPath = discoverPolPath();
        writeConfigFile(configPath, config);
    }

    // Migrate legacy configs without profileGroup assignments
    if (!config.accounts.empty() && migrateConfig(config)) {
        writeConfigFile(configPath, config);
    }

    if (setupMode || config.accounts.empty()) {
        setupConfig(config);
        writeConfigFile(configPath, config);
        std::cout << "Setup complete. Exiting.\n";
        return 0;
    }
    if (editMode) {
        std::cout << "Editing configuration...\n";
        editConfig(config);
        writeConfigFile(configPath, config);
        std::cout << "Configuration updated. Exiting.\n";
        return 0;
    }
    if (config.accounts.empty()) {
        std::cout << "No accounts configured. Please run with --setup to configure accounts.\n";
        return 1;
    }

    // If --character was specified on command line, skip the menu
    if (!characterName.empty()) {
        AccountConfig* toLaunch = nullptr;
        for (auto& acc : config.accounts) {
            if (_stricmp(acc.name.c_str(), characterName.c_str()) == 0) { toLaunch = &acc; break; }
        }
        if (!toLaunch) {
            std::cout << "No account found for character: " << characterName << "\n";
            return 1;
        }
        if (toLaunch->profileGroup > 0) {
            if (!swapProfileGroup(toLaunch->profileGroup, config, configPath)) {
                std::cerr << "Profile swap failed. Aborting launch.\n";
                return 1;
            }
        }
        if (!config.POLProxy) {
            launchAccount(*toLaunch, config);
            return 0;
        }
        proxyPort = (config.clientRegion == "JP") ? 51300 : 51304;
        std::thread proxyThread(startProxyServer);
        launchAccount(*toLaunch, config);
        while (!shouldExit) { Sleep(100); }
        proxyThread.join();
        if (hostsEntryAdded.load()) { removeHostsEntry(); hostsEntryAdded = false; }
        return 0;
    }

    // Main menu loop — Alliance Selection UI
    while (true) {
        std::cout << "\n===================================================\n";
        std::cout << "           FFXI autoPOL - Alliance Launcher\n";
        std::cout << "===================================================\n";

        // Display accounts grouped by party
        // Party 1 = accounts 1-6, Party 2 = 7-12, Party 3 = 13-18 (display position)
        std::cout << "\n  Characters:\n";
        for (size_t i = 0; i < config.accounts.size(); ++i) {
            if (i == 6 || i == 12) {
                std::cout << "  ---\n"; // Party separator
            }
            std::string partyLabel;
            if (i < 6) partyLabel = "P1";
            else if (i < 12) partyLabel = "P2";
            else partyLabel = "P3";

            std::cout << "  [" << (i + 1) << "] " << partyLabel << " | "
                      << config.accounts[i].name
                      << " (group " << config.accounts[i].profileGroup
                      << " slot " << config.accounts[i].slot << ")\n";
        }

        std::cout << "\n  Options:\n";
        std::cout << "  [M] Launch multiple (enter numbers separated by commas)\n";
        // Show party presets only if enough accounts exist
        if (config.accounts.size() >= 6)
            std::cout << "  [P1] Launch Party 1 (chars 1-6)\n";
        if (config.accounts.size() >= 12)
            std::cout << "  [P2] Launch Party 2 (chars 7-12)\n";
        if (config.accounts.size() >= 18)
            std::cout << "  [P3] Launch Party 3 (chars 13-18)\n";
        if (config.accounts.size() >= 12)
            std::cout << "  [A]  Launch Full Alliance (all characters)\n";
        // Show saved presets
        if (!config.presets.empty()) {
            std::cout << "  --- Saved Presets ---\n";
            for (size_t pi = 0; pi < config.presets.size(); pi++) {
                std::cout << "  [" << (pi + 1) << "p] " << config.presets[pi].name << " (";
                for (size_t ai = 0; ai < config.presets[pi].accountIndices.size(); ai++) {
                    int idx = config.presets[pi].accountIndices[ai];
                    if (idx >= 1 && (size_t)idx <= config.accounts.size()) {
                        if (ai > 0) std::cout << ", ";
                        std::cout << config.accounts[idx - 1].name;
                    }
                }
                std::cout << ")\n";
            }
        }
        std::cout << "  [S] Manage saved presets\n";
        std::cout << "  [E] Edit configuration\n";
        std::cout << "  [B] Backup/Archive login_w.bin\n";
        std::cout << "  [I] Import existing archives\n";
        std::cout << "  [Q] Exit\n";
        std::cout << "===================================================\n";
        std::cout << "Select character number, preset, or option letter: ";

        std::string input;
        std::getline(std::cin, input);
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

        // Handle menu options
        if (lowerInput == "q") {
            std::cout << "Exiting.\n";
            return 0;
        }
        if (lowerInput == "e") {
            if (!editConfig(config)) {
                writeConfigFile(configPath, config);
                config = loadConfig(configPath);
            } else {
                writeConfigFile(configPath, config);
            }
            continue;
        }
        if (lowerInput == "b") {
            archiveMenu(config, configPath);
            continue;
        }
        if (lowerInput == "i") {
            importArchives(config, configPath);
            continue;
        }
        if (lowerInput == "s") {
            // Manage saved presets
            std::string presetInput;
            std::cout << "\n--- Saved Presets ---\n";
            std::cout << "  [C] Create new preset\n";
            std::cout << "  [D] Delete a preset\n";
            std::cout << "  [X] Back\n";
            std::cout << "Choice: ";
            std::getline(std::cin, presetInput);
            std::transform(presetInput.begin(), presetInput.end(), presetInput.begin(), ::tolower);

            if (presetInput == "c") {
                SavedPreset newPreset;
                std::cout << "Preset name: ";
                std::getline(std::cin, newPreset.name);
                if (newPreset.name.empty()) { std::cout << "Cancelled.\n"; continue; }
                std::cout << "Enter character numbers separated by commas (e.g. 1,3,7): ";
                std::getline(std::cin, presetInput);
                std::stringstream pss(presetInput);
                std::string ptoken;
                while (std::getline(pss, ptoken, ',')) {
                    ptoken.erase(0, ptoken.find_first_not_of(" \t"));
                    ptoken.erase(ptoken.find_last_not_of(" \t") + 1);
                    if (!ptoken.empty() && std::all_of(ptoken.begin(), ptoken.end(), ::isdigit)) {
                        int idx = std::stoi(ptoken);
                        if (idx >= 1 && (size_t)idx <= config.accounts.size()) {
                            newPreset.accountIndices.push_back(idx);
                        }
                    }
                }
                if (newPreset.accountIndices.empty()) {
                    std::cout << "No valid characters. Cancelled.\n";
                } else {
                    config.presets.push_back(newPreset);
                    writeConfigFile(configPath, config);
                    std::cout << "Preset '" << newPreset.name << "' saved with " << newPreset.accountIndices.size() << " character(s).\n";
                }
            } else if (presetInput == "d") {
                if (config.presets.empty()) {
                    std::cout << "No presets to delete.\n";
                } else {
                    for (size_t pi = 0; pi < config.presets.size(); pi++) {
                        std::cout << "  [" << (pi + 1) << "] " << config.presets[pi].name << "\n";
                    }
                    std::cout << "Delete which? ";
                    std::getline(std::cin, presetInput);
                    if (!presetInput.empty() && std::all_of(presetInput.begin(), presetInput.end(), ::isdigit)) {
                        int idx = std::stoi(presetInput);
                        if (idx >= 1 && (size_t)idx <= config.presets.size()) {
                            std::cout << "Delete '" << config.presets[idx - 1].name << "'? (y/n): ";
                            std::getline(std::cin, presetInput);
                            if (presetInput == "y" || presetInput == "Y") {
                                config.presets.erase(config.presets.begin() + idx - 1);
                                writeConfigFile(configPath, config);
                                std::cout << "Deleted.\n";
                            }
                        }
                    }
                }
            }
            continue;
        }

        // Check for saved preset launch (e.g., "1p", "2p", "3p")
        if (lowerInput.size() >= 2 && lowerInput.back() == 'p') {
            std::string numPart = lowerInput.substr(0, lowerInput.size() - 1);
            if (!numPart.empty() && std::all_of(numPart.begin(), numPart.end(), ::isdigit)) {
                int presetIdx = std::stoi(numPart);
                if (presetIdx >= 1 && (size_t)presetIdx <= config.presets.size()) {
                    SavedPreset& preset = config.presets[presetIdx - 1];
                    std::vector<AccountConfig*> launchQueue;
                    for (int idx : preset.accountIndices) {
                        if (idx >= 1 && (size_t)idx <= config.accounts.size()) {
                            launchQueue.push_back(&config.accounts[idx - 1]);
                        }
                    }
                    if (!launchQueue.empty()) {
                        std::cout << "\nLaunching preset '" << preset.name << "' (" << launchQueue.size() << " chars)...\n";
                        for (size_t qi = 0; qi < launchQueue.size(); qi++) {
                            AccountConfig* acc = launchQueue[qi];
                            if (acc->profileGroup > 0) {
                                if (!swapProfileGroup(acc->profileGroup, config, configPath)) {
                                    std::cerr << "Profile swap failed for " << acc->name << ". Skipping.\n";
                                    continue;
                                }
                            }
                            std::cout << "\n--- [" << (qi + 1) << "/" << launchQueue.size() << "] Launching: " << acc->name << " ---\n";
                            if (!config.POLProxy) {
                                launchAccount(*acc, config);
                            } else {
                                shouldExit = false;
                                proxyPort = (config.clientRegion == "JP") ? 51300 : 51304;
                                std::thread proxyThread(startProxyServer);
                                launchAccount(*acc, config);
                                while (!shouldExit) { Sleep(100); }
                                proxyThread.join();
                                if (hostsEntryAdded.load()) { removeHostsEntry(); hostsEntryAdded = false; }
                            }
                            if (qi < launchQueue.size() - 1) {
                                std::cout << "\n[CHECKPOINT] " << acc->name << " launched.\n";
                                std::cout << "Press Enter when ready for next character...";
                                std::string dummy;
                                std::getline(std::cin, dummy);
                            }
                        }
                        std::cout << "\nPreset '" << preset.name << "' complete. Returning to menu...\n";
                    }
                    continue;
                }
            }
        }

        // Party/Alliance presets — build queue and fall through to sequential launch
        std::vector<AccountConfig*> launchQueue;
        bool isPreset = false;

        if (lowerInput == "p1" && config.accounts.size() >= 6) {
            for (size_t i = 0; i < 6; i++) launchQueue.push_back(&config.accounts[i]);
            isPreset = true;
        } else if (lowerInput == "p2" && config.accounts.size() >= 12) {
            for (size_t i = 6; i < 12; i++) launchQueue.push_back(&config.accounts[i]);
            isPreset = true;
        } else if (lowerInput == "p3" && config.accounts.size() >= 18) {
            for (size_t i = 12; i < 18; i++) launchQueue.push_back(&config.accounts[i]);
            isPreset = true;
        } else if (lowerInput == "a" && config.accounts.size() >= 2) {
            for (size_t i = 0; i < config.accounts.size(); i++) launchQueue.push_back(&config.accounts[i]);
            isPreset = true;
        }

        if (isPreset) {
            // Execute the sequential launch for the preset
            std::cout << "\nLaunching " << launchQueue.size() << " character(s) sequentially...\n";
            for (size_t qi = 0; qi < launchQueue.size(); qi++) {
                AccountConfig* acc = launchQueue[qi];
                if (acc->profileGroup > 0) {
                    if (!swapProfileGroup(acc->profileGroup, config, configPath)) {
                        std::cerr << "Profile swap failed for " << acc->name << ". Skipping.\n";
                        continue;
                    }
                }
                std::cout << "\n--- [" << (qi + 1) << "/" << launchQueue.size() << "] Launching: " << acc->name << " ---\n";
                if (!config.POLProxy) {
                    launchAccount(*acc, config);
                } else {
                    shouldExit = false;
                    proxyPort = (config.clientRegion == "JP") ? 51300 : 51304;
                    std::thread proxyThread(startProxyServer);
                    launchAccount(*acc, config);
                    while (!shouldExit) { Sleep(100); }
                    proxyThread.join();
                    if (hostsEntryAdded.load()) { removeHostsEntry(); hostsEntryAdded = false; }
                }
                if (qi < launchQueue.size() - 1) {
                    std::cout << "\n[CHECKPOINT] " << acc->name << " launched.\n";
                    std::cout << "Press Enter when ready for next character...";
                    std::string dummy;
                    std::getline(std::cin, dummy);
                }
            }
            std::cout << "\nAll " << launchQueue.size() << " character(s) launched. Returning to menu...\n";
            continue;
        }

        // Multi-launch mode
        if (lowerInput == "m") {
            std::cout << "Enter character numbers separated by commas (e.g. 1,3,5): ";
            std::getline(std::cin, input);

            // Parse comma-separated numbers into a launch queue
            std::vector<AccountConfig*> launchQueue;
            std::stringstream ss(input);
            std::string token;
            while (std::getline(ss, token, ',')) {
                // Trim whitespace
                token.erase(0, token.find_first_not_of(" \t"));
                token.erase(token.find_last_not_of(" \t") + 1);
                if (!token.empty() && std::all_of(token.begin(), token.end(), ::isdigit)) {
                    int idx = std::stoi(token);
                    if (idx >= 1 && (size_t)idx <= config.accounts.size()) {
                        launchQueue.push_back(&config.accounts[idx - 1]);
                    } else {
                        std::cout << "Skipping invalid number: " << idx << "\n";
                    }
                }
            }

            if (launchQueue.empty()) {
                std::cout << "No valid characters selected.\n";
                continue;
            }

            // Sequential launch loop
            std::cout << "\nLaunching " << launchQueue.size() << " character(s) sequentially...\n";
            for (size_t qi = 0; qi < launchQueue.size(); qi++) {
                AccountConfig* acc = launchQueue[qi];

                // Swap profile group if needed
                if (acc->profileGroup > 0) {
                    if (!swapProfileGroup(acc->profileGroup, config, configPath)) {
                        std::cerr << "Profile swap failed for " << acc->name << ". Skipping.\n";
                        continue;
                    }
                }

                std::cout << "\n--- [" << (qi + 1) << "/" << launchQueue.size() << "] Launching: " << acc->name << " ---\n";

                if (!config.POLProxy) {
                    launchAccount(*acc, config);
                } else {
                    shouldExit = false;
                    proxyPort = (config.clientRegion == "JP") ? 51300 : 51304;
                    std::thread proxyThread(startProxyServer);
                    launchAccount(*acc, config);
                    while (!shouldExit) { Sleep(100); }
                    proxyThread.join();
                    if (hostsEntryAdded.load()) { removeHostsEntry(); hostsEntryAdded = false; }
                }

                // Pause between launches (except after the last one)
                if (qi < launchQueue.size() - 1) {
                    std::cout << "\n[CHECKPOINT] " << acc->name << " launched.\n";
                    std::cout << "Press Enter when ready for next character...";
                    std::getline(std::cin, input);
                }
            }

            std::cout << "\nAll " << launchQueue.size() << " character(s) launched. Returning to menu...\n";
            continue;
        }

        // Try to parse as a number (single account selection)
        if (!input.empty() && std::all_of(input.begin(), input.end(), ::isdigit)) {
            int choice = std::stoi(input);
            if (choice >= 1 && (size_t)choice <= config.accounts.size()) {
                AccountConfig* toLaunch = &config.accounts[choice - 1];

                // Swap profile group if needed
                if (toLaunch->profileGroup > 0) {
                    if (!swapProfileGroup(toLaunch->profileGroup, config, configPath)) {
                        std::cerr << "Profile swap failed. Aborting launch.\n";
                        continue;
                    }
                }

                std::cout << "Launching: " << toLaunch->name << "\n";

                if (!config.POLProxy) {
                    launchAccount(*toLaunch, config);
                } else {
                    shouldExit = false;
                    proxyPort = (config.clientRegion == "JP") ? 51300 : 51304;
                    std::thread proxyThread(startProxyServer);
                    launchAccount(*toLaunch, config);
                    while (!shouldExit) { Sleep(100); }
                    proxyThread.join();
                    if (hostsEntryAdded.load()) { removeHostsEntry(); hostsEntryAdded = false; }
                }

                std::cout << "\nLaunch complete. Returning to menu...\n";
                continue; // Return to main menu
            }
        }

        std::cout << "Invalid selection. Try again.\n";
    }
    return 0;
}
