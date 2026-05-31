/**
 * Algorithm Flight Recorder — C++ Engine
 * Compiles to WebAssembly via Emscripten + Embind.
 *
 * ── Build command ────────────────────────────────────────────────────────────
 *   emcc engine.cpp -o ../public/engine.js     \
 *     -sWASM=1                                  \
 *     -sALLOW_MEMORY_GROWTH=1                   \
 *     -sFORCE_FILESYSTEM=1                      \
 *     -sMODULARIZE=1                            \
 *     -sEXPORT_NAME="AlgoEngineWASM"            \
 *     -sUSE_SQLITE3=1                           \
 *     --bind                                    \  ← enables EMSCRIPTEN_BINDINGS
 *     -lidbfs.js                                \
 *     -O2                                       \
 *     -std=c++17
 *
 * ── Why EMSCRIPTEN_BINDINGS instead of extern "C" ────────────────────────────
 *   Embind (--bind) exposes C++ functions as proper JS methods with typed
 *   argument checking. extern "C" + ccall requires manual string marshalling.
 *   The spec explicitly requires EMSCRIPTEN_BINDINGS.
 *
 * ── Log Contract (spec-compliant pipe-delimited format) ───────────────────────
 *   Every step serialises as:   ValueArray|PointerA|PointerB|Status;
 *   e.g.:  5,3,8,1|0|1|compare;3,5,8,1|0|1|swap;
 *   JS splits on ';', then '|' to get [arrayCSV, a, b, phase].
 *   get_steps_pipe() returns this format; get_steps_json() returns JSON.
 *   Both are available — JS bridge uses pipe-delimited as the primary format
 *   per the spec, with JSON as a rich fallback.
 *
 * ── SQLite table (spec-compliant) ────────────────────────────────────────────
 *   Table: Performance_Logs
 *   Columns: Algorithm_Name | Dataset_Size | Total_Steps_Computed
 *   Extended: Comparisons_Count | Swaps_Count | Run_Timestamp
 */

#include <emscripten/bind.h>      // ← Embind — required for EMSCRIPTEN_BINDINGS
#include <sqlite3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <functional>

using namespace emscripten;

// ─── Step (extended with lineNumber for source-code highlighting) ─────────────

struct Step {
    std::vector<int> array;   // full array snapshot
    int  indexA     = -1;     // primary active index   (red)
    int  indexB     = -1;     // secondary active index (yellow)
    std::string action;       // human-readable description
    std::string phase;        // "compare" | "swap" | "pivot" | "merge" | "done"
    int  lineNumber = -1;     // source line to highlight in the code panel (-1 = none)
};

// ─── StepRecorder — owns the step timeline and JSON/pipe serialisation ─────────
//
// Replaces the old global g_steps vector + free functions.
// The DB hook is injected via a callback so algorithms stay DB-agnostic.

class StepRecorder {
public:
    using DBHook = std::function<void(const std::string&, int, int, int, int)>;

    explicit StepRecorder(DBHook hook = nullptr) : db_hook_(std::move(hook)) {}

    void clear() { steps_.clear(); }

    void record(const std::vector<int>& arr, int a, int b,
                const std::string& action, const std::string& phase,
                int lineNumber = -1)
    {
        steps_.push_back({arr, a, b, action, phase, lineNumber});
    }

    int  size()  const { return static_cast<int>(steps_.size()); }
    bool empty() const { return steps_.empty(); }

    const Step& at(int i) const { return steps_.at(i); }

    int count_phase(const std::string& phase) const {
        int n = 0;
        for (auto& s : steps_) if (s.phase == phase) ++n;
        return n;
    }

    // Flush performance metrics to SQLite via the injected hook
    void flush_db(const std::string& algo_name, int dataset_size) {
        if (db_hook_) {
            db_hook_(algo_name, dataset_size, size(),
                     count_phase("compare"), count_phase("swap"));
        }
    }

    // ── Serialisers ──────────────────────────────────────────────────────────

