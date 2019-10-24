;;;;
;;;; Standard library
;;;;
(def-module :system)

;;; function composition
(export 'id 'compose 'curry 'flip 'negate)

(def (id x) x)
(def (compose f g) (fn (x) (f (g x))))
(def (curry f &rest curried) (fn (&rest args) (apply f (++ curried args))))
(def (flip f) (fn (&rest args) (apply f (reverse args))))
(def (negate f) (fn (&rest args) (not (apply f args))))

;;; boolean operations
(export 'or 'and 'not '!=)

(def-macro (or a b) `(if ,a true ,b))
(def-macro (and a b) `(if ,a ,b false))
(def (or a b) (if a true b))
(def (and a b) (if a b false))
(def (not a) (if a false true))
(def != (negate =))

;;; vector operations
(export 'vector 'empty? 'head 'tail 'fill 'reverse 'map 'foldl 'foldr 'range 'iota 'flatten)

(def (vector &rest xs) xs)

(def (empty? xs) (= (length xs) 0))

(def (head xs) (get 0 xs))

(def (tail xs) (slice 1 (- (length xs) 1) xs))

(def (fill n x) (tabulate n (fn (i) x)))

(def (reverse xs)
     (let ((n (length xs)))
       (tabulate n (fn (i) (get (- n i 1) xs)))))

(def (map f xs)
     (tabulate (length xs) (fn (i) (f (get i xs)))))

(def (foldl f init xs)
  (if (empty? xs) init (foldl f (f (head xs) init) (tail xs))))

(def (foldr f init xs)
  (if (empty? xs) init (f (head xs) (foldr f init (tail xs)))))

(def (range start end)
     (if (< end start)
       (vector)
       (tabulate (- end start -1) (fn (i) (+ start i)))))

(def iota (curry range 1))

(def flatten (curry apply ++))

;;; array operations
(export 'fill-array)

(def (array &rest xs) (tabulate-array (length xs) (fn (i) (get i xs))))

(def (fill-array n x) (tabulate-array n (fn (i) x)))
