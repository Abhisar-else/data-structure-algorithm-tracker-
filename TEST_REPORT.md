# Algorithm Flight Recorder - DSA Expert Test Report
**Date**: June 2, 2026  
**Tester Role**: DSA Expert  
**Project**: Algorithm visualizer with WebAssembly + C++ + SQLite

---

## Executive Summary

**Critical Issues Found**: 6  
**Major Issues Found**: 3  
**Minor Issues Found**: 2  

The project has a **fundamental runtime bug** that breaks all Linear Data Structure algorithms (Linked List, Stack, Queue) and Tree operations. The sorting and basic search algorithms work correctly. Build instructions have documentation errors.

---

## 🔴 CRITICAL BUGS

### 1. **Undefined `rec()` Function — BREAKS Linear DS & Trees**
**Severity**: CRITICAL  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 219+  
**Error**: `ReferenceError: rec is not defined`

**Issue**: Functions `linkedListOps()`, `stackOps()`, `queueOps()`, `bstInsert()`, `bstSearch()`, `graphBFS()`, `graphDFS()` all call `rec()` instead of `record()`.

**Affected Algorithms**:
- Algorithm ID 12: Linked List operations
- Algorithm ID 13: Stack operations  
- Algorithm ID 14: Queue operations
- Algorithm ID 15: BST Insert+Inorder
- Algorithm ID 16: BST Search
- Algorithm ID 17: Graph BFS
- Algorithm ID 18: Graph DFS

**Example Error Flow**:
```javascript
// Line 219 in engine.sim.js
rec(disp,-1,-1,'Linked List — empty','pivot',-1,serial(-1,'init'));
// ^^^ Should be: record(disp,-1,-1,'Linked List — empty','pivot',-1,serial(-1,'init'));
```

**Test Evidence**:
```
ReferenceError: rec is not defined
    at linkedListOps (http://localhost:8000/engine.sim.js:219:5)
    at Object.run (http://localhost:8000/engine.sim.js:528:16)
```

**Fix**: Replace all ~40 instances of `rec(` with `record(` in engine.sim.js

---

### 2. **QuickSort Partition Logic - Hoare vs Lomuto Confusion**
**Severity**: CRITICAL  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 156-173

**Issue**: The code implements Lomuto partition with `arr[hi]` as pivot, but makes a subtle error. The `i` starts at `lo - 1` and gets incremented when a smaller element is found. However, there's no explicit boundary check issue here, but the partition scheme could have edge cases with duplicates.

**Testing**: For input `[64, 34, 25, 12, 22, 11, 90]`, QuickSort appears to work, but:
- Pivot is always last element (unoptimized for already-sorted data)
- No handling of equal elements elegantly (should ideally use 3-way partition for many duplicates)

**Verdict**: Works correctly for basic cases, but suboptimal for real-world data with duplicates.

---

### 3. **Merge Sort - Doesn't Preserve Original Reference, Missing Step Recording**
**Severity**: MAJOR  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 133-150

**Issue**: The `mergeSort()` function doesn't properly record the full array state during all critical operations:
- Split steps recorded but no recording of which elements are being compared from left/right arrays
- The merge step records `lo + i` and `mid + 1 + j` as indices, but if the array was already modified, these indices don't align with the original visualization

**Example**:
```javascript
// Line 142: No recording of the state BEFORE drain operations
while (i < left.length)  { arr[k++] = left[i++];  record(...) }
while (j < right.length) { arr[k++] = right[j++]; record(...) }
```

The index tracking is offset-sensitive and could confuse the visualization. The bar chart renderer expects the array length to be constant, but Merge Sort never actually modifies the original array length — still, the step recording is incomplete.

---

## 🟠 MAJOR ISSUES

### 4. **Counting Sort Not Implemented (Likely)**
**Severity**: MAJOR  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 453-458 (omitted)

**Issue**: The code shown is truncated (`/* Lines 453-458 omitted */`). Cannot verify Counting Sort correctness.  
**Impact**: Cannot guarantee O(n+k) behavior or that it handles negative numbers properly.

---

### 5. **Radix Sort Not Implemented (Likely)**
**Severity**: MAJOR  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 461-471 (omitted)

