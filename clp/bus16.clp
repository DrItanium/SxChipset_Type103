; responsible for generating all possible legal 16-bit bus states (as individual letters)
(deftemplate group-properties
             (slot title 
                   (type SYMBOL)
                   (default ?NONE))
             (slot max-length
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE)))
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
                        (default ?NONE))
             (slot valid
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE)))
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
(deftemplate path-expansion
             (slot title
                   (type SYMBOL)
                   (default-dynamic (gensym*)))
             (slot group
                   (type SYMBOL)
                   (default ?NONE))
             (slot width
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE))
             (multislot original
                        (type SYMBOL)
                        (default ?NONE))
             (multislot expansion
                        (type SYMBOL)))
(deftemplate distinct-path-match
             (slot target0
                   (type SYMBOL)
                   (default ?NONE))
             (slot target1
                   (type SYMBOL)
                   (default ?NONE))
             (multislot match
                        (default ?NONE)))

(deftemplate smaller-to-larger-mapping
             (slot standalone
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot can-flow-in
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot can-flow-out
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot smaller
                   (type SYMBOL)
                   (default ?NONE))
             (slot larger
                   (type SYMBOL)
                   (default ?NONE))
             (multislot match-before 
                        (type SYMBOL)
                        (default ?NONE))
             (multislot match 
                        (type SYMBOL)
                        (default ?NONE))
             (multislot match-after
                        (type SYMBOL)
                        (default ?NONE)))
(deftemplate stage
             (slot current
                   (type SYMBOL)
                   (default ?NONE))
             (multislot rest
                        (type SYMBOL)))
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
          (defnode bus32 TRUE  TRUE  FALSE FALSE)
          (defnode bus32 FALSE FALSE TRUE  TRUE)
          (defnode bus32 FALSE TRUE  TRUE  FALSE)
          (defnode bus32 TRUE  TRUE  TRUE  FALSE)
          (defnode bus32 FALSE TRUE  TRUE  TRUE)
          (defnode bus32 TRUE  TRUE  TRUE  TRUE)
          (group-properties (title bus32)
                            (max-length 4))
          )
(defrule next-stage
         (declare (salience -10000))
         ?f <- (stage (rest ?next $?rest))
         =>
         (modify ?f 
                 (current ?next)
                 (rest $?rest)))
(deffacts stages
          (stage (current construct)
                 (rest generate
                       infer)))
(defrule make-node
         (stage (current construct))
         (defnode ?group ?cont $?middle ?end)
         =>
         (bind ?size 
               (+ (if ?cont then 8 else 0)
                  (if ?end then 8 else 0)))
         (bind ?is-valid
               (or ?cont
                   ?end))
         (progn$ (?v ?middle)
                 (bind ?is-valid
                       (or ?is-valid
                           ?v))
                 (bind ?size
                       (+ ?size
                          (if ?v then 8 else 0))))
         (assert (node (group ?group)
                       (title (gensym*))
                       (width ?size)
                       (valid ?is-valid)
                       (byte-enable-bits ?cont 
                                         $?middle 
                                         ?end)
                       (link-to-next ?end)
                       (continue-from-prev ?cont))))

(defrule make-custom-transition
         (stage (current construct))
         (node (group ?group)
               (title ?from)
               (link-to-next TRUE))
         (node (group ?group)
               (title ?to)
               (continue-from-prev TRUE))
         =>
         (assert (defstate ?group ?from -> ?to)))

(defrule make-transition
         (stage (current construct))
         (defstate ?group ?from -> ?to)
         =>
         (assert (transition (group ?group)
                             (from ?from)
                             (to ?to))))

(defrule generate-initial-path
         (stage (current generate))
         (node (title ?name)
               (group ?group)
               (width ?width))
         =>
         (assert (path (group ?group)
                       (contents ?name)
                       (width ?width))))

(defrule walk-path
         (stage (current generate))
         ?f <- (path (group ?group)
                     (contents $?before 
                               ?curr)
                     (width ?width))
         (transition (group ?group)
                     (from ?curr)
                     (to ?next))
         (node (title ?next)
               (group ?group)
               (width ?cost))
         (test (<= (+ ?cost ?width) 128))
         (group-properties (title ?group)
                           (max-length ?length))
         (test (< (+ (length$ ?before) 1) ?length))
         =>
         (duplicate ?f 
                    (width (+ ?width 
                              ?cost))
                    (contents $?before
                              ?curr
                              ?next)))


(defrule map-smaller-to-larger 
         "map the smaller bus width to the larger ones"
         (stage (current infer))
         (node (title ?title)
               (group ?g0)
               (byte-enable-bits $?bits))
         (node (title ?title2)
               (group ~?g0)
               (byte-enable-bits $?a $?bits $?b))
         =>
         (assert (smaller-to-larger-mapping (smaller ?title)
                                            (larger ?title2)
                                            (match-before ?a)
                                            (match ?bits)
                                            (match-after ?b))))
(defrule update-mapping-flow-in0 
         (stage (current infer))
         ?f <- (smaller-to-larger-mapping (can-flow-in UNKNOWN)
                                          (match-before ?start $?))
         =>
         (modify ?f
                 (can-flow-in ?start)))

(defrule update-mapping-flow-in1
         (stage (current infer))
         ?f <- (smaller-to-larger-mapping (can-flow-in UNKNOWN)
                                          (match-before)
                                          (match ?start $?))
         =>
         (modify ?f
                 (can-flow-in ?start)))

(defrule update-mapping-flow-out0 
         (stage (current infer))
         ?f <- (smaller-to-larger-mapping (can-flow-out UNKNOWN)
                                          (match-after $? ?end))
         =>
         (modify ?f
                 (can-flow-out ?end)))

(defrule update-mapping-flow-out1
         (stage (current infer))
         ?f <- (smaller-to-larger-mapping (can-flow-out UNKNOWN)
                                          (match-after)
                                          (match $? ?end))
         =>
         (modify ?f
                 (can-flow-out ?end)))

(defrule update-mapping-standalone
         (stage (current infer))
         ?f <- (smaller-to-larger-mapping (standalone UNKNOWN)
                                          (can-flow-in ?in&:(neq ?in UNKNOWN))
                                          (can-flow-out ?out&:(neq ?out UNKNOWN)))
         =>
         (modify ?f 
                 (standalone (and (not ?in)
                                  (not ?out)))))


(defrule make-path-expansion
         (stage (current infer))
         (path (group ?group)
               (width ?width)
               (contents $?contents))
         =>
         (assert (path-expansion (group ?group)
                                 (width ?width)
                                 (original ?contents))))
(defrule expand-path
         (stage (current infer))
         ?f <- (path-expansion (group ?group)
                               (original ?first 
                                         $?rest)
                               (expansion $?exp))
         (node (group ?group)
               (title ?first)
               (byte-enable-bits $?bits))
         =>
         (modify ?f
                 (original $?rest)
                 (expansion $?exp 
                            $?bits)))
(defrule match-two-distinct-paths
         (stage (current infer))
         (path-expansion (original)
                         (expansion $?exp)
                         (title ?name))
         (path-expansion (original)
                         (expansion $?exp)
                         (title ?other&~?name))
         =>
         (assert (distinct-path-match (target0 ?name)
                                      (target1 ?other)
                                      (match $?exp))))

(defrule eliminate-duplicate-matches
         (stage (current infer))
         ?f <- (distinct-path-match (target0 ?a)
                                    (target1 ?b)
                                    (match $?m))
         (distinct-path-match (target0 ?b)
                              (target1 ?a)
                              (match $?m))
         =>
         (retract ?f))

