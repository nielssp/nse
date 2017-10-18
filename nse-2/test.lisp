(def nil '())
(def list (fn xs xs))
(def cons (fn (head tail) (list head . tail)))
(def head (fn ((head . tail)) head))
(def tail (fn ((head . tail)) tail))

(def nil? (fn (xs) (= xs nil)))
(def map (fn (f xs) (if (nil? xs) nil (cons (f (head xs)) (map f (tail xs))))))
