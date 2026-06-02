/**
 * QA STRESS TEST SUITE - DSA Visualizer
 * Principal QA Engineer & WebAssembly Performance Expert
 * 
 * This script conducts comprehensive edge-case testing including:
 * 1. Memory & Bridge Analysis
 * 2. Edge Case Execution (empty, single, large, sorted, reverse-sorted, negative)
 * 3. UI State Synchronization
 * 4. Input Validation & Injection Tests
 */

// ════════════════════════════════════════════════════════════════════════════════
// TEST HARNESS
// ════════════════════════════════════════════════════════════════════════════════

const QAStressTest = (() => {
  let results = [];
  let failureCount = 0;
  let successCount = 0;

  function log(category, status, message, details = {}) {
    const entry = { category, status, message, timestamp: new Date().toISOString(), ...details };
    results.push(entry);
    console.log(`[${status}] ${category}: ${message}`, details);
    if (status === 'FAIL') failureCount++;
    else if (status === 'PASS') successCount++;
  }

  // ════════════════════════════════════════════════════════════════════════════
  // 1. MEMORY & BRIDGE ANALYSIS
  // ════════════════════════════════════════════════════════════════════════════

  function analyzeMemoryBridge() {
    console.log('\n╔════ MEMORY & BRIDGE ANALYSIS ════╗\n');

    // Check 1: Verify AlgoEngine API exists
    if (typeof AlgoEngine === 'undefined') {
      log('BRIDGE', 'FAIL', 'AlgoEngine not defined', { location: 'global scope' });
      return;
    }
    log('BRIDGE', 'PASS', 'AlgoEngine object exists');

    // Check 2: Verify required methods
    const requiredMethods = ['run', 'getHistory', 'NAMES', 'CATEGORY'];
    requiredMethods.forEach(method => {
      if (typeof AlgoEngine[method] === 'undefined') {
        log('BRIDGE', 'FAIL', `AlgoEngine.${method} missing`, { type: typeof AlgoEngine[method] });
      } else {
        log('BRIDGE', 'PASS', `AlgoEngine.${method} exists`, { type: typeof AlgoEngine[method] });
      }
    });

    // Check 3: Verify NAMES array length
    if (AlgoEngine.NAMES && AlgoEngine.NAMES.length !== 23) {
      log('BRIDGE', 'WARN', `Expected 23 algorithms, got ${AlgoEngine.NAMES.length}`, 
        { count: AlgoEngine.NAMES.length });
    } else {
      log('BRIDGE', 'PASS', 'Correct algorithm count (23)', { count: 23 });
    }

    // Check 4: Memory leak in JSON serialization
    const largeArray = Array.from({length: 10000}, (_, i) => i);
    const memBefore = performance.memory?.usedJSHeapSize || 0;
    const jsonStr = JSON.stringify(largeArray);
    const memAfter = performance.memory?.usedJSHeapSize || 0;
    const memDelta = (memAfter - memBefore) / 1024 / 1024; // MB
    
    log('MEMORY', memDelta > 100 ? 'WARN' : 'PASS', 
      `JSON serialization of 10k elements: ${memDelta.toFixed(2)}MB`, 
      { elements: 10000, memDeltaMB: memDelta.toFixed(2) });

    // Check 5: Validate step structure
    const testSteps = AlgoEngine.run(0, [5, 3, 8, 1]); // Bubble Sort
    if (!Array.isArray(testSteps)) {
      log('BRIDGE', 'FAIL', 'AlgoEngine.run() did not return an array', 
        { returned: typeof testSteps });
      return;
    }
    
    if (testSteps.length === 0) {
      log('BRIDGE', 'WARN', 'AlgoEngine.run() returned empty step array for valid input');
      return;
    }

    const firstStep = testSteps[0];
    const requiredFields = ['array', 'a', 'b', 'action', 'phase'];
    const missingFields = requiredFields.filter(f => !(f in firstStep));
    
    if (missingFields.length > 0) {
      log('BRIDGE', 'FAIL', `Step object missing fields: ${missingFields.join(', ')}`, 
        { step: firstStep });
    } else {
      log('BRIDGE', 'PASS', 'Step structure valid', { stepCount: testSteps.length });
    }

    // Check 6: Detect rec() function bug (from previous report)
    const linkedListSteps = AlgoEngine.run(12, [1, 2, 3]);
    if (linkedListSteps.length === 0 || linkedListSteps.some(s => s.phase === 'error')) {
      log('BRIDGE', 'FAIL', 'Linked List (algo 12) produces no output or error', 
        { stepCount: linkedListSteps.length });
    } else {
      log('BRIDGE', 'PASS', 'Linked List algorithm executes', { stepCount: linkedListSteps.length });
    }
  }

  // ════════════════════════════════════════════════════════════════════════════
  // 2. EDGE CASE EXECUTION
  // ════════════════════════════════════════════════════════════════════════════

  function edgeCaseExecution() {
    console.log('\n╔════ EDGE CASE EXECUTION ════╗\n');

    const edgeCases = [
      { name: 'Empty array', input: [], algoId: 0 },
      { name: 'Single element', input: [42], algoId: 0 },
      { name: 'Two elements', input: [5, 3], algoId: 0 },
      { name: 'Already sorted', input: [1, 2, 3, 4, 5], algoId: 0 },
      { name: 'Reverse sorted', input: [5, 4, 3, 2, 1], algoId: 0 },
      { name: 'All duplicates', input: [7, 7, 7, 7], algoId: 0 },
      { name: 'Negative numbers', input: [-5, -1, -3, -2], algoId: 0 },
      { name: 'Mixed pos/neg', input: [-10, 5, -3, 0, 20], algoId: 0 },
      { name: 'Large dataset (1000)', input: Array.from({length: 1000}, () => Math.floor(Math.random() * 10000)), algoId: 0 },
      { name: 'Large dataset (5000)', input: Array.from({length: 5000}, () => Math.floor(Math.random() * 10000)), algoId: 0 },
    ];

    edgeCases.forEach(testCase => {
      try {
        const start = performance.now();
        const steps = AlgoEngine.run(testCase.algoId, testCase.input);
        const elapsed = performance.now() - start;

        if (!Array.isArray(steps)) {
          log('EDGE_CASE', 'FAIL', `${testCase.name}: returned non-array`, 
            { returned: typeof steps });
          return;
        }

        // Validate final state
        if (steps.length > 0) {
          const finalArray = steps[steps.length - 1].array;
          const isSorted = finalArray.every((v, i, arr) => i === 0 || arr[i-1] <= v);
          
          if (testCase.input.length > 1 && !isSorted) {
            log('EDGE_CASE', 'FAIL', `${testCase.name}: final array not sorted`, 
              { expected: JSON.stringify(testCase.input.sort((a,b)=>a-b)), 
                got: JSON.stringify(finalArray) });
          } else {
            log('EDGE_CASE', 'PASS', `${testCase.name}: ✓ (${steps.length} steps, ${elapsed.toFixed(2)}ms)`, 
              { stepCount: steps.length, timeMs: elapsed.toFixed(2) });
          }
        } else {
          log('EDGE_CASE', testCase.input.length === 0 ? 'PASS' : 'FAIL', 
            `${testCase.name}: ${steps.length} steps`, 
            { stepCount: steps.length });
        }
      } catch (err) {
        log('EDGE_CASE', 'FAIL', `${testCase.name}: Exception thrown`, 
          { error: err.message, stack: err.stack?.split('\n')[0] });
      }
    });
  }

  // ════════════════════════════════════════════════════════════════════════════
  // 3. UI STATE SYNCHRONIZATION TEST
  // ════════════════════════════════════════════════════════════════════════════

  function uiStateSynchronization() {
    console.log('\n╔════ UI STATE SYNCHRONIZATION ════╗\n');

    // Simulate rapid step execution
    const testArray = [9, 3, 7, 1, 4];
    const steps = AlgoEngine.run(0, testArray);

    if (steps.length === 0) {
      log('UI_SYNC', 'FAIL', 'No steps to verify synchronization');
      return;
    }

    // Check 1: Array dimensions consistency
    const firstStepArrayLen = steps[0].array.length;
    const stepsWithWrongLen = steps.filter((s, idx) => s.array.length !== firstStepArrayLen);
    
    if (stepsWithWrongLen.length > 0) {
      log('UI_SYNC', 'FAIL', `${stepsWithWrongLen.length} steps have inconsistent array length`, 
        { expected: firstStepArrayLen, found: stepsWithWrongLen.map(s => s.array.length) });
    } else {
      log('UI_SYNC', 'PASS', 'All steps maintain consistent array length', 
        { length: firstStepArrayLen, stepCount: steps.length });
    }

    // Check 2: Index bounds validation
    const outOfBoundsSteps = steps.filter(s => 
      (s.a >= 0 && s.a >= s.array.length) || (s.b >= 0 && s.b >= s.array.length)
    );
    
    if (outOfBoundsSteps.length > 0) {
      log('UI_SYNC', 'FAIL', `${outOfBoundsSteps.length} steps have out-of-bounds indices`, 
        { steps: outOfBoundsSteps });
    } else {
      log('UI_SYNC', 'PASS', 'All index references are within bounds');
    }

    // Check 3: Phase label validity
    const validPhases = ['compare', 'swap', 'pivot', 'merge', 'insert', 'delete', 'visit', 'found', 'done', 'dp'];
    const invalidPhases = steps.filter(s => !validPhases.includes(s.phase));
    
    if (invalidPhases.length > 0) {
      log('UI_SYNC', 'FAIL', `${invalidPhases.length} steps have invalid phase labels`, 
        { invalid: [...new Set(invalidPhases.map(s => s.phase))] });
    } else {
      log('UI_SYNC', 'PASS', 'All phase labels are valid', { validPhases: validPhases.length });
    }

    // Check 4: Action string length (UI rendering concern)
    const maxActionLen = Math.max(...steps.map(s => (s.action || '').length));
    if (maxActionLen > 500) {
      log('UI_SYNC', 'WARN', `Longest action string: ${maxActionLen} chars (may cause UI wrap issues)`, 
        { maxLen: maxActionLen, recommended: 150 });
    } else {
      log('UI_SYNC', 'PASS', `Action strings reasonable (max ${maxActionLen} chars)`);
    }

    // Check 5: Rapid playback simulation (10x speed)
    const playbackSteps = [0, Math.floor(steps.length * 0.25), Math.floor(steps.length * 0.5), 
                           Math.floor(steps.length * 0.75), steps.length - 1];
    const playbackSync = playbackSteps.every(idx => {
      const step = steps[idx];
      return step && Array.isArray(step.array) && step.phase && (step.a === -1 || step.a >= 0) && (step.b === -1 || step.b >= 0);
    });
    
    if (playbackSync) {
      log('UI_SYNC', 'PASS', 'Rapid playback state valid at key checkpoints');
    } else {
      log('UI_SYNC', 'FAIL', 'Rapid playback state corrupted at some checkpoints');
    }
  }

  // ════════════════════════════════════════════════════════════════════════════
  // 4. INPUT VALIDATION & INJECTION TESTS
  // ════════════════════════════════════════════════════════════════════════════

  function inputValidation() {
    console.log('\n╔════ INPUT VALIDATION & INJECTION ════╗\n');

    const maliciousInputs = [
      { name: 'Null input', input: null, algoId: 0 },
      { name: 'Undefined input', input: undefined, algoId: 0 },
      { name: 'String input', input: 'not an array', algoId: 0 },
      { name: 'Object input', input: {}, algoId: 0 },
      { name: 'Mixed types in array', input: [1, 'two', 3, null, 5], algoId: 0 },
      { name: 'NaN in array', input: [1, NaN, 3], algoId: 0 },
      { name: 'Infinity in array', input: [1, Infinity, -Infinity, 3], algoId: 0 },
      { name: 'Very large numbers', input: [1e308, -1e308, 1e307], algoId: 0 },
      { name: 'Invalid algo ID', input: [1, 2, 3], algoId: 999 },
      { name: 'Negative algo ID', input: [1, 2, 3], algoId: -1 },
      { name: 'Float algo ID', input: [1, 2, 3], algoId: 0.5 },
      { name: 'Circular reference array', input: (() => { const a = [1, 2, 3]; a.self = a; return a; })(), algoId: 0 },
      { name: 'Very deep array nesting', input: [[[[[[[[[[[1]]]]]]]]]], algoId: 0 },
    ];

    maliciousInputs.forEach(testCase => {
      try {
        const result = AlgoEngine.run(testCase.algoId, testCase.input);
        
        if (result === undefined || result === null) {
          log('INJECTION', 'WARN', `${testCase.name}: returned falsy value`, 
            { returned: typeof result });
        } else if (!Array.isArray(result)) {
          log('INJECTION', 'FAIL', `${testCase.name}: did not return array`, 
            { returned: typeof result });
        } else if (result.length > 0 && !result[0].array) {
          log('INJECTION', 'FAIL', `${testCase.name}: step object missing 'array' field`, 
            { step: result[0] });
        } else {
          log('INJECTION', 'PASS', `${testCase.name}: handled gracefully`);
        }
      } catch (err) {
        log('INJECTION', 'FAIL', `${testCase.name}: Exception thrown`, 
          { error: err.message.substring(0, 100), code: err.code });
      }
    });
  }

  // ════════════════════════════════════════════════════════════════════════════
  // 5. WASM MEMORY MANAGEMENT TEST
  // ════════════════════════════════════════════════════════════════════════════

  function wasmMemoryManagement() {
    console.log('\n╔════ WASM MEMORY MANAGEMENT ════╗\n');

    // Simulate multiple large algorithm runs
    const iterations = 5;
    const arraySize = 2000;
    
    try {
      const memSnapshots = [];
      
      for (let i = 0; i < iterations; i++) {
        const largeArray = Array.from({length: arraySize}, () => Math.floor(Math.random() * 10000));
        const beforeMem = performance.memory?.usedJSHeapSize || 0;
        
        // Run algorithm
        AlgoEngine.run(0, largeArray);
        
        const afterMem = performance.memory?.usedJSHeapSize || 0;
        memSnapshots.push({
          iteration: i,
          heapBefore: (beforeMem / 1024 / 1024).toFixed(2),
          heapAfter: (afterMem / 1024 / 1024).toFixed(2),
          delta: ((afterMem - beforeMem) / 1024 / 1024).toFixed(2)
        });
      }

      // Check for memory leak pattern
      const deltas = memSnapshots.map(s => parseFloat(s.delta));
      const avgDelta = deltas.reduce((a, b) => a + b, 0) / deltas.length;
      const lastDelta = deltas[deltas.length - 1];
      
      if (lastDelta > avgDelta * 2) {
        log('MEMORY', 'WARN', `Possible memory leak: last iteration delta ${lastDelta}MB > avg ${avgDelta.toFixed(2)}MB`, 
          { snapshots: memSnapshots });
      } else {
        log('MEMORY', 'PASS', `No obvious memory leak pattern detected (avg delta ${avgDelta.toFixed(2)}MB)`, 
          { iterations, arraySize });
      }
    } catch (err) {
      log('MEMORY', 'FAIL', 'Memory test failed with exception', { error: err.message });
    }
  }

  // ════════════════════════════════════════════════════════════════════════════
  // MAIN TEST EXECUTION
  // ════════════════════════════════════════════════════════════════════════════

  function runAllTests() {
    console.clear();
    console.log('╔══════════════════════════════════════════════════════════════════════════╗');
    console.log('║       QA STRESS TEST - DSA Algorithm Visualizer                          ║');
    console.log('║       Principal QA Engineer & WebAssembly Performance Expert             ║');
    console.log('╚══════════════════════════════════════════════════════════════════════════╝');

    analyzeMemoryBridge();
    edgeCaseExecution();
    uiStateSynchronization();
    inputValidation();
    wasmMemoryManagement();

    // Summary
    console.log('\n╔════ TEST SUMMARY ════╗\n');
    console.log(`✓ PASSED: ${successCount}`);
    console.log(`✗ FAILED: ${failureCount}`);
    console.log(`⚠ TOTAL TESTS: ${results.length}\n`);

    return { results, successCount, failureCount };
  }

  return { runAllTests, getResults: () => results };
})();

// Auto-run on page load
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.qaResults = QAStressTest.runAllTests();
    window.exportResults = () => {
      const csv = 'Category,Status,Message,Timestamp\n' +
        window.qaResults.results
          .map(r => `"${r.category}","${r.status}","${r.message.replace(/"/g, '""')}","${r.timestamp}"`)
          .join('\n');
      const blob = new Blob([csv], {type: 'text/csv'});
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'qa-test-results.csv';
      a.click();
    };
  });
} else {
  window.qaResults = QAStressTest.runAllTests();
}
