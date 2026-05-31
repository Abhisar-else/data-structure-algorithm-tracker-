/**
 * engine.sim.js — JavaScript simulation of the C++ WASM engine.
 *
 * Mirrors the Embind API exactly:
 *   AlgoEngine.run(algoId, array, extra?) → Step[]   (legacy bridge helper)
 *   AlgoEngine.NAMES                                  (algorithm name list)
 *   AlgoEngine.getHistory()               → HistoryRow[]
 *
 * Step shape (matches C++ JSON output):
 *   { array[], a, b, action, phase, ln }
 *   ln = lineNumber (-1 for non-BubbleSort algorithms)
 *
 * PerformanceLogs row shape (matches Performance_Logs column names):
 *   { id, Run_Timestamp, Algorithm_Name, Dataset_Size,
 *     Total_Steps_Computed, Comparisons_Count, Swaps_Count }
 */

const AlgoEngine = (() => {

  // ── In-memory Performance_Logs (spec column names) ───────────────────────────
  const performanceLogs = [];
  let logId = 1;

  function dbLog(Algorithm_Name, Dataset_Size, Total_Steps_Computed,
                 Comparisons_Count, Swaps_Count) {
    performanceLogs.unshift({
      id: logId++,
      Run_Timestamp: new Date().toISOString().replace('T', ' ').slice(0, 19),
      Algorithm_Name,
      Dataset_Size,
      Total_Steps_Computed,
      Comparisons_Count,
      Swaps_Count,
    });
    if (performanceLogs.length > 50) performanceLogs.pop();
  }

  // ── Step recorder ────────────────────────────────────────────────────────────
  let steps = [];

  // ln = lineNumber for source-code highlight panel (-1 = no highlight)
  function record(arr, a, b, action, phase, ln = -1) {
    steps.push({ array: [...arr], a, b, action, phase, ln });
  }

  // ── BubbleSort — OOP mirror (generates lineNumber tags per spec) ──────────────
  //
  // Line map matches BubbleSort::sort() in engine.cpp exactly:
  //  1 = fn entry      2 = outer for      3 = swapped=false
  //  4 = inner for     5 = if condition   6 = swap()
  //  7 = swapped=true  10 = early-exit   12 = fn exit / done

  function bubbleSortOOP(arr) {
    const n = arr.length;
    record(arr, -1, -1, 'Starting Bubble Sort', 'pivot', 1);

    for (let i = 0; i < n - 1; i++) {
      record(arr, -1, -1,
        `Outer pass ${i} — bubble ceiling at ${n - 1 - i}`, 'pivot', 2);
      let swapped = false;
      record(arr, -1, -1, 'swapped = false', 'pivot', 3);

      for (let j = 0; j < n - i - 1; j++) {
        record(arr, j, j + 1,
          `Comparing arr[${j}]=${arr[j]} vs arr[${j+1}]=${arr[j+1]}`,
          'compare', 4);
        record(arr, j, j + 1,
          `Is arr[${j}] > arr[${j+1}] ?`, 'compare', 5);

        if (arr[j] > arr[j + 1]) {
          [arr[j], arr[j + 1]] = [arr[j + 1], arr[j]];
          record(arr, j, j + 1,
            `Swapped → ${arr[j]} ↔ ${arr[j+1]}`, 'swap', 6);
          swapped = true;
          record(arr, j, j + 1, 'swapped = true', 'swap', 7);
        }
      }

      record(arr, -1, -1,
        `Inner loop done — swapped=${swapped}`, 'pivot', 10);
      if (!swapped) {
        record(arr, -1, -1, 'No swaps — breaking early', 'done', 10);
        break;
      }
    }
    record(arr, -1, -1, 'Array is fully sorted!', 'done', 12);
  }

  // ── Procedural algorithms (ln = -1, no line highlighting) ────────────────────

  function selectionSort(arr) {
    const n = arr.length;
    for (let i = 0; i < n - 1; i++) {
      let minIdx = i;
      record(arr, i, -1, `Seeking minimum from index ${i}`, 'pivot');
      for (let j = i + 1; j < n; j++) {
        record(arr, minIdx, j,
          `Is [${arr[j]}] < current min [${arr[minIdx]}]?`, 'compare');
        if (arr[j] < arr[minIdx]) {
          minIdx = j;
          record(arr, minIdx, -1, `New minimum found: ${arr[minIdx]}`, 'pivot');
        }
      }
      if (minIdx !== i) {
        [arr[i], arr[minIdx]] = [arr[minIdx], arr[i]];
        record(arr, i, minIdx,
          `Placed minimum ${arr[i]} at position ${i}`, 'swap');
      }
    }
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function insertionSort(arr) {
    const n = arr.length;
    for (let i = 1; i < n; i++) {
      const key = arr[i];
      let j = i - 1;
      record(arr, i, -1, `Inserting [${key}] into sorted region`, 'pivot');
      while (j >= 0 && arr[j] > key) {
        record(arr, j, j + 1, `Shifting [${arr[j]}] right`, 'compare');
        arr[j + 1] = arr[j];
        record(arr, j, j + 1, `Shifted → position ${j + 1}`, 'swap');
        j--;
      }
      arr[j + 1] = key;
      record(arr, j + 1, -1, `Inserted [${key}] at position ${j + 1}`, 'swap');
    }
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function mergeSort(arr) {
    function mergeStep(lo, hi) {
      if (lo >= hi) return;
      const mid = (lo + hi) >> 1;
      record(arr, lo, hi, `Split [${lo}..${hi}] at mid=${mid}`, 'pivot');
      mergeStep(lo, mid);
      mergeStep(mid + 1, hi);
      const left  = arr.slice(lo, mid + 1);
      const right = arr.slice(mid + 1, hi + 1);
      let i = 0, j = 0, k = lo;
      while (i < left.length && j < right.length) {
        record(arr, lo + i, mid + 1 + j,
          `Merge: [${left[i]}] vs [${right[j]}]`, 'compare');
        arr[k++] = left[i] <= right[j] ? left[i++] : right[j++];
        record(arr, k - 1, -1, `Placed ${arr[k-1]} at position ${k-1}`, 'merge');
      }
      while (i < left.length)  { arr[k++] = left[i++];  record(arr, k-1, -1, `Drain left: ${arr[k-1]}`,  'merge'); }
      while (j < right.length) { arr[k++] = right[j++]; record(arr, k-1, -1, `Drain right: ${arr[k-1]}`, 'merge'); }
    }
    mergeStep(0, arr.length - 1);
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function quickSort(arr) {
    function partition(lo, hi) {
      const pivot = arr[hi];
      record(arr, hi, -1, `Pivot selected: ${pivot}`, 'pivot');
      let i = lo - 1;
      for (let j = lo; j < hi; j++) {
        record(arr, j, hi, `Compare [${arr[j]}] ≤ pivot [${pivot}]?`, 'compare');
        if (arr[j] <= pivot) {
          i++;
          if (i !== j) {
            [arr[i], arr[j]] = [arr[j], arr[i]];
            record(arr, i, j, `Swap: ${arr[i]} ↔ ${arr[j]}`, 'swap');
          }
        }
      }
      [arr[i + 1], arr[hi]] = [arr[hi], arr[i + 1]];
      record(arr, i + 1, hi,
        `Pivot [${pivot}] placed at position ${i + 1}`, 'swap');
      return i + 1;
    }
    function qs(lo, hi) {
      if (lo >= hi) return;
      const pi = partition(lo, hi);
      qs(lo, pi - 1);
      qs(pi + 1, hi);
    }
    qs(0, arr.length - 1);
    record(arr, -1, -1, 'Array is sorted!', 'done');
  }

  function binarySearch(arr, target) {
    arr.sort((a, b) => a - b);
    record(arr, -1, -1, `Array sorted. Searching for [${target}]`, 'pivot');
    let lo = 0, hi = arr.length - 1;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      record(arr, lo, hi, `Range [${lo}..${hi}] → mid=${mid}`, 'compare');
      record(arr, mid, -1,
        `Checking mid [${arr[mid]}] vs target [${target}]`, 'pivot');
      if (arr[mid] === target) {
        record(arr, mid, -1, `✓ Found [${target}] at index ${mid}!`, 'done');
        return;
      } else if (arr[mid] < target) {
        lo = mid + 1;
        record(arr, mid, -1,
          `[${arr[mid]}] < target → search right half`, 'compare');
      } else {
        hi = mid - 1;
        record(arr, mid, -1,
          `[${arr[mid]}] > target → search left half`, 'compare');
      }
    }
    record(arr, -1, -1, `✗ [${target}] not found in array`, 'done');
  }

  // ── Public API ────────────────────────────────────────────────────────────────

  const NAMES = [
    'Bubble Sort', 'Selection Sort', 'Insertion Sort',
    'Merge Sort', 'Quick Sort', 'Binary Search'
  ];

  function run(algoId, inputArray, extra = 0) {
    steps = [];
    const arr = [...inputArray];

    switch (algoId) {
      case 0: bubbleSortOOP(arr);        break;  // OOP path — generates ln tags
      case 1: selectionSort(arr);        break;
      case 2: insertionSort(arr);        break;
      case 3: mergeSort(arr);            break;
      case 4: quickSort(arr);            break;
      case 5: binarySearch(arr, extra);  break;
    }

    const cmps  = steps.filter(s => s.phase === 'compare').length;
    const swaps = steps.filter(s => s.phase === 'swap').length;

    // Log with spec-compliant Performance_Logs column names
    dbLog(
      NAMES[algoId] || 'Unknown',
      inputArray.length,
      steps.length,
      cmps,
      swaps
    );

    return steps;
  }

  function getHistory() {
    return performanceLogs.slice(0, 50);
  }

  return { run, getHistory, NAMES };
})();
