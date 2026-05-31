/**
 * Algorithm Flight Recorder — C++ Engine
 * Compiles to WebAssembly via Emscripten.
 *
 * Build command (requires Emscripten SDK):
 *   emcc engine.cpp -o ../public/engine.js \
 *     -s WASM=1 \
 *     -s EXPORTED_FUNCTIONS='["_run_algorithm","_get_steps_json","_get_history_json","_free_result","_malloc","_free"]' \
 *     -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
 *     -s ALLOW_MEMORY_GROWTH=1 \
 *     -s MODULARIZE=1 \
 *     -s EXPORT_NAME="AlgoEngine" \
 *     --embed-file recorder.db \
 *     -lidbfs.js \
 *     -O2
 *
 * SQLite is bundled via Emscripten's port:
 *   Add flag: -sUSE_SQLITE3
 */

#include <emscripten/emscripten.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <functional>

// ─── Data Structures ──────────────────────────────────────────────────────────

struct Step {
    std::vector<int> array;   // full array snapshot at this moment
    int  indexA   = -1;       // primary active index (red highlight)
    int  indexB   = -1;       // secondary active index (yellow highlight)
    std::string action;       // human-readable description
    std::string phase;        // "compare" | "swap" | "pivot" | "merge" | "done"
};

static std::vector<Step> g_steps;
static sqlite3*           g_db = nullptr;

// ─── SQLite Helpers ───────────────────────────────────────────────────────────

static void db_init() {
    if (g_db) return;
    sqlite3_open(":memory:", &g_db);
    const char* sql =
        "CREATE TABLE IF NOT EXISTS history ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT DEFAULT (datetime('now')),"
        "  algorithm TEXT,"
        "  input_size INTEGER,"
        "  total_steps INTEGER,"
        "  comparisons INTEGER,"
        "  swaps       INTEGER"
        ");";
    sqlite3_exec(g_db, sql, nullptr, nullptr, nullptr);
}

static void db_log(const std::string& algo, int size, int steps, int cmps, int swaps) {
    db_init();
    std::string sql =
        "INSERT INTO history (algorithm, input_size, total_steps, comparisons, swaps) "
        "VALUES ('" + algo + "'," +
        std::to_string(size)  + "," +
        std::to_string(steps) + "," +
        std::to_string(cmps)  + "," +
        std::to_string(swaps) + ");";
    sqlite3_exec(g_db, sql.c_str(), nullptr, nullptr, nullptr);
}

// ─── JSON Helpers ─────────────────────────────────────────────────────────────

static std::string array_to_json(const std::vector<int>& arr) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) oss << ",";
        oss << arr[i];
    }
    oss << "]";
    return oss.str();
}

static std::string escape_json(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else           out += c;
    }
    return out;
}

static std::string steps_to_json() {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < g_steps.size(); ++i) {
        const Step& s = g_steps[i];
        if (i) oss << ",";
        oss << "{"
            << "\"array\":"  << array_to_json(s.array)  << ","
            << "\"a\":"      << s.indexA                << ","
            << "\"b\":"      << s.indexB                << ","
            << "\"action\":\"" << escape_json(s.action) << "\","
            << "\"phase\":\"" << escape_json(s.phase)   << "\""
            << "}";
    }
    oss << "]";
    return oss.str();
}

// ─── Step Recording ───────────────────────────────────────────────────────────

static void record(const std::vector<int>& arr, int a, int b,
                   const std::string& action, const std::string& phase) {
    g_steps.push_back({arr, a, b, action, phase});
}

// ─── Algorithms ───────────────────────────────────────────────────────────────

// Bubble Sort ─────────────────────────────────────────────────────────────────
static void bubble_sort(std::vector<int> arr) {
    int n = arr.size();
    for (int i = 0; i < n - 1; ++i) {
        bool swapped = false;
        for (int j = 0; j < n - i - 1; ++j) {
            record(arr, j, j+1,
                   "Comparing [" + std::to_string(arr[j]) + "] vs [" + std::to_string(arr[j+1]) + "]",
                   "compare");
            if (arr[j] > arr[j+1]) {
                std::swap(arr[j], arr[j+1]);
                swapped = true;
                record(arr, j, j+1,
                       "Swapped → " + std::to_string(arr[j]) + " ↔ " + std::to_string(arr[j+1]),
                       "swap");
            }
        }
        if (!swapped) break;
    }
    record(arr, -1, -1, "Array is sorted!", "done");
}

// Selection Sort ──────────────────────────────────────────────────────────────
static void selection_sort(std::vector<int> arr) {
    int n = arr.size();
    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        record(arr, i, -1, "Seeking minimum from index " + std::to_string(i), "pivot");
        for (int j = i + 1; j < n; ++j) {
            record(arr, minIdx, j,
                   "Is [" + std::to_string(arr[j]) + "] < current min [" + std::to_string(arr[minIdx]) + "]?",
                   "compare");
            if (arr[j] < arr[minIdx]) {
                minIdx = j;
                record(arr, minIdx, -1, "New minimum found: " + std::to_string(arr[minIdx]), "pivot");
            }
        }
        if (minIdx != i) {
            std::swap(arr[i], arr[minIdx]);
            record(arr, i, minIdx, "Placed minimum " + std::to_string(arr[i]) + " at position " + std::to_string(i), "swap");
        }
    }
    record(arr, -1, -1, "Array is sorted!", "done");
}

