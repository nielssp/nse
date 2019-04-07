;;;;
;;;; Self-hosted standard library
;;;;
(in-module :system)

;;; stdio
(export 'print 'println 'get-char)

(def (print str &key (to *stdout*))
     "Prints str to standard output"
     (stream-write str to))
(def (println str &key (to *stdout*))
     "Prints str and a newline to standard output"
     (stream-write str to)
     (stream-write "\n" to))

(def (get-byte &key (from *stdin*))
     "Reads a single byte from standard input"
     (elem-at 0 (stream-read 1 from)))

;;; function composition
(export 'compose 'curry 'flip 'negate 'pipe)

(def (compose f g) (fn (x) (f (g x))))
(def (curry f &rest curried) (fn (&rest args) (apply f (++ curried args))))
(def (flip f) (fn (x y) (f y x)))
(def (negate f) (fn (&rest args) (not (apply f args))))
(def-macro (pipe x &rest funcs) (foldl list x funcs))

;;; list fundamentals
(export 'nil 'nil? 'list 'cons 'head 'tail)

(def nil '())
(def (nil? xs) (= xs nil))
(def (list &rest xs) xs)
(def (cons head tail) (list head . tail))
(def (head &match (h . t)) h)
(def (tail &match (h . t)) t)

;;; boolean
(export 'or 'and 'not '!= 'cond)

(def-macro (or a b) (list 'if a 'true b))
(def-macro (and a b) (list 'if a b 'false))
(def (or a b) (or a b))
(def (and a b) (and a b))
(def (not a) (if a false true))
(def != (negate =))

(def-macro (cond &rest cases) (if (nil? cases) ''nil (list 'if (head (head cases)) (elem 1 (head cases)) (cons 'cond (tail cases)))))

;;; list operations
(export 'length 'reverse 'elem 'drop 'take 'slice 'map '++ 'filter 'foldl
        'foldr 'reduce 'sum 'product 'range 'iota 'zip-with 'zip 'flatten)

(def (length xs)
  (let ((rec (fn (xs acc) (if (nil? xs) acc (rec (tail xs) (+ 1 acc))))))
    (rec xs 0)))

(def (reverse xs)
     (let ((rec (fn (xs acc) (if (nil? xs) acc (rec (tail xs) (cons (head xs) acc))))))
       (rec xs nil)))

(def (elem n xs) (if (= n 0) (head xs) (elem (- n 1) (tail xs))))

(def (drop n xs) (if (= n 0) xs (drop (- n 1) (tail xs))))

(def (take n xs) (if (= n 0) nil (cons (head xs) (take (- n 1) (tail xs)))))

(def (slice i length xs) (take length (drop i xs)))

(def (map f xs) (if (nil? xs) nil (cons (f (head xs)) (map f (tail xs)))))

(def (++ xs ys)
  (if (nil? ys) xs
    (let ((rec (fn (xs acc) (if (nil? xs) acc (rec (tail xs) (cons (head xs) acc))))))
      (rec (reverse xs) ys))))

(def (filter f xs)
     (if (nil? xs) '()
       (if (f (head xs)) (cons (head xs) (filter f (tail xs))) (filter f (tail xs)))))

(def (foldl f init xs)
  (if (nil? xs) init (foldl f (f (head xs) init) (tail xs))))

(def (foldr f init xs)
  (if (nil? xs) init (f (head xs) (foldr f init (tail xs)))))

(def (reduce f xs)
  (cond
    ((nil? xs) (f))
    ((nil? (tail xs)) (head xs))
    (true (f (head xs) (reduce f (tail xs))))))

(def (sum xs) (apply + xs))

(def (product xs) (apply * xs))

(def (range start end)
  (let acc '())
  (recur (start end acc)
    (if (= start end)
      (cons start acc)
      (continue start (- end 1) (cons end acc)))))

(def iota (curry range 1))

(def (zip-with f  xs ys)
  (if (or (nil? xs) (nil? ys)) '()
    (cons (f (head xs) (head ys)) (zip-with f (tail xs) (tail ys)))))

(def (zip xs ys) (zip-with list xs ys))

(def (flatten xs) (foldr ++ nil xs))

(def (flatten' xs)
  (let ((rec (fn (xs rec) (if (nil? xs) rec (rec (tail xs) (++ rec (head xs)))))))
    (rec xs '())))

;;; read monad
(export 'read>>= 'read>>)

(def read-char 'read-char)
(def read-string 'read-string)
(def read-symbol 'read-symbol)
(def read-int 'read-int)
(def read-any 'read-any)
(def read-ignore 'read-ignore)
(def (read-return v) (list 'read-return v))
(def (read>>= r f) (list 'read-bind r f))
(def (read>> r1 r2) (list 'read-bind r1 (fn (v) r2)))

;;; multiline comment
(export '|)

(def-read-macro |
  (let ((read-bars (read>>= read-char (fn (c) (if (= c (ascii "#")) read-ignore (if (= c (ascii "|")) read-bars read-until-bar)))))
        (read-until-bar (read>>= read-char (fn (c) (if (= c (ascii "|")) read-bars read-until-bar)))))
    (read>> read-char read-until-bar)))


;;; partial function application
(export '\()

(def (ascii c) (byte-at 0 c))
(def (find-params code)
     (if (is-a code ^symbol)
       (if (= (ascii "%") (byte-at 0 (symbol-name code)))
         (list code)
         nil)
       (if (and (is-a code ^improper-list) (not (nil? code)))
         (++ (find-params (head code)) (find-params (tail code)))
         nil)))

(def-read-macro \(
  (read>>= read-any (fn (xs)
  (read-return (list 'fn (find-params (syntax->datum xs)) xs)))))

