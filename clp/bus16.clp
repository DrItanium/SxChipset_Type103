; responsible for generating all possible legal 16-bit bus states (as individual letters)

(deftemplate node
             (slot group
                   (type SYMBOL)
                   (default ?NONE))
             (slot title
                   (type SYMBOL)
                   (default ?NONE))
             (slot width
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE)))
(deftemplate transition
             (slot group
                   (type SYMBOL)
                   (default ?NONE))
             (slot from
                   (type SYMBOL)
                   (default ?NONE))
             (slot to
                   (type SYMBOL)
                   (default ?NONE)))
(deftemplate path
             (slot width
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default-dynamic 0))
             (slot group
                   (type SYMBOL)
                   (default ?NONE))
             (multislot contents
                        (default ?NONE)))
(deffacts bus16-states
          (defnode bus16 A 8)
          (defnode bus16 B 8)
          (defnode bus16 C 8)
          (defnode bus16 D 8)
          (defnode bus16 E 16)
          (defnode bus16 F 16)
          (defstate bus16 B -> F)
          (defstate bus16 B -> C)
          (defstate bus16 D -> E)
          (defstate bus16 D -> A)
          (defstate bus16 E -> F)
          (defstate bus16 E -> C)
          (defstate bus16 F -> E)
          (defstate bus16 F -> A)
          (max-path-length bus16 8))
(deffacts bus32-states
          (defnode bus32 A 8)
          (defnode bus32 B 8)
          (defnode bus32 C 8)
          (defnode bus32 D 8)
          (defnode bus32 E 16)
          (defnode bus32 F 16)
          (defnode bus32 G 32)
          (defnode bus32 H 16)
          (defnode bus32 I 24)
          (defnode bus32 J 24)
          (defstate bus32 D -> A)
          (defstate bus32 D -> E)
          (defstate bus32 D -> I)
          (defstate bus32 D -> G)

          (defstate bus32 F -> A)
          (defstate bus32 F -> E)
          (defstate bus32 F -> I)
          (defstate bus32 F -> G)

          (defstate bus32 G -> A)
          (defstate bus32 G -> E)
          (defstate bus32 G -> I)
          (defstate bus32 G -> G)

          (defstate bus32 J -> A)
          (defstate bus32 J -> E)
          (defstate bus32 J -> I)
          (defstate bus32 J -> G)

          (max-path-length bus32 4))

(defrule make-node
         (declare (salience 10000))
         ?f <- (defnode ?group ?name ?cost)
         =>
         (retract ?f)
         (assert (node (group ?group)
                       (title ?name)
                       (width ?cost))))

(defrule make-transition
         (declare (salience 10000))
         ?f <- (defstate ?group ?from -> ?to)
         =>
         (retract ?f)
         (assert (transition (group ?group)
                             (from ?from)
                             (to ?to))))

(defrule generate-initial-path
         ?f <- (node (title ?name)
                     (group ?group)
                     (width ?width))
         =>
         (assert (path (group ?group)
                       (contents ?name)
                       (width ?width))))

(defrule walk-path
         ?f <- (path (group ?group)
                     (contents $?before ?curr)
                     (width ?width))
         (transition (group ?group)
                     (from ?curr)
                     (to ?next))
         (node (title ?next)
               (group ?group)
               (width ?cost))
         (max-path-length ?group 
                          ?length)
         (test (< (+ (length$ ?before)
                     1)
                  ?length))
         =>
         (duplicate ?f 
                    (width (+ ?width ?cost))
                    (contents $?before
                              ?curr
                              ?next)))