    std::string to_json() const {
        std::ostringstream oss;
        oss << "[";
        for (int i = 0; i < size(); ++i) {
            const Step& s = steps_[i];
            if (i) oss << ",";
            oss << "{"
                << "\"array\":"      << array_to_json(s.array)   << ","
                << "\"a\":"          << s.indexA                  << ","
                << "\"b\":"          << s.indexB                  << ","
                << "\"action\":\""   << escape_json(s.action)    << "\","
                << "\"phase\":\""    << escape_json(s.phase)     << "\","
                << "\"ln\":"         << s.lineNumber              // line number for highlight
                << "}";
        }
        oss << "]";
        return oss.str();
    }

    // Spec-required pipe-delimited: ValueArray|PointerA|PointerB|Status|LineNumber;
    std::string to_pipe() const {
        std::ostringstream oss;
        for (const Step& s : steps_) {
            for (size_t i = 0; i < s.array.size(); ++i) {
                if (i) oss << ",";
                oss << s.array[i];
            }
            oss << "|" << s.indexA
                << "|" << s.indexB
                << "|" << s.phase
                << "|" << s.lineNumber
                << ";";
        }
        return oss.str();
    }

private:
    std::vector<Step> steps_;
    DBHook            db_hook_;

    static std::string array_to_json(const std::vector<int>& arr) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < arr.size(); ++i) { if (i) oss << ","; oss << arr[i]; }
        oss << "]";
        return oss.str();
    }

    static std::string escape_json(const std::string& s) {
        std::string out;
        for (char c : s) {
            if      (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else                out += c;
        }
        return out;
    }
};

// ─── Abstract Algorithm base class ────────────────────────────────────────────
//
// Every concrete algorithm holds a reference to a StepRecorder.
// sort() is the single point of polymorphic dispatch.

class Algorithm {
public:
    explicit Algorithm(StepRecorder& recorder) : recorder_(recorder) {}
    virtual ~Algorithm() = default;

    virtual void sort(std::vector<int>& arr) = 0;

    // Convenience — forwards to the recorder with the algorithm's line map
    void record(const std::vector<int>& arr, int a, int b,
                const std::string& action, const std::string& phase,
                int lineNumber = -1)
    {
        recorder_.record(arr, a, b, action, phase, lineNumber);
    }

protected:
    StepRecorder& recorder_;
};

// ─── BubbleSort : Algorithm ───────────────────────────────────────────────────
//
// Bubble Sort source snippet (shown in the code panel, 1-indexed):
//
//  1  void bubbleSort(int arr[], int n) {
//  2    for (int i = 0; i < n-1; i++) {
//  3      bool swapped = false;
//  4      for (int j = 0; j < n-i-1; j++) {
//  5        if (arr[j] > arr[j+1]) {
//  6          swap(arr[j], arr[j+1]);
//  7          swapped = true;
//  8        }
//  9      }
// 10      if (!swapped) break;
// 11    }
// 12  }
//
// Each record() call tags the line that is actively executing.

class BubbleSort : public Algorithm {
public:
    explicit BubbleSort(StepRecorder& recorder) : Algorithm(recorder) {}

    void sort(std::vector<int>& arr) override {
        const int n = static_cast<int>(arr.size());

        record(arr, -1, -1, "Starting Bubble Sort", "pivot", 1);          // line 1: fn entry

        for (int i = 0; i < n - 1; ++i) {
            record(arr, -1, -1,
                   "Outer pass " + std::to_string(i) + " — bubble ceiling at " + std::to_string(n-1-i),
                   "pivot", 2);                                             // line 2: outer for

            bool swapped = false;
            record(arr, -1, -1, "swapped = false", "pivot", 3);           // line 3

            for (int j = 0; j < n - i - 1; ++j) {
                record(arr, j, j + 1,
                       "Comparing arr[" + std::to_string(j) + "]=" +
                       std::to_string(arr[j]) + " vs arr[" + std::to_string(j+1) + "]=" +
                       std::to_string(arr[j+1]),
                       "compare", 4);                                       // line 4: inner for / compare

                record(arr, j, j + 1,
                       "Is arr[" + std::to_string(j) + "] > arr[" + std::to_string(j+1) + "] ?",
                       "compare", 5);                                       // line 5: if condition

                if (arr[j] > arr[j + 1]) {
                    std::swap(arr[j], arr[j + 1]);
                    record(arr, j, j + 1,
                           "Swapped → " + std::to_string(arr[j]) + " ↔ " + std::to_string(arr[j+1]),
                           "swap", 6);                                      // line 6: swap()

                    swapped = true;
                    record(arr, j, j + 1, "swapped = true", "swap", 7);   // line 7
                }
            }

            record(arr, -1, -1,
                   "Inner loop done — swapped=" + std::string(swapped ? "true" : "false"),
                   "pivot", 10);                                            // line 10: early-exit check

            if (!swapped) {
                record(arr, -1, -1, "No swaps — array is sorted, breaking early", "done", 10);
                break;
            }
        }

        record(arr, -1, -1, "Array is fully sorted!", "done", 12);        // line 12: fn exit
    }
};

