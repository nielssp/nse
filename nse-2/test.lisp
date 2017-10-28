
(def-type bool (union-type &'t &'f))
(def-type int (union-type &i8 (union-type &i16 (union-type &i32 &i64))))
(def-type float (union-type &f32 &f64))
(def-type number (union-type &int &float))

(def nil '())
(def (list . xs) xs)
(def (cons head tail) (list head . tail))
(def (head (h . t)) h)
(def (tail (h . t)) t)

(def (nil? xs) (= xs nil))
(def (map f xs) (if (nil? xs) nil (cons (f (head xs)) (map f (tail xs)))))
