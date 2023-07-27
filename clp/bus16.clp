; responsible for generating all possible legal 16-bit bus states (as individual letters)
(deftemplate group-properties
             (slot title 
                   (type SYMBOL)
                   (default ?NONE))
             (slot max-length
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE))
             (slot max-size
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default-dynamic 128)))
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
                   (default ?NONE))
             (slot link-to-next
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot continue-from-prev 
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (multislot byte-enable-bits
                        (type SYMBOL)
                        (allowed-symbols FALSE
                                         TRUE)
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
(deffacts states
          (defnode bus16 TRUE FALSE)
          (defnode bus16 FALSE TRUE)
          (defnode bus16 TRUE TRUE)
          (group-properties (title bus16)
                            (max-length 8))
          (defnode bus32 TRUE  FALSE FALSE FALSE)
          (defnode bus32 FALSE TRUE  FALSE FALSE)
          (defnode bus32 FALSE FALSE TRUE  FALSE)
          (defnode bus32 FALSE FALSE FALSE TRUE)
          (defnode bus32 TRUE TRUE FALSE FALSE)
          (defnode bus32 FALSE FALSE TRUE TRUE)
          (defnode bus32 FALSE TRUE TRUE FALSE)
          (defnode bus32 TRUE TRUE TRUE FALSE)
          (defnode bus32 FALSE TRUE TRUE TRUE)
          (defnode bus32 TRUE TRUE TRUE TRUE)
          (group-properties (title bus32)
                            (max-length 4))

          (defnode bus64 TRUE  TRUE  FALSE FALSE FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE TRUE  TRUE  FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE TRUE  TRUE  FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE FALSE FALSE TRUE  TRUE )
          (defnode bus64 TRUE  TRUE  TRUE TRUE FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE TRUE  TRUE  TRUE TRUE)
          (defnode bus64 TRUE  TRUE  TRUE TRUE TRUE TRUE TRUE TRUE)
          (defnode bus64 FALSE TRUE  TRUE TRUE TRUE TRUE TRUE TRUE)
          (defnode bus64 TRUE  TRUE TRUE TRUE TRUE TRUE TRUE FALSE)
          (defnode bus64 TRUE  FALSE FALSE FALSE FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE TRUE  FALSE FALSE FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE TRUE  FALSE FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE TRUE  FALSE FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE TRUE  FALSE FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE FALSE TRUE  FALSE FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE FALSE FALSE TRUE  FALSE)
          (defnode bus64 FALSE FALSE FALSE FALSE FALSE FALSE FALSE TRUE )
          (group-properties (title bus64)
                            (max-length 2))
          )

(defrule make-node
         (declare (salience 10000))
         ?f <- (defnode ?group ?cont $?middle ?end)
         =>
         (retract ?f)
         (bind ?size 
               (+ (if ?cont then 8 else 0)
                  (if ?end then 8 else 0)))
         (progn$ (?v ?middle)
                 (bind ?size
                       (+ ?size
                          (if ?v then 8 else 0))))
         
         (assert (node (group ?group)
                       (title (gensym*))
                       (width ?size)
                       (byte-enable-bits ?cont 
                                         $?middle 
                                         ?end)
                       (link-to-next ?end)
                       (continue-from-prev ?cont))))

(defrule make-custom-transition
         (declare (salience 10000))
         (node (group ?group)
               (title ?from)
               (link-to-next TRUE))
         (node (group ?group)
               (title ?to)
               (continue-from-prev TRUE))
         =>
         (assert (defstate ?group ?from -> ?to)))

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
         (group-properties (title ?group)
                           (max-length ?length))
         (test (< (+ (length$ ?before)
                     1)
                  ?length))
         =>
         (duplicate ?f 
                    (width (+ ?width ?cost))
                    (contents $?before
                              ?curr
                              ?next)))


