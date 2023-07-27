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
             (slot be0
                   (type SYMBOL)
                   (allowed-symbols FALSE
                                    TRUE)
                   (default ?NONE))
             (slot be1
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
(deffacts bus16-states
          (defnode bus16 A 8 TRUE FALSE)
          (defnode bus16 B 8 FALSE TRUE)
          (defnode bus16 C 16 TRUE TRUE)
          (max-path-length bus16 8))

(defrule make-node
         (declare (salience 10000))
         ?f <- (defnode ?group ?name ?cost ?be0 ?be1)
         =>
         (retract ?f)
         (assert (node (group ?group)
                       (title ?name)
                       (width ?cost)
                       (be0 ?be0)
                       (be1 ?be1)
                       (link-to-next ?be1)
                       (continue-from-prev ?be0))))

(defrule make-custom-transition
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
         (defstate ?group ?from -> ?to)
         =>
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


