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
             (slot num-nodes
                   (type INTEGER)
                   (range 0 ?VARIABLE)
                   (default ?NONE))
             (slot absolute-terminator
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot absolute-starter
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot standalone
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot maximal-closed
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot mapped-to-actual-operation
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot unaligned 
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
             (slot must-span-transactions
                   (type SYMBOL)
                   (allowed-symbols UNKNOWN
                                    FALSE
                                    TRUE))
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
(deffacts bus-groups
          (busgen bus16 2 8)
          (busgen bus32 4 4)
          )
(deffacts stages
          (stage (current construct)
                 (rest cleanup
                       generate
                       infer)))

(deffacts operation-mappings
          (valid-op-width 128)
          (valid-op-width 96)
          (valid-op-width 64)
          (valid-op-width 32)
          (valid-op-width 16)
          (valid-op-width 8)
          )
(defrule next-stage
         (declare (salience -10000))
         ?f <- (stage (rest ?next $?rest))
         =>
         (modify ?f 
                 (current ?next)
                 (rest $?rest)))
(defrule make-node
         (stage (current construct))
         ?f <- (defnode ?group ?cont $?middle ?end)
         =>
         (retract ?f)
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
         (group-properties (title ?group)
                           (max-length ?len&:(> ?len 1)))
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
         ?f <- (defstate ?group ?from -> ?to)
         =>
         (retract ?f)
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


(defrule make-path-expansion
         (stage (current infer))
         ?f <- (path (group ?group)
                     (width ?width)
                     (contents $?contents))
         =>
         (retract ?f)
         (assert (path-expansion (group ?group)
                                 (width ?width)
                                 (num-nodes (length$ ?contents))
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



(deffunction generate-bus-recursive
             (?title ?depth ?contents)
             (if (<= ?depth 0) then
               (assert (defnode ?title ?contents))
               else
               (progn$ (?v (create$ FALSE
                                    TRUE))
                       (generate-bus-recursive ?title
                                               (- ?depth 1)
                                               (create$ ?contents
                                                        ?v)))))

(deffunction generate-bus-full
             (?title ?depth ?length)
             (assert (group-properties (title ?title)
                                       (max-length ?length)))
             (generate-bus-recursive ?title
                                     ?depth
                                     (create$)))

(defrule generate-bus-group
         (stage (current construct))
         ?f <- (busgen ?title 
                       ?depth
                       ?length)
         =>
         (retract ?f)
         (generate-bus-full ?title
                            ?depth
                            ?length))

(defrule eliminate-empty-paths
         (stage (current cleanup))
         ?f <- (node (width 0))
         =>
         (retract ?f))

(defrule eliminate-transitions-with-bad-from-field
         (stage (current cleanup))
         ?f <- (transition (from ?from))
         (not (node (title ?from)))
         =>
         (retract ?f))

(defrule eliminate-transitions-with-bad-to-field
         (stage (current cleanup))
         ?f <- (transition (to ?from))
         (not (node (title ?from)))
         =>
         (retract ?f))

(defrule find-illegal-match-group
         (stage (current cleanup))
         ?f <- (node (byte-enable-bits $?  TRUE $? FALSE $?  TRUE $?))
         =>
         (retract ?f))

(defrule mark-path-as-fully-standalone
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (standalone UNKNOWN)
                               (absolute-terminator ?a&~UNKNOWN)
                               (absolute-starter ?b&~UNKNOWN))
         =>
         (modify ?f
                 (standalone (and ?a ?b))))

(defrule mark-path-as-absolute-terminator
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (expansion $? ?k)
                               (absolute-terminator UNKNOWN))
         =>
         (modify ?f
                 (absolute-terminator (not ?k))))

(defrule mark-path-as-absolute-starter
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (expansion ?k $?)
                               (absolute-starter UNKNOWN))
         =>
         (modify ?f
                 (absolute-starter (not ?k))))

(defrule mark-path-as-maximally-closed
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (group ?group)
                               (num-nodes ?nn)
                               (maximal-closed UNKNOWN)
                               (expansion $?exp))
         (group-properties (title ?group)
                           (max-length ?ml))
         =>
         (modify ?f 
                 (maximal-closed (and (= ?nn ?ml)
                                      (expand$ ?exp)))))

(defrule mark-path-as-mapped-valid-op-width
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (mapped-to-actual-operation UNKNOWN)
                               (width ?width))
         (valid-op-width ?width)
         =>
         (modify ?f
                 (mapped-to-actual-operation TRUE)))

(defrule mark-path-as-not-mapped-to-valid-op-width
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (mapped-to-actual-operation UNKNOWN)
                               (width ?width))
         (not (valid-op-width ?width))
         =>
         (modify ?f
                 (mapped-to-actual-operation FALSE)))

(defrule eliminate-entries-which-are-standalone-but-not-possibly-mapped
         (stage (current infer))
         ?f <- (path-expansion (mapped-to-actual-operation FALSE)
                               (standalone TRUE)
                               (maximal-closed FALSE))
         =>
         (retract ?f))

(defrule check-if-path-is-unaligned 
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (mapped-to-actual-operation ?result&~UNKNOWN)
                               (unaligned UNKNOWN)
                               (expansion $?x))
         =>
         (modify ?f
                 (unaligned (and ?result 
                                 (not (and (expand$ ?x)))))))

(defrule valid-if-spanning-transactions
         (stage (current infer))
         ?f <- (path-expansion (original)
                               (mapped-to-actual-operation ?a&~UNKNOWN)
                               (must-span-transactions UNKNOWN))
         =>
         (modify ?f
                 (must-span-transactions (not ?a))))