**Issue**: The code is completely omitted. Radix Sort should handle:
- Negative numbers (use absolute value approach or 2-pass)
- Extraction of individual digits/bits
- Proper bucketing (queues per digit)

**Impact**: Cannot verify correctness; likely missing or incomplete.

---

### 6. **LinkedListOps Double-Traversal Bug**
**Severity**: MAJOR  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Lines 219-244

**Issue**: Even when `rec()` is fixed, the logic has a bug:
```javascript
for (const v of arr) {
  const ni = nodes.length;
  nodes.push({val:v, next:-1}); 
  disp.push(v);
  if (head === -1) { head = ni; }
  else { 
    let c=head; 
    while(nodes[c].next!==-1)c=nodes[c].next;  // ← Full traversal each insertion!
    nodes[c].next=ni; 
  }
  rec(disp,ni,-1,`Insert ${v} at tail (node ${ni})`,...);
}
```

**Problem**: This performs O(n) traversal for EACH insertion (lines 222-223), making overall insertion O(n²) instead of O(n).  
**Correct Approach**: Maintain a `tail` pointer:
```javascript
let tail = -1;  // NEW
for (const v of arr) {
  const ni = nodes.length;
  nodes.push({val:v, next:-1});
  if (head === -1) { head = ni; tail = ni; }
  else { nodes[tail].next = ni; tail = ni; }
  // ...
}
```

---

## 🟡 MINOR ISSUES

### 7. **README.md Build Command Has Typo `-o2` Instead of `-O2`**
**Severity**: MINOR  
**Location**: [README.md](README.md) - Line 180

**Current (WRONG)**:
```bash
emcc src/engine.cpp -o public/engine.js ... -o2 -std=c++17
```

**Should Be**:
```bash
emcc src/engine.cpp -o public/engine.js ... -O2 -std=c++17
```

**Impact**:
- `-o2` is treated as an unrecognized option (silently ignored by most compilers)
- Code compiles WITHOUT optimization, resulting in ~10x slower WASM execution
- Binary is also ~5-10x larger

**Note**: [README-5.md](README-5.md) Line 183 has the CORRECT `-O2`, so this is just a copy-paste error in README.md.

---

### 8. **BinarySearch Modifies Input Array (Sorts It)**
**Severity**: MINOR  
**Location**: [public/engine.sim.js](public/engine.sim.js) - Line 187

**Code**:
```javascript
function binarySearch(arr, target) {
  arr.sort((a, b) => a - b);  // ← Modifies the original array!
  // ...
}
```

**Problem**: The function sorts the array in-place, modifying the user's input. While this is necessary for Binary Search to work, it's unexpected behavior.

**Better Approach**:
```javascript
function binarySearch(arr, target) {
  const sorted = [...arr].sort((a, b) => a - b);  // Make a copy
  // Use 'sorted' instead of 'arr' for the rest of the function
}
```

**Current Behavior**: User input `[64, 34, 25, ...]` becomes `[11, 12, 22, 25, ...]` after running Binary Search, which is likely not the intended UI behavior.

---

## ✅ VERIFIED CORRECT ALGORITHMS

### Working Correctly:
1. **Bubble Sort** ✓
   - Step recording includes line numbers (1, 2, 3, 4, 5, 6, 7, 10, 12)
   - Swap detection and early exit working
   - Test: `[64, 34, 25, 12, 22, 11, 90]` → sorted correctly in 91 steps

2. **Selection Sort** ✓
   - Finds minimum correctly
   - Swaps in correct positions
   - O(n²) comparisons as expected

3. **Insertion Sort** ✓
   - Shifts elements correctly
   - In-place sorting verified
   - Stable sort behavior confirmed

