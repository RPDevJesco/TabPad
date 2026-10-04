; python, after the grammar's own query.
; The later pattern wins here, and the grammar's query calls every name
; behind a dot a property after it has called the ones that are called
; methods: so the methods are said again, last.

(call
  function: (attribute attribute: (identifier) @function.method))
