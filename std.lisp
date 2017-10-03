(module
  (define nil (c-var Nil)) 
  (define cons (h t) (c-apply Cons h t))

  (define nil? (x) (= x nil))

  (define h (xs) (c-apply add_ref (c-apply head xs)))
  (define t (xs) (c-apply add_ref (c-apply tail xs)))

  (define length (xs)
    (if (nil? xs) 0 (+ 1 (length (t xs)))))

  (define ! (b) (if b 'f 't))
  (define = (a b) (c-apply nse_equals a b))
  (define != (a b) (! (= a b)))
  (define + (a b) (c-apply Int (c-op (c-int a) + (c-int b))))
  (define - (a b) (c-apply Int (c-op (c-int a) - (c-int b))))
  (define * (a b) (c-apply Int (c-op (c-int a) * (c-int b))))
  (define / (a b) (c-apply Int (c-op (c-int a) / (c-int b))))
  (define % (a b) (c-apply Int (c-op (c-int a) % (c-int b))))
  (define < (a b) (!= 0 (c-apply Int (c-op (c-int a) < (c-int b)))))
  (define > (a b) (!= 0 (c-apply Int (c-op (c-int a) > (c-int b)))))
  (define <= (a b) (!= 0 (c-apply Int (c-op (c-int a) <= (c-int b)))))
  (define >= (a b) (!= 0 (c-apply Int (c-op (c-int a) >= (c-int b)))))

  (define and (a b) (if a b 'f))
  (define or (a b) (if a 't b))

  (define print (obj) (c-apply nse_print obj))

  (define apply (f args) (c-apply nse_apply f args))

  (define curry (f . args) (lambda args' (apply f (append args args'))))

  (define flip (f) (lambda (x y) (f y x)))

  (define append (xs ys)
    (if (nil? xs) ys
      (if (nil? ys) xs
        (cons (h xs) (append (t xs) ys))))))
