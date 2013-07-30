;; -*- lisp -*-
;;
;; Simon Tatham's standardised Sawfish setup file.
;;
;; This file is best included in your Sawfish configuration by means
;; of adding a line like this to ~/.sawfish/rc:
;;
;;   (load (concat (getenv "HOME") "/src/local/sawfish.jl") nil t t)

;; ----------------------------------------------------------------------
;; Function to re-run the default placement mode.

(define (re-place-window w)
  ((placement-mode (if (window-transient-p w)
                       place-transient-mode place-window-mode)) w))
(define-command 're-place-window re-place-window #:spec "%W")

;; ----------------------------------------------------------------------
;; Mouse actions on window title/border

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
    "Button1-C-Click" 're-place-window
    "Button3-C-Click" 'popup-window-menu
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
    "Button3-C-Click" 'popup-window-menu
)
(unbind-keys border-keymap
    "Button1-Off" "Button2-Off" "Button3-Off"
    "Button2-Click"
)

;; ----------------------------------------------------------------------
;; Left-Windows + x to minimise windows

(define (window-under-pointer)
    ;; A tweaked version of query-pointer-window which returns the
    ;; window under the pointer if possible, or if there is no such
    ;; window falls back to the one with the input focus. (It's just
    ;; conceivable that there might not be one of those either, so
    ;; callers of this function should still be prepared for it to
    ;; return nil.)
    (let ((w1 (query-pointer-window))
	  (w2 (input-focus)))
	(cond
            ((not (null w1)) w1)
            (t w2)
	)
    )
)

(define (iconify-window-under-pointer)
    (let ((w (window-under-pointer)))
	(cond ((not (null w)) (iconify-window w)))
    )
)

(define (lower-window-under-pointer)
    (let ((w (window-under-pointer)))
	(cond ((not (null w)) (lower-window w)))
    )
)

(bind-keys global-keymap
    "H-x" 'iconify-window-under-pointer
    "H-z" 'lower-window-under-pointer
)

;; ----------------------------------------------------------------------
;; Some variable settings

;; In _general_, I approve of programs stating their own window
;; position. I'll override that for specific window types that
;; misbehave.
(setq ignore-program-positions nil)

;; Some versions of Sawfish ship with this set to 0 by default,
;; which looks ugly.
(setq sp-padding 6)

;; ----------------------------------------------------------------------
;; On creation of new windows, fiddle with their properties

;; Some applications have a nasty habit of trying to place all
;; their windows at (0,0). Identify those apps' windows, ignore
;; their specified positions, and let Sawfish DTRT.
(add-hook 'add-window-hook
          (lambda (w)
            (let ((class (window-class w)))
              (when (or (equal class "Mozilla-bin")
                        (equal class "Gnotravex")
                        (equal class "Xpdf"))
                (window-put w 'ignore-program-position t)))))

;; Sawfish 1.5.3 on Ubuntu 12.04 fails to ignore Xfce notifications
;; for purposes of first-fit placement of other windows.
(add-hook 'add-window-hook
          (lambda (w)
            (let ((class (window-class w)))
              (when (equal class "Xfce4-notifyd")
                (make-window-ignored w t)))))

;; Sawfish 1.5.3 on Ubuntu 13.04 fails to spot that windows popped up
;; from the XFCE panel should be frameless.
(add-hook 'add-window-hook
          (lambda (w)
            (let ((class (window-class w)))
              (when (equal class "Xfce4-panel")
                (window-put w 'type 'unframed)))))

;; ----------------------------------------------------------------------
;; My personalised focus handling

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
    (let ((class (window-class w)))
	(or (equal class "Putty")
	    (equal class "Pterm")
	)
    )
)

;; This function contains knowledge about window types which I
;; don't want to focus just because of mouse movements.
(define (sgt-no-focus-on-mouse-p w)
    (let ((class (window-class w)))
	(or (equal class "Panel")
	    (equal class "Gnome-panel")
	    (equal class "Xfce4-panel")
	)
    )
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
		    (focus-push-map w click-to-focus-map)))
	)
    )
)

;; Set it as the default, and disable focus-windows-when-mapped which
;; would negate the entire point of doing all this.
(setq focus-mode 'sgt-enter-only)
(setq focus-windows-when-mapped nil)

;; ----------------------------------------------------------------------
;; Key binding for merge-with-next-workspace
(bind-keys global-keymap
    "H-M-F1" 'merge-next-workspace
    "M-C-Right" 'merge-next-workspace
    "M-C-Left" 'merge-previous-workspace
)

;; ----------------------------------------------------------------------
;; Add a window menu item to paste a window's title. Depends on my
;; utility 'xcopy', because Sawfish appears to have no built-in
;; facility to write to the X selection (though it can read it).
(define (sgt-copy-window-title w)
  (system (concat "echo -n '"
                  (string-replace "'" "'\\''" (window-name w))
                  "' | xcopy")))
(define-command 'sgt-copy-window-title sgt-copy-window-title #:spec "%W")
(setq window-ops-menu
      (append window-ops-menu
              '(() ("Copy window title" sgt-copy-window-title))))

;; ----------------------------------------------------------------------
;; Some other miscellaneous configuration options I feel strongly
;; about

(setq place-window-mode 'first-fit)
(setq default-frame-style 'Crux)
(setq Crux:normal-color (get-color "#00000000c000"))
(setq CruxSideways:normal-color Crux:normal-color)
(setq CruxAbsent:normal-color Crux:normal-color)
(setq EdgelessSafetyCrux:normal-color Crux:normal-color)
