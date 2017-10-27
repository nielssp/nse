(def nil '())
(def (list . xs) xs)
(def (cons head tail) (list head . tail))
(def (head (h . t)) h)
(def (tail (h . t)) t)

(def (nil? xs) (= xs nil))
(def (map f xs) (if (nil? xs) nil (cons (f (head xs)) (map f (tail xs)))))
