# TabPad

A tabbed text editor for Windows, MacOS, Linux and the browser that takes its inspiration from Notepad++.

TabPad opens fast, keeps every tab you had open, and never asks you to save before you close it. Unsaved text is still there the next time you start it, even after a crash or a power cut.

## Why it exists

A text editor should be a text editor.

- **It is only software.** TabPad carries no messages, no news and no opinions. Its release notes say what changed in the program and nothing else.
- **It talks to nobody.** There are no update checks, no telemetry and no accounts. The desktop program never opens a network connection. The browser version is built so that it cannot.
- **It is cross-platform by design.** The same editor runs on Windows, on Linux and in a browser tab. The editor itself is plain C with no operating system in it; each platform is a thin layer underneath. macOS builds from the same source and has not been tested yet.
- **It does not lose your work.** Tabs, unsaved text, the caret and the scroll position are written down as you type, not when you remember to save.

## Features

### Editing

- Tabs, with unsaved tabs kept across restarts
- Undo and redo for every tab
- Find, replace and replace all, with match case, whole word and regular expressions
- Groups in a regex replacement: `$0` to `$9`, plus `\n`, `\t` and `$$`
- Go to line
- Duplicate, delete and move lines; indent and unindent; upper and lower case; trim trailing space
- Overwrite mode
- Word wrap
- Lines of any length: a line longer than 4096 characters continues on the next display line

### Files

- UTF-8, UTF-8 with BOM, UTF-16 LE, UTF-16 BE and ANSI, detected on open and convertible from the Encoding menu
- Windows, Unix and old Macintosh line ends, kept as found and convertible from the Edit menu
- A file that changed on disk while you had unsaved edits is flagged, and neither version is thrown away
- A second TabPad started with a file hands the file to the one already running

### Languages

Syntax colouring by Tree-sitter grammars:

C, C++, C#, CSS, JavaScript, HTML, Lua, Salesforce Apex, Python, Ruby, Rust, Markdown, JSON, XML, Bash, PowerShell

Also:

- **CSV and TSV** with a colour for each column
- **HTML** with its `<script>` and `<style>` coloured as JavaScript and CSS
- **Markdown** with emphasis, code spans and links, and each fenced code block coloured in the language it names
- **More languages by folder.** Put a folder holding a Tree-sitter grammar library, its `highlights.scm` and a small `language.ini` into `languages`, and TabPad has that language the next time it starts. Queries written for Neovim load too.

### Look

- Text font and interface font chosen separately from the fonts installed on the machine
- The system's own fonts by default; a font carried inside the program when the machine has none
- Follows the display scale, fractional scales included, and a scale that changes while it runs
- Zoom for the text alone
- Toolbar, status bar, line numbers and visible spaces and tabs, each switchable
- The interface in English or Simplified Chinese; a translation is one text file anyone can add

### Status bar

File kind, length, lines, line, column, position, selection, line ends, encoding, insert or overwrite.

### System

- Registers itself as a program that opens text files: `tabpad --register`, undone with `tabpad --unregister`
- Settings are kept beside the session and need no configuration file

## The browser version

`tabpad.html` is the whole editor in one file. Open it from disk or serve it from anywhere.

- It sends nothing and fetches nothing. The page forbids every network connection through its Content-Security-Policy, and its script is pinned by hash.
- Tabs, text and settings stay in that browser on that machine.
- **File > Erase Everything Kept Here...** removes all of it.
- Ctrl+N, Ctrl+W, Ctrl+T and Ctrl+Tab belong to the browser. Use the menus, the toolbar or the tab bar for those.
- Save writes the file in place where the browser allows it, and hands it back as a download where it does not.

Tested in Firefox, Chrome and Edge.

## Shortcuts

On macOS, Cmd works in place of Ctrl.

### File

| Keys | Does |
| --- | --- |
| Ctrl+N | New tab |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Alt+S | Save as |
| Ctrl+Shift+S | Save all |
| Ctrl+W | Close tab |

### Edit

| Keys | Does |
| --- | --- |
| Ctrl+Z | Undo |
| Ctrl+Y, Ctrl+Shift+Z | Redo |
| Ctrl+X, Ctrl+C, Ctrl+V | Cut, copy, paste |
| Ctrl+A | Select all |
| Ctrl+D | Duplicate line |
| Ctrl+Shift+L | Delete line |
| Ctrl+Shift+Up, Ctrl+Shift+Down | Move line up, down |
| Tab, Shift+Tab | Indent, unindent the selected lines |
| Ctrl+Shift+U | UPPERCASE |
| Ctrl+U | lowercase |
| Ins | Overwrite mode |

### Search

| Keys | Does |
| --- | --- |
| Ctrl+F | Find |
| F3, Shift+F3 | Find next, previous |
| Enter, Shift+Enter | Next, previous match while in the find field |
| Ctrl+H | Replace |
| Ctrl+G | Go to line |
| Esc | Close the panel |

### View

| Keys | Does |
| --- | --- |
| Ctrl++, Ctrl+- | Zoom in, out |
| Ctrl+0 | Restore default zoom |
| Ctrl+wheel | Zoom |
| Alt+Z | Word wrap |

### Tabs and menus

| Keys | Does |
| --- | --- |
| Ctrl+Tab, Ctrl+PageDown | Next tab |
| Ctrl+Shift+Tab, Ctrl+PageUp | Previous tab |
| Alt+F, E, S, V, N, L, W | Open the File, Edit, Search, View, Encoding, Language, Window menu |
| Arrows, Enter, Esc | Move in a menu, choose, close |

### Moving and selecting

| Keys | Does |
| --- | --- |
| Arrows | Move |
| Ctrl+Left, Ctrl+Right | Move by word |
| Home, End | Start, end of the line as shown; again for the whole line |
| Ctrl+Home, Ctrl+End | Start, end of the text |
| PageUp, PageDown | Move by a screen |
| Shift with any of these | Select |
| Ctrl+Backspace, Ctrl+Delete | Delete a word |
| Double click, triple click | Select a word, a line |
