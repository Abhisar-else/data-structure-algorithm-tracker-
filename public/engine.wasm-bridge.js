/**
 * engine.wasm-bridge.js — Production bridge for the real C++ WASM engine.
 *
 * Uses Emscripten Embind to communicate with the C++ engine.
 * Falls back to the JavaScript simulation (engine.sim.js) if WASM is not ready.
 */

(function() {
  const simEngine = window.AlgoEngine; // Preserve the simulation engine if loaded first
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
        footerEngine.innerHTML = '<span class="dot live"></span> Engine: C++ WebAssembly';
      }
    }).catch(err => {
      console.warn('[AlgoEngine] WASM failed to load, using JS simulation fallback.', err);
      const footerEngine = document.getElementById('footer-engine');
      if (footerEngine) {
        footerEngine.innerHTML = '<span class="dot warn"></span> Engine: JS Simulation — Build with Emscripten to enable native C++ WASM';
      }
    });
  } else {
    const footerEngine = document.getElementById('footer-engine');
    if (footerEngine) {
      footerEngine.innerHTML = '<span class="dot warn"></span> Engine: JS Simulation — Build with Emscripten to enable native C++ WASM';
    }
  }

  window.AlgoEngine = {
    NAMES,
    CATEGORY,

    run: (algoId, inputArray, extra = 0) => {
      if (ready && Module) {
        try {
          Module.run_algorithm(algoId, JSON.stringify(inputArray), extra);
          return JSON.parse(Module.get_steps_json());
        } catch (err) {
          console.error('[AlgoEngine] WASM run error:', err);
        }
      }
      if (simEngine) return simEngine.run(algoId, inputArray, extra);
      console.error('[AlgoEngine] No engine ready.');
      return [];
    },

    getHistory: () => {
      if (ready && Module) {
        try {
          return JSON.parse(Module.get_history_json());
        } catch (err) {
          console.error('[AlgoEngine] WASM history error:', err);
        }
      }
      if (simEngine) return simEngine.getHistory();
      return [];
    },

    clearHistory: () => {
      if (ready && Module && typeof Module.clear_history === 'function') {
        try {
          Module.clear_history();
        } catch (err) {
          console.error('[AlgoEngine] WASM clear error:', err);
        }
      }
      if (simEngine && typeof simEngine.clearHistory === 'function') {
        simEngine.clearHistory();
      }
    }
  };
})();
