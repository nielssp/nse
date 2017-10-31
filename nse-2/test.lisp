
(def-type bool (union-type &'t &'f))
(def-type int (union-type &i8 (union-type &i16 (union-type &i32 &i64))))
(def-type float (union-type &f32 &f64))
(def-type number (union-type &int &float))
(def-type (error t) (union-type (cons-type &'ok (cons-type t &nil)) (cons-type &'error (cons-type &string (cons-type &any &nil)))))

(def nil '())
(def (list . xs) xs)
(def (cons head tail) (list head . tail))
(def (head (h . t)) h)
(def (tail (h . t)) t)

(def-macro (or a b) (list 'if a ''t b))
(def-macro (and a b) (list 'if a b ''f))

(def (nil? xs) (= xs nil))
(def (map f xs) (if (nil? xs) nil (cons (f (head xs)) (map f (tail xs)))))

(def (++ xs ys)
     (if (nil? xs) ys
       (if (nil? ys) xs
         (cons (head xs) (++ (tail xs) ys)))))

(def (filter f xs)
     (if (nil? xs) '()
       (if (f (head xs)) (cons (head xs) (filter f (tail xs))) (filter f (tail xs)))))

(def (foldl start f xs)
  (if (nil? xs) start (foldl f (f (head xs)) (tail xs))))

(def (foldr start f xs)
  (if (nil? xs) start (f (head xs) (foldr start f (tail xs)))))

(def (sum xs) (foldl 0 + xs))

(def (range start end)
  (if (= start end) (cons end nil) (cons start (range (+ start 1) end))))

(def (zip-with f  xs ys)
  (if (or (nil? xs) (nil? ys)) '()
    (cons (f (head xs) (head ys)) (zip-with f (tail xs) (tail ys)))))

(def (zip xs ys) (zip-with (fn (x y) (list x y)) xs ys))

