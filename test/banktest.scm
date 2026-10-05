;;; banktest: two banks of code for the 16K slot at $4000-$7FFF (see
;;; src/bank.s for why the slot is there), and the program below it.
;;;
;;; Each bank is a memory at the slot's address, holding the sections a
;;; source file names with #pragma clang section, and scattered to its own
;;; 16K block of far memory. The blocks are what bankStore.raw holds, in
;;; address order, and what is loaded to $40000 before any banked call.

(define memories
  '((memory program
            (address (#x2001 . #x3fff)) (type any)
            (section (programStart #x2001) (startup #x200e)))
    (memory zeroPage (address (#x2 . #x7f)) (type ram) (qualifier zpage)
            (section (registers #x2)))
    (memory stackPage (address (#x100 . #x1ff)) (type ram))
    (memory freeSpace (address (#x1600 . #x1eff)) (section zpsave))
    (memory bankA (address (#x4000 . #x7fff))
            (scatter-to bankAStore)
            (section bank_a bank_a_data))
    (memory bankB (address (#x4000 . #x7fff))
            (scatter-to bankBStore)
            (section bank_b bank_b_data))
    (memory bankStore (address (#x40000 . #x5ffff))
            (section (bankAStore #x40000) (bankBStore #x44000)))
    ))
