;; -*- mode: lisp; mode: fold -*-
;;
;; Simon Tatham's standardised Sawfish setup file.
;;
;; This file is best included in your Sawfish configuration by means
;; of adding a line like this to ~/.sawfish/rc:
;;
;;   (load (concat (getenv "HOME") "/src/local/sawfish.jl") nil t t)

;; Mouse actions on window title/border {{{

;;  - On the title bar, dragging moves the window, unless you
;;    double-click first in which case it resizes.
;;  - On the window border, dragging resizes the window, unless you
;;    double-click first in which case it moves.
;;  - Using the left button for either of these, or just clicking
;;    with it once, brings the window to the top.
;;  - Using the right button for either of these, or just clicking
;;    with it once, sends the window to the bottom.
;;  - Using the middle button has no effect on the window's z-order.

(bind-keys title-keymap
  "Button1-Move" 'move-window-interactively
  "Button2-Move" 'move-window-interactively
  "Button3-Move" 'move-window-interactively
  "Button1-Click" 'raise-window
  "Button3-Click" 'lower-window
  "Button1-Click2" 'resize-window-interactively
  "Button2-Click2" 'resize-window-interactively
  "Button3-Click2" 'resize-window-interactively
)
(unbind-keys title-keymap
  "Button1-Off" "Button2-Off" "Button3-Off"
  "Button2-Click"
)

(bind-keys border-keymap
  "Button1-Move" 'resize-window-interactively
  "Button2-Move" 'resize-window-interactively
  "Button3-Move" 'resize-window-interactively
  "Button1-Click" 'raise-window
  "Button3-Click" 'lower-window
  "Button1-Click2" 'move-window-interactively
  "Button2-Click2" 'move-window-interactively
  "Button3-Click2" 'move-window-interactively
)
(unbind-keys border-keymap
  "Button1-Off" "Button2-Off" "Button3-Off"
  "Button2-Click"
)

;; }}}
;; Left-Windows + x to minimise windows {{{

(define (iconify-window-under-pointer)
   (let ((w1 (query-pointer-window))
         (w2 (input-focus)))
      (cond ((not (null w1)) (iconify-window w1))
            ((not (null w2)) (iconify-window w2))
      )
   )
)

(define-command 'iconify-window-under-pointer iconify-window-under-pointer)

(bind-keys window-keymap
  "H-x" 'iconify-window-under-pointer
)

;; }}}
;; Some variable settings {{{

;; In _general_, I approve of programs stating their own window
;; position. I'll override that for specific window types that
;; misbehave.
(setq ignore-program-positions nil)

;; Some versions of Sawfish ship with this set to 0 by default,
;; which looks ugly.
(setq sp-padding 6)

;; }}}
;; On creation of new windows, fiddle with their properties {{{

(define (sgt-add-window w)
  ;; Some applications have a nasty habit of trying to place all
  ;; their windows at (0,0). Identify those apps' windows, ignore
  ;; their specified positions, and let Sawfish DTRT.
  (let ((class (caddr (get-x-property w 'WM_CLASS))))
     (when (or
	      (equal class "Gecko\0Mozilla-bin\0")
	      (equal class "mozilla-bin\0Mozilla-bin\0")
	      (equal class "gnotravex\0Gnotravex\0")
	      (equal class "win\0Xpdf\0"))
	(window-put w 'ignore-program-position t)
     )
  )
)
(add-hook 'add-window-hook sgt-add-window)

;; }}}
;; My personalised focus handling {{{

;; Fun new focus mode. Acts like `enter-only' in _almost_ all cases,
;; except that when a new window is created _underneath_
;; the mouse pointer, it does not necessarily gain focus as a
;; result: if the currently focused window is a PuTTY or pterm, that
;; window keeps the focus and the new window can be explicitly
;; focused either by clicking or by moving the mouse out and back in.
;;
;; To do this I have to be a bit sneaky, since X doesn't naturally
;; provide a means of doing this. We first expect to receive a
;; map-notify for the window, and _then_ later we receive an
;; enter-notify because the pointer is now inside it. But at the
;; time of the map-notify we can already tell this is going to
;; happen by using query-pointer-window.
;;
;; So. I start by defining a map-notify hook which notices when a
;; window is mapped under the pointer, and (if the currently
;; focused window is one we don't want to take focus away from)
;; responds by storing that window ID in a global variable. The
;; actual focus-mode handler will ignore a single enter-notify for
;; that window ID, _if_ it's the very next enter-notify. (This
;; ensures that waving the mouse about will clear any incorrect
;; setting of this variable and avoid any trouble.)
;;
;; In addition to all this, I also avoid transferring focus to
;; certain types of window (the GNOME panel) due to mouse
;; movements only.

;; This function contains the knowledge of which window types we do
;; not want to unexpectedly take focus away from.
(define (sgt-keeps-focus-p w)
  (let ((class (caddr (get-x-property w 'WM_CLASS))))
    (or (equal class "putty\0Putty\0")
        (equal class "pterm\0Pterm\0")
  ))
)

;; This function contains knowledge about window types which I
;; don't want to focus just because of mouse movements.
(define (sgt-no-focus-on-mouse-p w)
  (let ((class (caddr (get-x-property w 'WM_CLASS))))
    (or (equal class "panel_window\0Panel\0")
        (equal class "gnome-panel\0Gnome-panel\0")
  ))
)

;; Hook called when a window is mapped, which stashes the new window
;; ID so we can remember to ignore a pointer-in event for it later.
(setq sgt-enter-notify-ignore nil)
(define (sgt-map-notify w)
  ;;(print (list 'map-notify w))
  (when (and (equal w (query-pointer-window))
         (not (null (input-focus)))
         (sgt-keeps-focus-p (input-focus)))
   ;;(print (list 'prepare-to-ignore w))
   (setq sgt-enter-notify-ignore w)
  )
)
(add-hook 'map-notify-hook sgt-map-notify)

;; The actual focus mode.
(define-focus-mode 'sgt-enter-only
    (lambda (w action)
      ;;(print (list 'enter-only w action 'ignoring sgt-enter-notify-ignore))
      (case action
        ((pointer-in)
         (when (and (window-really-wants-input-p w)
                    (not (sgt-no-focus-on-mouse-p w))
                    (not (equal sgt-enter-notify-ignore w)))
           ;;(print (list 'setting-focus-to w))
           (set-input-focus w))
           ;;(print 'removing-ignore)
           (setq sgt-enter-notify-ignore nil))
        ((focus-in)
	 (focus-pop-map w))
	((focus-out add-window)
	 (unless (or (not (window-really-wants-input-p w))
		     (eq w (input-focus)))
	   (focus-push-map w click-to-focus-map)))
	((before-mode-change)
	 (focus-pop-map w))
	((after-mode-change)
	 (unless (or (not (window-really-wants-input-p w))
		     (eq w (input-focus)))
	   (focus-push-map w click-to-focus-map))))))

;; Set it as the default.
(setq focus-mode 'sgt-enter-only)

;; }}}