// Insertion Sort ──────────────────────────────────────────────────────────────
static void insertion_sort(std::vector<int> arr) {
    int n = arr.size();
    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        int j   = i - 1;
        record(arr, i, -1, "Inserting [" + std::to_string(key) + "] into sorted region", "pivot");
        while (j >= 0 && arr[j] > key) {
            record(arr, j, j+1, "Shifting [" + std::to_string(arr[j]) + "] right", "compare");
            arr[j+1] = arr[j];
            record(arr, j, j+1, "Shifted → position " + std::to_string(j+1), "swap");
            --j;
        }
        arr[j+1] = key;
        record(arr, j+1, -1, "Inserted [" + std::to_string(key) + "] at position " + std::to_string(j+1), "swap");
    }
    record(arr, -1, -1, "Array is sorted!", "done");
}

// Merge Sort ──────────────────────────────────────────────────────────────────
static std::vector<int> g_merge_arr; // shared reference for recording

static void merge(int l, int m, int r) {
    std::vector<int> left(g_merge_arr.begin()+l, g_merge_arr.begin()+m+1);
    std::vector<int> right(g_merge_arr.begin()+m+1, g_merge_arr.begin()+r+1);
    int i=0, j=0, k=l;
    while (i<(int)left.size() && j<(int)right.size()) {
        record(g_merge_arr, l+i, m+1+j,
               "Merge: [" + std::to_string(left[i]) + "] vs [" + std::to_string(right[j]) + "]",
               "compare");
        if (left[i] <= right[j]) {
            g_merge_arr[k++] = left[i++];
        } else {
            g_merge_arr[k++] = right[j++];
        }
        record(g_merge_arr, k-1, -1, "Placed " + std::to_string(g_merge_arr[k-1]) + " at position " + std::to_string(k-1), "merge");
    }
    while (i<(int)left.size())  { g_merge_arr[k++]=left[i++];  record(g_merge_arr, k-1,-1,"Drain left: "+std::to_string(g_merge_arr[k-1]),"merge"); }
    while (j<(int)right.size()) { g_merge_arr[k++]=right[j++]; record(g_merge_arr, k-1,-1,"Drain right: "+std::to_string(g_merge_arr[k-1]),"merge"); }
}

static void merge_sort_rec(int l, int r) {
    if (l >= r) return;
    int m = l + (r-l)/2;
    record(g_merge_arr, l, r, "Split ["+std::to_string(l)+".."+std::to_string(r)+"] at mid="+std::to_string(m), "pivot");
    merge_sort_rec(l, m);
    merge_sort_rec(m+1, r);
    merge(l, m, r);
}

static void merge_sort(std::vector<int> arr) {
    g_merge_arr = arr;
    merge_sort_rec(0, arr.size()-1);
    record(g_merge_arr, -1, -1, "Array is sorted!", "done");
}

// Quick Sort ──────────────────────────────────────────────────────────────────
static std::vector<int> g_quick_arr;

static int partition(int low, int high) {
    int pivot = g_quick_arr[high];
    record(g_quick_arr, high, -1, "Pivot selected: " + std::to_string(pivot), "pivot");
    int i = low - 1;
    for (int j = low; j < high; ++j) {
        record(g_quick_arr, j, high,
               "Compare [" + std::to_string(g_quick_arr[j]) + "] ≤ pivot [" + std::to_string(pivot) + "]?",
               "compare");
        if (g_quick_arr[j] <= pivot) {
            ++i;
            std::swap(g_quick_arr[i], g_quick_arr[j]);
            if (i != j) record(g_quick_arr, i, j, "Swap: "+std::to_string(g_quick_arr[i])+" ↔ "+std::to_string(g_quick_arr[j]), "swap");
        }
    }
    std::swap(g_quick_arr[i+1], g_quick_arr[high]);
    record(g_quick_arr, i+1, high, "Pivot ["+std::to_string(pivot)+"] placed at position "+std::to_string(i+1), "swap");
    return i+1;
}

static void quick_sort_rec(int low, int high) {
    if (low >= high) return;
    int pi = partition(low, high);
    quick_sort_rec(low, pi-1);
    quick_sort_rec(pi+1, high);
}

static void quick_sort(std::vector<int> arr) {
    g_quick_arr = arr;
    quick_sort_rec(0, arr.size()-1);
    record(g_quick_arr, -1, -1, "Array is sorted!", "done");
}

