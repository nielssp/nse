(module
  (import std)

  (define æøå 'unicode-lol)

  (define map (f xs)
    (if (nil? xs) '() (cons (f (h xs)) (map f (t xs)))))

  (define filter (f xs)
    (if (nil? xs) '()
      (if (f (h xs)) (cons (h xs) (filter f (t xs))) (filter f (t xs)))))

  (define foldl (start f xs)
    (if (nil? xs) start (foldl f (f (h xs)) (t xs))))

  (define foldr (start f xs)
    (if (nil? xs) start (f (h xs) (foldr start f (t xs)))))

  (define sum (xs) (foldl 0 + xs))

  (define range (start end)
    (if (> start end) '() (cons start (range (+ start 1) end))))

  (define zipWith (f  xs ys)
    (if (or (nil? xs) (nil? ys)) '()
      (cons (f (h xs) (h ys)) (zipWith f (t xs) (t ys)))))

  (define zip (xs ys) (zipWith (lambda (x y) (list x y)) xs ys))

  (define even? (x) (= 0 (% x 2)))

  (define f (x) (t x))
  (define g (x) (cons (f x) (f x)))

  (define main () (print (map (lambda (x) (+ x 2)) (cons 1 (cons 2 (cons 3 nil))))))

  (private
    (define my-list 'foo)
    (define secret 42)))

