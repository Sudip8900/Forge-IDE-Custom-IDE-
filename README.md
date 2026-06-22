# code ZEN

code ZEN is a modern, high-performance, lightweight integrated development environment (IDE) built using C++, OpenGL 3.3, GLFW, and the docking branch of Dear ImGui. Designed specifically for Windows, it features a native Windows pseudo-terminal integration, multi-tab code runner capabilities, and advanced layout management.

---

## Key Features

### 1. High-Performance Graphics Framework
* **Dear ImGui Docking System**: Undock, drag, drop, and split panels (Code Editor, File Explorer, Terminals, Output Logs) in any configuration. Supports multi-viewport mode (drag windows outside the main window bounds, ideal for multi-monitor setups).
* **Smooth Rendering**: Capable of high-refresh rates (capped at VSync limit, e.g., 144 FPS).
* **Immersive Styling**: Dark mode client window frame decoration via native Windows DWM calls (`DwmSetWindowAttribute`).
* **Application & Executable Icons**: Customized with a premium logo. Configured as a native Windows executable icon (`.ico` resource compiled via CMake) and runtime GLFW window title bar icons.
* **Persistent Layouts**: Saves and restores your workspace structure, panel sizes, and settings across launches in `imgui.ini` and `settings.txt`.

### 2. Integrated Terminal & ConPTY Subsystem
* **Native Windows ConPTY Integration**: Real-time Win32 Pseudo-Console rendering with raw interactive shells (PowerShell, CMD, Git Bash, etc.).
* **Tabbed & Split Views**: Create multiple terminal tabs. Each tab can be split horizontally or vertically to run multiple terminal sessions side-by-side.
* **Console Scrollback & History**: Full scrolling support (via mouse wheel, scrollbars, or keyboard shortcut `Shift + PageUp` / `Shift + PageDown`).
* **Text Selection & Clipboard**: Drag to select text within terminals, right-click to Copy or Paste from/to the OS clipboard.
* **Terminal Search**: Built-in search bar (supporting case-sensitivity and regex) to find logs or text within the terminal scrollback history.
* **Git Macro Shortcuts**: Convenient right-click menu context actions for common git tasks (`git status`, `git add .`, `git commit`, `git push`, `git pull`, `git branch`, etc.) mapped straight to the session.
* **Thread-Safe Input Dispatching**: Integrated background thread-safe deferred queue allows compiling or external tasks to write safely to the terminal without causing recursive mutex deadlocks.

### 3. Workspace File Explorer
* **Asynchronous Indexing**: The file tree is cached and scanned in a background thread to prevent UI micro-stutters during heavy disk operations.
* **Last-Workspace Auto-Load**: Remembers the directory folder you were working on and automatically re-opens it on startup.
* **Directory Navigation**: Full hierarchical file tree view with intuitive icons and expansion buttons.

### 4. Multi-Language Code Editor & Runner
* **Built-in Editor**: Standard source file editing with tab space formatting.
* **Custom Compile & Run Actions**: Set custom compilers and runners per file extension (e.g. `g++`/`cl` for C++, `python` for Python) via the UI Settings popup.
* **Single-Click Runner**: Run code instantly. The runner handles compilation and dispatch commands, directing runtime logs straight to the Output Panel.

### 5. Color-Coded Output Panel
* **Live System Logs**: Catch all output from compilers, run scripts, and background operations.
* **Contextual Log Highlight Colorizer**:
  * **Red**: Compilation errors, failures (`Failed`, `ERROR`, `error`).
  * **Yellow**: Compilation warnings (`Warning`, `warning`).
  * **Green**: Execution success (`SUCCESSFUL`, `Succeeded`, `success`).
* **Tools**: Features quick "Clear", "Copy All", and "Auto-Scroll" controls.

---

## Directory Structure

* `src/core/`: Application engine controller, workspace file indexer, and state storage.
* `src/ui/`: UI manager, themes (Slate Dark, Cyberpunk, Monokai, GitHub Light), and panels.
* `src/ui/panels/`: View layouts for `EditorPanel`, `FileExplorerPanel`, `TerminalPanel`, and `OutputPanel`.
* `src/ui/terminal/`: Win32 ConPTY wrappers, terminal emulator layout cell matrix, scrollback handles, and Git macros helper.
* `src/utils/`: Common system helpers (Logger, file reader, etc.).
* `resources/`: Application assets and Windows resource compiler scripts (executable `.ico` icon and `.rc` configuration).

---

## Build Instructions (Windows)

code ZEN compiles on Windows using **MSVC (Microsoft Visual C++)** and the **Ninja** build system via a provided helper script `build.bat`.

### Prerequisites
1. **Visual Studio 2022**: Ensure the "Desktop development with C++" workload is checked.
2. **CMake**: Version 3.15 or later installed and added to your system `PATH`.
3. **Ninja**: Ninja is recommended for fast builds (and is included by default in Visual Studio's CMake tools).

### Compiling and Bundling
1. Open a standard Windows Command Prompt (`cmd.exe`).
2. Run the bootstrap compile script:
   ```cmd
   build.bat
   ```
3. **What `build.bat` does**:
   * Resolves Visual Studio's internal C++ compiler variables via `vcvarsall.bat x64`.
   * Appends Visual Studio's embedded CMake/Ninja to your environment PATH if system ones are missing.
   * Runs CMake configuration with offline-friendly dependency resolution (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`).
   * Fetches required third-party libraries (GLFW, Dear ImGui Docking) over Git or cached downloads.
   * Compiles code-ZEN with multi-threaded optimizations.
   * Copies the compiled binary output to `build\Release\code-ZEN.exe`.

---

## How to Run

After compiling, the release executable will be available at:
`build\Release\code-ZEN.exe`

Launch the app from the root directory or your terminal:
```cmd
.\build\Release\code-ZEN.exe
```

### Basic Usage Instructions
* **Changing Theme**: Go to the top menu bar **Theme** and select from *Slate Dark*, *Cyberpunk*, *Monokai*, or *GitHub Light*.
* **Creating Terminals**: Open the *Integrated Terminal* panel. Click the **+** tab button to open a terminal, or right-click within a terminal to split it horizontally or vertically.
* **Running Code**: Open any code file in the *Code Editor*. Click **Run** on the editor toolbar. You can edit build/run commands for the active file extension under **Runner Settings** (accessible via the Gear icon in the editor toolbar).