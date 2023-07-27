; responsible for generating all possible legal 16-bit bus states (as individual letters)

(deftemplate node
             (slot title
                   (type SYMBOL)
                   (default ?NONE))
             (slot width
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE)))
(deftemplate transition
             (slot from
                   (type SYMBOL)
                   (default ?NONE))
             (slot to
                   (type SYMBOL)
                   (default ?NONE)))
(deffacts bus-states
          ;(defnode nil 0)
          (defnode A 8)
          (defnode B 8)
          (defnode C 8)
          (defnode D 8)
          (defnode E 16)
          (defnode F 16)
          ;(defstate A -> nil)
          ;(defstate B -> nil)
          (defstate B -> F)
          (defstate B -> C)
          ;(defstate C -> nil)
          ;(defstate D -> nil)
          (defstate D -> E)
          (defstate D -> A)
          ;(defstate E -> nil)
          (defstate E -> F)
          (defstate E -> C)
          ;(defstate F -> nil)
          (defstate F -> E)
          (defstate F -> A)
          (max-path-length 8))

(defrule make-node
         (declare (salience 10000))
         ?f <- (defnode ?name ?cost)
         =>
         (retract ?f)
         (assert (node (title ?name)
                       (width ?cost))))

(defrule make-transition
         (declare (salience 10000))
         ?f <- (defstate ?from -> ?to)
         =>
         (retract ?f)
         (assert (transition (from ?from)
                             (to ?to))))

(defrule generate-initial-path
         ?f <- (node (title ?name))
         =>
         (assert (path ?name)))
(defrule walk-path
         (path $?before ?curr)
         (transition (from ?curr)
                     (to ?next))
         (max-path-length ?length)
         (test (<= (+ (length$ ?before)
                      1)
                   ?length))
         =>
         (assert (path $?before ?curr ?next)))