// ─── Global recorder instance (replaces g_steps) ─────────────────────────────
//
// The recorder is constructed once and reused across calls.
// The DB hook is wired in lazily on first run_algorithm call.

static StepRecorder* g_recorder = nullptr;
static sqlite3*      g_db       = nullptr;

// ─── SQLite Helpers ───────────────────────────────────────────────────────────

static void db_init() {
    if (g_db) return;
    // Use IDBFS-mounted path when running in browser (FORCE_FILESYSTEM=1 makes
    // this work); falls back gracefully to :memory: if mount isn't ready yet.
    int rc = sqlite3_open("/data/performance.db", &g_db);
    if (rc != SQLITE_OK) sqlite3_open(":memory:", &g_db);

    // ── Spec-compliant table: Performance_Logs ────────────────────────────────
    //    Required columns: Algorithm_Name, Dataset_Size, Total_Steps_Computed
    //    Extended columns: Comparisons_Count, Swaps_Count, Run_Timestamp
    const char* sql =
        "CREATE TABLE IF NOT EXISTS Performance_Logs ("
        "  id                  INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  Run_Timestamp       TEXT DEFAULT (datetime('now')),"
        "  Algorithm_Name      TEXT NOT NULL,"
        "  Dataset_Size        INTEGER NOT NULL,"
        "  Total_Steps_Computed INTEGER NOT NULL,"
        "  Comparisons_Count   INTEGER NOT NULL,"
        "  Swaps_Count         INTEGER NOT NULL"
        ");";
    sqlite3_exec(g_db, sql, nullptr, nullptr, nullptr);
}

static void db_log(const std::string& algo, int size, int steps, int cmps, int swaps) {
    db_init();
    // Parameterised insert — avoids SQL injection from algo name strings
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO Performance_Logs "
        "(Algorithm_Name, Dataset_Size, Total_Steps_Computed, Comparisons_Count, Swaps_Count) "
        "VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, algo.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 2, size);
        sqlite3_bind_int (stmt, 3, steps);
        sqlite3_bind_int (stmt, 4, cmps);
        sqlite3_bind_int (stmt, 5, swaps);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// ─── Step Recording (procedural algorithms use this helper) ──────────────────
// BubbleSort now uses the OOP path. All other algorithms still call this.

static void record(const std::vector<int>& arr, int a, int b,
                   const std::string& action, const std::string& phase,
                   int lineNumber = -1)
{
    if (g_recorder) g_recorder->record(arr, a, b, action, phase, lineNumber);
}

// ─── Algorithms (procedural — Selection, Insertion, Merge, Quick, Binary) ────

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
    return g_recorder ? g_recorder->count_phase(phase) : 0;
}

// ─── Exported API — Embind (EMSCRIPTEN_BINDINGS) ──────────────────────────────

