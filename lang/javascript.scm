; javascript, after the grammar's own query.
; The grammar's query names these only when they are not local variables,
; which this highlighter cannot know and so never matches. They are rarely
; shadowed: named here without the test.

((identifier) @variable.builtin
 (#match? @variable.builtin "^(arguments|module|console|window|document)$"))

((identifier) @function.builtin
 (#eq? @function.builtin "require"))
