; json, after the grammar's own query.
; The later pattern wins here, and the grammar's query colours every string
; after it colours keys: so the keys are said again, last.

(pair
  key: (string) @string.special.key)