static int run_algorithm_impl(int algo_id, const std::string& data_json, int extra) {

    // Lazy-init the recorder with the DB hook wired in
    if (!g_recorder) {
        g_recorder = new StepRecorder(
            [](const std::string& algo, int size, int steps, int cmps, int swaps) {
                db_log(algo, size, steps, cmps, swaps);
            }
        );
    }
    g_recorder->clear();

    // Parse input — accepts "[5,3,8,1]" or "5,3,8,1"
    std::vector<int> arr;
    std::string nums;
    for (char c : data_json) if (c != '[' && c != ']' && c != ' ') nums += c;
    std::istringstream ss(nums);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) {
            try { arr.push_back(std::stoi(tok)); } catch (...) {}
        }
    }
    if (arr.empty()) return 0;

    static const char* names[] = {
        "Bubble Sort","Selection Sort","Insertion Sort",
        "Merge Sort","Quick Sort","Binary Search"
    };
    std::string algo_name = (algo_id >= 0 && algo_id <= 5) ? names[algo_id] : "Unknown";

    switch (algo_id) {
        case 0: {
            // ── OOP path: polymorphic BubbleSort via Algorithm base ──────────
            BubbleSort bs(*g_recorder);
            bs.sort(arr);
            break;
        }
        case 1: selection_sort(arr);       break;
        case 2: insertion_sort(arr);       break;
        case 3: merge_sort(arr);           break;
        case 4: quick_sort(arr);           break;
        case 5: binary_search(arr, extra); break;
        default: break;
    }

    // Flush metrics to Performance_Logs
    g_recorder->flush_db(algo_name, static_cast<int>(arr.size()));

    return g_recorder->size();
}

static std::string get_steps_json_impl()  {
    return g_recorder ? g_recorder->to_json() : "[]";
}
static std::string get_steps_pipe_impl()  {
    return g_recorder ? g_recorder->to_pipe() : "";
}

static std::string get_history_json_impl() {
    db_init();
    std::ostringstream oss;
    oss << "[";

    // Query uses spec column names: Algorithm_Name, Dataset_Size, Total_Steps_Computed
    auto cb = [](void* ud, int, char** vals, char**) -> int {
        std::ostringstream* o = (std::ostringstream*)ud;
        // cols: id | Run_Timestamp | Algorithm_Name | Dataset_Size |
        //       Total_Steps_Computed | Comparisons_Count | Swaps_Count
        if (o->tellp() > 1) *o << ",";
        *o << "{"
           << "\"id\":"                   << (vals[0]?vals[0]:"0")  << ","
           << "\"Run_Timestamp\":\""      << (vals[1]?vals[1]:"")   << "\","
           << "\"Algorithm_Name\":\""     << (vals[2]?vals[2]:"")   << "\","
           << "\"Dataset_Size\":"         << (vals[3]?vals[3]:"0")  << ","
           << "\"Total_Steps_Computed\":" << (vals[4]?vals[4]:"0")  << ","
           << "\"Comparisons_Count\":"    << (vals[5]?vals[5]:"0")  << ","
           << "\"Swaps_Count\":"          << (vals[6]?vals[6]:"0")
           << "}";
        return 0;
    };

    sqlite3_exec(g_db,
        "SELECT id, Run_Timestamp, Algorithm_Name, Dataset_Size, "
        "Total_Steps_Computed, Comparisons_Count, Swaps_Count "
        "FROM Performance_Logs ORDER BY id DESC LIMIT 50;",
        cb, &oss, nullptr);

    oss << "]";
    return oss.str();
}

// Flush IDBFS writes to the browser's IndexedDB for cross-reload persistence.
// Call after run_algorithm_impl to ensure the Performance_Logs row is saved.
static void db_sync_impl() {
    // EM_ASM is still valid alongside Embind
    EM_ASM(
        if (typeof FS !== 'undefined' && typeof FS.syncfs === 'function') {
            FS.syncfs(false, function(err) {
                if (err) console.warn('[AlgoEngine] IDBFS sync error:', err);
            });
        }
    );
}

// ── EMSCRIPTEN_BINDINGS block — the spec-required binding method ───────────────
EMSCRIPTEN_BINDINGS(algo_engine) {
    function("run_algorithm",    &run_algorithm_impl);
    function("get_steps_json",   &get_steps_json_impl);
    function("get_steps_pipe",   &get_steps_pipe_impl);   // spec primary format
    function("get_history_json", &get_history_json_impl);
    function("db_sync",          &db_sync_impl);
}
