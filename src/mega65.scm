;;; Memory map for vaxum65. Starts as the stock mega65-plain.scm, kept here
;;; so that it can grow with the port.
;;;
;;; `program` is $2001-$9FFF: 32 KB for code, data, C stack and heap, which
;;; is what is safe with every ROM mapped in. The PRG carries a C65 BASIC
;;; stub (SYS 8206) at $2001.
;;;
;;; Room to grow, when the 32 KB runs out:
;;;
;;; - $A000-$BFFF becomes RAM once BASIC is banked out ($D030 bit 4) in
;;;   __low_level_init. Declare it with a name and a section and *no*
;;;   `(type ...)`, and put only BSS there: a typed memory can be given
;;;   initialised data or the C stack, which makes a second content area,
;;;   and the link fails with "multiple program areas not allowed in prg
;;;   output".
;;; - Banks 1, 4 and 5 and attic RAM ($8000000) are reached through __far
;;;   pointers. $20000-$3FFFF is the C65 ROM, and $1F800-$1FFFF is the
;;;   colour RAM alias -- never write there.
;;;
;;; $1600-$1EFF (`freeSpace`) sits below $2001 where the ROM keeps its own
;;; buffers: nothing that must survive a Kernal call can live there.

(define memories
  '((memory program
            (address (#x2001 . #x9fff)) (type any)
            (section (programStart #x2001) (startup #x200e)))
    (memory zeroPage (address (#x2 . #x7f)) (type ram) (qualifier zpage)
            (section (registers #x2)))
    (memory stackPage (address (#x100 . #x1ff)) (type ram))
    (memory freeSpace (address (#x1600 . #x1eff)) (section zpsave))
    ))
