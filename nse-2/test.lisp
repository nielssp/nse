(def list (fn xs xs))
(def cons (fn (head tail) (list head . tail)))
(def head (fn ((head tail)) head))
(def head (fn ((head tail)) tail))
