; types
(def-type bool (union-type ^'t ^'f))
(def-type int (union-type ^i8 (union-type ^i16 (union-type ^i32 ^i64))))
(def-type float (union-type ^f32 ^f64))
(def-type number (union-type ^int ^float))
(def-type (error t) (union-type (cons-type ^'ok (cons-type t ^nil)) (cons-type ^'error (cons-type ^string (cons-type ^any ^nil)))))

; function composition
(def (compose f g) (fn (x) (f (g x))))
(def (curry f . curried) (fn args (apply f (++ curried args))))
(def (flip f) (fn (x y) (f y x)))
(def (negate f) (fn args (not (apply f args))))
(def-macro (pipe x . funcs) (foldl list x funcs))

; list fundamentals
(def nil '())
(def (nil? xs) (= xs nil))
(def (list . xs) xs)
(def (cons head tail) (list head . tail))
(def (head (h . t)) h)
(def (tail (h . t)) t)

; boolean
(def-macro (or a b) (list 'if a ''t b))
(def-macro (and a b) (list 'if a b ''f))
(def (or a b) (or a b))
(def (and a b) (and a b))
(def (not a) (if a 'f 't))
(def != (negate =))

(def-macro (cond . cases) (if (nil? cases) ''nil (list 'if (head (head cases)) (elem 1 (head cases)) (cons 'cond (tail cases)))))

; list operations
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

(def (sum xs) (foldl + 0 xs))

(def (range start end)
  (let ((acc '()))
    (loop (start end acc)
      (if (= start end)
        (cons start acc)
        (continue start (- end 1) (cons end acc))))))

(def iota (curry range 1))

(def (zip-with f  xs ys)
  (if (or (nil? xs) (nil? ys)) '()
    (cons (f (head xs) (head ys)) (zip-with f (tail xs) (tail ys)))))

(def (zip xs ys) (zip-with list xs ys))

(def (flatten xs) (foldr ++ nil xs))

(def (flatten' xs)
  (let ((rec (fn (xs rec) (if (nil? xs) rec (rec (tail xs) (++ rec (head xs)))))))
    (rec xs '())))

; option type
(def-type (option t) (union-type (cons-type ^'some (cons-type t ^nil)) ^'none))
(def (some x) (list 'some x))
(def none 'none)
(def (oget ('some x)) x)
(def (defined? opt) (not (is-a opt ^'none)))
(def (omap f opt) (if (defined? opt) (some (f (oget opt))) none))

; read monad
(def read-char 'read-char)
(def read-string 'read-string)
(def read-symbol 'read-symbol)
(def read-int 'read-int)
(def read-any 'read-any)
(def read-ignore 'read-ignore)
(def (read-return v) (list 'read-return v))
(def (read>>= r f) (list 'read-bind r f))
(def (read>> r1 r2) (list 'read-bind r1 (fn (v) r2)))

; multiline comment

(def-read-macro |
  (let ((read-bars (read>>= read-char (fn (c) (if (= c (ascii "#")) read-ignore (if (= c (ascii "|")) read-bars read-until-bar)))))
        (read-until-bar (read>>= read-char (fn (c) (if (= c (ascii "|")) read-bars read-until-bar)))))
    (read>> read-char read-until-bar)))


; partial function application
(def (ascii c) (byte-at 0 c))
(def (find-params code)
     (if (is-a code ^any-symbol)
       (if (= (ascii "%") (byte-at 0 (symbol-name code)))
         (list code)
         nil)
       (if (is-a code ^(cons any any))
         (++ (find-params (head code)) (find-params (tail code)))
         nil)))

(def-read-macro \(
  (read>>= read-any (fn (xs)
  (read-return (list 'fn (find-params (syntax->datum xs)) xs)))))