// Binary Search ───────────────────────────────────────────────────────────────
static void binary_search(std::vector<int> arr, int target) {
    // first, show the sorted array
    std::sort(arr.begin(), arr.end());
    record(arr, -1, -1, "Array sorted. Searching for [" + std::to_string(target) + "]", "pivot");
    int lo = 0, hi = arr.size()-1;
    while (lo <= hi) {
        int mid = lo + (hi-lo)/2;
        record(arr, lo, hi, "Range ["+std::to_string(lo)+".."+std::to_string(hi)+"] → mid="+std::to_string(mid), "compare");
        record(arr, mid, -1, "Checking mid ["+std::to_string(arr[mid])+"] vs target ["+std::to_string(target)+"]", "pivot");
        if (arr[mid] == target) {
            record(arr, mid, -1, "✓ Found ["+std::to_string(target)+"] at index "+std::to_string(mid)+"!", "done");
            return;
        } else if (arr[mid] < target) {
            lo = mid + 1;
            record(arr, mid, -1, "["+std::to_string(arr[mid])+"] < target → search right half", "compare");
        } else {
            hi = mid - 1;
            record(arr, mid, -1, "["+std::to_string(arr[mid])+"] > target → search left half", "compare");
        }
    }
    record(arr, -1, -1, "✗ ["+std::to_string(target)+"] not found in array", "done");
}

// ─── Count helpers ────────────────────────────────────────────────────────────

static int count_phase(const std::string& phase) {
    int n = 0;
    for (auto& s : g_steps) if (s.phase == phase) ++n;
    return n;
}

// ─── Exported API ─────────────────────────────────────────────────────────────

extern "C" {

/**
 * run_algorithm(algo_id, data_json, extra)
 *   algo_id: 0=Bubble, 1=Selection, 2=Insertion, 3=Merge, 4=Quick, 5=BinarySearch
 *   data_json: JSON array string e.g. "[5,3,8,1]"
 *   extra: for binary search, the target value (as int); ignored otherwise
 * Returns: total step count
 */
EMSCRIPTEN_KEEPALIVE
int run_algorithm(int algo_id, const char* data_json, int extra) {
    g_steps.clear();

    // Parse JSON array
    std::vector<int> arr;
    std::string raw(data_json);
    std::string nums;
    for (char c : raw) if (c != '[' && c != ']' && c != ' ') nums += c;
    std::istringstream ss(nums);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) arr.push_back(std::stoi(tok));
    }
    if (arr.empty()) return 0;

    static const char* names[] = {"Bubble Sort","Selection Sort","Insertion Sort","Merge Sort","Quick Sort","Binary Search"};
    std::string algo_name = (algo_id >= 0 && algo_id <= 5) ? names[algo_id] : "Unknown";

    switch (algo_id) {
        case 0: bubble_sort(arr);         break;
        case 1: selection_sort(arr);      break;
        case 2: insertion_sort(arr);      break;
        case 3: merge_sort(arr);          break;
        case 4: quick_sort(arr);          break;
        case 5: binary_search(arr, extra);break;
        default: break;
    }

    // Log to SQLite
    db_log(algo_name, arr.size(), g_steps.size(), count_phase("compare"), count_phase("swap"));

    return (int)g_steps.size();
}

/**
 * get_steps_json() → heap-allocated C string of the full step timeline.
 * Caller must free with free_result().
 */
EMSCRIPTEN_KEEPALIVE
char* get_steps_json() {
    std::string json = steps_to_json();
    char* buf = (char*)malloc(json.size() + 1);
    memcpy(buf, json.c_str(), json.size() + 1);
    return buf;
}

/**
 * get_history_json() → heap-allocated C string of the SQLite history table.
 */
EMSCRIPTEN_KEEPALIVE
char* get_history_json() {
    db_init();
    std::ostringstream oss;
    oss << "[";
    bool first = true;

    auto cb = [](void* ud, int, char** vals, char** cols) -> int {
        std::ostringstream* o = (std::ostringstream*)ud;
        // id, timestamp, algorithm, input_size, total_steps, comparisons, swaps
        if (o->tellp() > 1) *o << ",";
        *o << "{"
           << "\"id\":"         << (vals[0]?vals[0]:"0")    << ","
           << "\"timestamp\":\"" << (vals[1]?vals[1]:"")   << "\","
           << "\"algorithm\":\"" << (vals[2]?vals[2]:"")   << "\","
           << "\"input_size\":" << (vals[3]?vals[3]:"0")   << ","
           << "\"total_steps\":"<< (vals[4]?vals[4]:"0")   << ","
           << "\"comparisons\":"<< (vals[5]?vals[5]:"0")   << ","
           << "\"swaps\":"      << (vals[6]?vals[6]:"0")
           << "}";
        return 0;
    };

    sqlite3_exec(g_db,
        "SELECT id,timestamp,algorithm,input_size,total_steps,comparisons,swaps FROM history ORDER BY id DESC LIMIT 50;",
        cb, &oss, nullptr);

    oss << "]";
    std::string json = oss.str();
    char* buf = (char*)malloc(json.size() + 1);
    memcpy(buf, json.c_str(), json.size() + 1);
    return buf;
}

/** Free a string returned by get_steps_json / get_history_json */
EMSCRIPTEN_KEEPALIVE
void free_result(char* ptr) { free(ptr); }

} // extern "C"
