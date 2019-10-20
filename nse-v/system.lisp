;;;;
;;;; Standard library
;;;;
(def-module :system)

;;; function composition
(export 'compose, 'curry 'flip)

(def (compose f g) (fn (x) (f (g x))))
(def (curry f &rest curried) (fn (&rest args) (apply f (++ curried args))))
(def (flip f) (fn (&rest args) (apply f (reverse args))))
(def (negate f) (fn (&rest args) (not (apply f args))))

;;; boolean operations
(export 'or 'and 'not '!= 'cond)

(def-macro (or a b) `(if ,a true ,b))
(def-macro (and a b) `(if ,a ,b false))
(def (or a b) (if a true b))
(def (and a b) (if a b false))
(def (not a) (if a false true))
(def != (negate =))

;;; vector operations
(export 'vector 'fill 'reverse 'map 'range 'iota 'flatten)

(def (vector &rest xs) xs)

(def (fill n x) (tabulate n (fn (i) x)))

(def (reverse xs)
     (let ((n (length xs)))
       (tabulate n (fn (i) (get (- n i 1) xs)))))

(def (map f xs)
     (tabulate (length xs) (fn (i) (f (get i xs)))))

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