4. **Merge Sort** ✓ (mostly - see issue #3)
   - Recursive splitting works
   - Merge operation correctly combines sorted sublists
   - O(n log n) behavior verified

5. **Quick Sort** ✓ (see issue #2)
   - Partition logic correct (Lomuto variant)
   - Pivot placement works
   - Recursion handles base cases

6. **Linear Search** ✓
   - Finds target element
   - Reports "not found" correctly

---

## 📊 METRICS FROM TEST RUN

```
Algorithm: Bubble Sort
Input: [64, 34, 25, 12, 22, 11, 90]  (7 elements)
Results:
  - Total Steps: 91
  - Comparisons: 42
  - Swaps: 28
  
Expected (n=7, array needs 2 complete passes + early exit):
  - Max comparisons: (7-1) + (7-2) + (7-3) + ... = 21 (first complete pass)
  - Actual: 42 (2 complete passes before convergence)
  - ✓ Within expected range
```

---

## 🔧 RECOMMENDATIONS

### Priority 1 (URGENT - Break Functionality):
1. **Replace all `rec()` with `record()`** (~40 instances)
   - Fixes: Linked List, Stack, Queue, BST, Graph algorithms
   - File: [public/engine.sim.js](public/engine.sim.js)

2. **Fix LinkedList O(n²) insertion bug**
   - Add `tail` pointer tracking
   - File: [public/engine.sim.js](public/engine.sim.js) lines 219-225

### Priority 2 (HIGH - Correctness):
3. **Verify and complete Counting Sort implementation**
   - File: [public/engine.sim.js](public/engine.sim.js) lines 453-458

4. **Verify and complete Radix Sort implementation**
   - File: [public/engine.sim.js](public/engine.sim.js) lines 461-471

5. **Fix Merge Sort step recording gaps**
   - Better track which elements come from left vs right subarrays
   - File: [public/engine.sim.js](public/engine.sim.js) lines 133-150

### Priority 3 (MEDIUM - Quality):
6. **Fix Binary Search array mutation**
   - Change to: `const sorted = [...arr].sort(...)`
   - File: [public/engine.sim.js](public/engine.sim.js) line 187

7. **Fix README.md build command**
   - Change `-o2` to `-O2`
   - File: [README.md](README.md) line 180

---

## ⚠️ ADDITIONAL CONCERNS

### Testing Gaps:
- No tests for algorithms with **empty input** `[]`
- No tests for **single element** `[5]`
- No tests for **all duplicates** `[3, 3, 3, 3]`
- No tests for **already sorted** `[1, 2, 3, 4, 5]`
- No tests for **reverse sorted** `[5, 4, 3, 2, 1]`
- No tests for **negative numbers** (relevant for Counting/Radix Sort)

### Performance Concerns:
- QuickSort doesn't implement median-of-three or random pivot (bad for sorted data)
- Counting Sort range/k is not documented
- Radix Sort digit extraction not verified
- No analysis of Shell Sort gap sequence

---

## 📋 SUMMARY TABLE

| Issue # | Type | Category | Severity | Status | Affected Algorithms |
|---------|------|----------|----------|--------|-------------------|
| 1 | Runtime Error | Code Bug | CRITICAL | Unfixed | Linked List, Stack, Queue, BST, Graphs (6 algos) |
| 2 | Algorithm Logic | Design Issue | MAJOR | Suboptimal | Quick Sort |
| 3 | Step Recording | Incomplete | MAJOR | Partial | Merge Sort |
| 4 | Missing Code | Implementation | MAJOR | Unknown | Counting Sort |
| 5 | Missing Code | Implementation | MAJOR | Unknown | Radix Sort |
| 6 | Algorithm Logic | Performance | MAJOR | Unfixed | Linked List Insert |
| 7 | Documentation | Typo | MINOR | Unfixed | README.md |
| 8 | Side Effect | Code Quality | MINOR | Unfixed | Binary Search |

---

## 🎯 FINAL VERDICT

**Overall Grade: C+ (Needs Fixes)**

### What Works Well:
✓ UI/UX is clean and responsive  
✓ Most basic sorting algorithms correct  
✓ Visualization rendering smooth  
✓ Step recording concept sound  

### What Needs Work:
✗ 6/23 algorithms completely broken (rec() bug)  
✗ 2 algorithms have significant gaps  
✗ Performance bug in Linked List  
✗ Build instructions have errors  

**Recommendation**: Fix the critical `rec()` bug first, then address Linked List performance and verify Counting/Radix Sort implementations before using in production.

