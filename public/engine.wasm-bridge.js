/**
 * engine.wasm-bridge.js — Production bridge for the real C++ WASM engine.
 *
 * Uses Emscripten Embind to communicate with the C++ engine.
 */

window.AlgoEngine = (() => {
  let Module = null;
  let ready  = false;

  const NAMES = [
    'Bubble Sort','Selection Sort','Insertion Sort','Merge Sort','Quick Sort',
    'Heap Sort','Shell Sort','Counting Sort','Radix Sort',
    'Linear Search','Binary Search','Jump Search',
    'Linked List','Stack','Queue',
    'BST Insert+Inorder','BST Search',
    'Graph BFS','Graph DFS',
    'Min-Heap',
    'Fibonacci DP','LCS DP',
    'Hash Table',
    'Dijkstra'
  ];

  const CATEGORY = [
    'sort','sort','sort','sort','sort','sort','sort','sort','sort',
    'search','search','search',
    'ds','ds','ds','tree','tree',
    'graph','graph',
    'heap',
    'dp','dp',
    'hash',
    'graph'
  ];

  // Initialise the WASM module
  if (typeof AlgoEngineWASM === 'function') {
    AlgoEngineWASM().then(mod => {
      Module = mod;
      ready  = true;
      console.log('[AlgoEngine] C++ WASM module loaded ✓');
      
      const wasmStatus = document.getElementById('wasm-status');
      if (wasmStatus) {
        wasmStatus.innerHTML = '<div class="dot live"></div> WASM LIVE';
      }
      const footerEngine = document.getElementById('footer-engine');
      if (footerEngine) {
        footerEngine.textContent = 'Engine: C++ WebAssembly';
      }
    }).catch(err => {
      console.error('[AlgoEngine] Failed to load WASM module:', err);
    });
  }

  function run(algoId, inputArray, extra = 0) {
    if (!ready || !Module) return [];
    try {
      const jsonStr = JSON.stringify(inputArray);
      // Embind: no underscores, handles string conversions
      Module.run_algorithm(algoId, jsonStr, extra);
      const stepsJson = Module.get_steps_json();
      return JSON.parse(stepsJson);
    } catch (err) {
      console.error('[AlgoEngine] Error running algorithm:', err);
      return [];
    }
  }

  function getHistory() {
    if (!ready || !Module) return [];
    try {
      // Embind returns std::string as JS string, no manual free needed
      return JSON.parse(Module.get_history_json());
    } catch (err) {
      console.error('[AlgoEngine] Error fetching history:', err);
      return [];
    }
  }

  function clearHistory() {
    if (!ready || !Module) return;
    try {
      // If C++ exports a clear method, call it. 
      // For now, if not exported, we note it.
      if (typeof Module.clear_history === 'function') {
        Module.clear_history();
      }
    } catch (err) {
      console.error('[AlgoEngine] Error clearing history:', err);
    }
  }

  return { run, getHistory, clearHistory, NAMES, CATEGORY };
})();
