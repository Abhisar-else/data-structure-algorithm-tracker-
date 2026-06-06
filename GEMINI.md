# Algorithm Flight Recorder

A high-performance DSA (Data Structures and Algorithms) visualizer where the core logic is written in C++ and compiled to WebAssembly. It features real-time step-by-step recording, source code tracing, and performance telemetry stored in an embedded SQLite database.

## Project Overview

-   **Frontend:** A rich, responsive web dashboard (`public/index.html`) built with modern CSS and Vanilla JavaScript.
-   **Engine:** A C++ backend (`src/engine.cpp`) that implements 24+ algorithms across sorting, searching, graphs, trees, and dynamic programming.
-   **Bridge:** A dual-layer integration system.
    -   `public/engine.sim.js`: A JavaScript simulation of the C++ engine for zero-setup development.
    -   `public/engine.wasm-bridge.js`: The production bridge that connects the UI to the compiled WASM binary.
-   **Telemetry:** SQLite database used to log every run, including comparisons, swaps, and total operational steps, enabling performance benchmarking.

## Key Technologies

-   **C++17:** Core algorithm implementation.
-   **WebAssembly (WASM):** Compilation target for near-native performance.
-   **Emscripten / Embind:** Toolchain for C++ to JavaScript interoperability.
-   **SQLite:** Embedded database for historical performance data.
-   **Monaco Editor:** Integrated source code viewer with real-time line highlighting.

## Directory Structure

-   `src/`: Contains the C++ source code (`engine.cpp`).
-   `public/`: Contains the web assets, including the UI, JS bridges, and compiled WASM.
-   `emsdk/`: Local installation of the Emscripten SDK (used for building).

## Building and Running

### Running the Web Dashboard (JS Simulation)
The application works out-of-the-box using the JavaScript simulation.
1.  Open `public/index.html` in a web browser.
2.  Alternatively, serve it using a static server:
    ```bash
    cd public
    npx serve .
    ```

### Building the C++ WASM Engine
To compile the C++ source into WebAssembly:
1.  **Initialize Environment:**
    ```bash
    emsdk\emsdk_env.bat
    ```
2.  **Compile:**
    ```bash
    emcc src/engine.cpp -o public/engine.js -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -sMODULARIZE=1 -sEXPORT_NAME="AlgoEngineWASM" -sUSE_SQLITE3=1 --bind -lidbfs.js -O2 -std=c++17
    ```
    *Note: The compilation script is also available in `public/build.sh`.*

## Development Conventions

-   **Algorithm Implementation:** New algorithms should be added to `src/engine.cpp`. Use the `StepRecorder` class to record state transitions (`record()` method) for visualization.
-   **Consistency:** When adding a new algorithm to the C++ engine, ensure a matching implementation or fallback is added to `public/engine.sim.js` to maintain simulation parity.
-   **UI Updates:** All UI logic and rendering are contained in `public/index.html`. Structural visualizations (Graphs, Trees) are handled via SVG rendering.
-   **State Management:** The UI state (current algorithm, steps, playback status) is managed globally within the `index.html` script block.

## Quality Assurance

The project includes comprehensive test reports and stress tests:
-   `QA_FINAL_REPORT.md`: Summary of functional and regression testing.
-   `QA_STRESS_TEST_REPORT.md`: Results of high-load and edge-case testing.
-   `public/qa_stress_test.js`: Automated stress testing suite for the algorithm engine.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
