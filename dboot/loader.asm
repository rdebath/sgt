; Second stage loader for dboot.

	  org 0

; Set up segment registers.
	  mov ax,cs
	  mov ds,ax
	  mov es,ax

; Print a sign-on message
	  mov si,signon
	  call msg

; If we don't have "nodef" set, then...
	  cmp byte [nodef],0
	  jne do_prompt

; Check Capslock & Shift keys, to see if we need prompt
	  mov ah,2
	  int 0x16
	  mov bx,config		; first boot option, by default
	  test al,01011111b	; Capslock, Scrollock, Alt, Ctrl, Shifts
	  jz do_boot		; nothing pressed? go ahead & boot...

; Print the prompt and wait for a key.
do_prompt:
	  mov si,[msgptr]
	  call msg
.getkey	  mov ah,0
	  int 0x16
	  mov ah,al		; save it - LODSB kills AL
	  mov bx,config
.scanc	  mov si,[bx]		; key table
	  cmp si,0
	  je .getkey		; end of config block
.findk	  lodsb
	  cmp al,0
	  je .nextc
	  cmp al,ah
	  je got_option
	  jmp .findk
.nextc	  add bx,8		; size of a config entry
	  jmp .scanc

; OK, we've now got the address of "our" config block in BX.
got_option:
	  mov si,[bx]		; the key table
	  lodsb			; AL = first key value
	  push bx		; don't trash BX, it's valuable
	  mov ah,0x0E
	  mov bx,7
	  int 0x10		; display it
	  mov ax,0x0A0D		; CRLF
	  call printtwo
	  pop bx
do_boot	  push bx
	  mov si,booting_msg	; print "Booting "
	  call msg
	  pop bx
	  push bx
	  mov si,[bx+6]		; option name
	  call msg
	  mov ax,0x0A0D
	  call printtwo		; CRLF
	  pop bx
	  mov ax,[bx+2]		; boot type
	  mov si,[bx+4]		; point to the param block
	  cmp ax,1		; kernel-type?
	  je t_kern
	  cmp ax,2		; bootsector-type?
	  je b_bsect
	  cmp ax,3		; bootfile-type?
	  je t_bfile
	  mov ax,'**'
	  jmp panic		; config block corrupt

; Trampoline to b_kern, since it's out of branch range
t_kern	  jmp b_kern

; Trampoline to b_bfile, since it's out of branch range
t_bfile	  jmp b_bfile

; Bootsector boot option: possibly read an MBR into 0:0x600,
; possibly load SI with some value pointing to the partition
; table at 0x7BE, possibly cram some stuff into the BIOS
; keyboard buffer, then load and jump to a boot sector.

b_bsect	  mov cx,[si]
	  cmp cx,0xFFFF
	  je .noptab		; no partition table - don't bother
	  mov dx,[si+2]
	  push si
	  mov ax,0x201
	  xor bx,bx
	  mov es,bx
	  mov bx,0x600
	  int 0x13
	  jnc .ptok
	  jmp read_error
.ptok	  pop si
.noptab	  mov cx,[si+10]	; scancode pointer
	  cmp cx,0
	  je .nokeys
	  call keys
.nokeys	  mov word [retries],5	; if it's a floppy, keep trying
.trybs	  mov cx,[si+6]
	  mov dx,[si+8]
	  push si
	  mov ax,0x201
	  xor bx,bx
	  mov es,bx
	  mov bx,0x7C00		; load boot sector back where it should be
	  int 0x13
	  jnc .gotbs
	  pop si
	  mov dx,[si+8]
	  mov ah,0
	  int 0x13		; reset disk - it may be a floppy
	  dec word [retries]
	  jne .trybs		; and try again
	  jmp read_error	; now we give up

.gotbs	  pop si
	  mov ax,[si+4]		; SI on boot
	  mov si,ax
	  jmp 0:0x7C00		; go!

; Routine to insert keys into keyboard buffer.

keys	  push si
	  push es
	  mov ax,0x40
	  mov es,ax
	  mov si,cx
.key	  lodsw
	  cmp ax,0
	  je .kdone
	  mov bx,[es:0x1c]	; first free slot in kbuf
	  mov [es:bx],ax
	  add bx,2
	  cmp bx,[es:0x82]	; kbuf end
	  jne .nowrap
	  mov bx,[es:0x80]
.nowrap	  mov [es:0x1c],bx
	  jmp .key
.kdone	  pop es
	  pop si
	  ret

; Bootfile type boot option. Load the first sector of a file at
; 0x07C0:0 and chain to it.

b_bfile	  mov ax,[si]		; file name
	  mov [name],ax
	  call initdos		; won't return if error
	  mov ax,0x07c0		; real boot sector position
	  call getsect
	  mov cx,[si+2]		; scancode pointer
	  cmp cx,0
	  je .nokeys
	  call keys
.nokeys	  jmp 0:0x7C00		; go!

; Kernel type boot option: load a kernel file off the boot
; partition, set up the command line descriptor and boot it.

b_kern	  mov ax,0x8800		; stack grows down from 9800:0, just below us
	  mov ss,ax
	  mov sp,0xfffe

	  mov ax,[si]		; file name
	  mov [name],ax
	  mov ax,[si+2]		; command line
	  mov [cmdline],ax

; Plan:
;
; We're at 0x9800. Our stack is just below us.
; We must load the first sector of the kernel at 0x9000:0.
; Then we load the setup code (the next few sectors) at
; 0x9020:0 and upwards (just after the first sector).
; The number of setup sectors is stored at 0x9000:497.
; (If it's zero, assume four.)
; Next we load the *rest* of the kernel at 0x1000:0 and upwards.
; Then we set up a command line descriptor:
; - 0xA33F at 0x9000:0x20
; - the offset (segment must be 0x9000) of the command line
; at 0x9000:0x22. Command line is null-terminated.
; Finally we can jump to 0x9020:0 to pass control to the kernel.

	  call initdos		; won't return if error
	  mov ax,0x9000		; load the first sector
	  call getsect
	  mov ax,0x9000
	  mov es,ax
	  mov cl,[es:497]	; # of setup sectors
	  xor ch,ch
	  mov ax,0x9020		; starting segment addr
.ssect	  push ax
	  push cx
	  call getsect
	  pop cx
	  pop ax
	  add ax,0x20		; next position...
	  loop .ssect

; That's loaded the setup sectors in. Now we must load the actual
; kernel image at 0x1000:0.

	  mov cx,[total]	; # of sectors remaining
	  mov ax,0x1000
.ksect	  push ax
	  push cx
	  call getsect
	  pop cx
	  pop ax
	  add ax,0x20		; next position...
	  loop .ksect

; Now set up the command line descriptor
	  mov ax,0x9000
	  mov es,ax
	  mov word [es:0x20],0xA33F
	  mov ax,[cmdline]
	  add ax,0x8000		; segment 0x9000 not 0x9800
	  mov word [es:0x22],ax

; And jump to the kernel!
	  jmp 0x9020:0

; Plan for DOS filesystem access:

; We store the cluster (or part thereof) we're currently using
; at offset 0x4000 (absolute address 0x9C000) and the FAT sector
; we currently have in memory at 0x3E00 (0x9BE00). We store
; pointers to the FAT sector and our current position in the
; cluster. Also we store the number of sectors remaining in the
; current cluster. Oh - we copy the DOS boot sector to 0x3C00
; (0x9BC00).

; Initialise DOS file system access.

initdos:

; Start by loading the bootsector of the DOS partition.
	  mov ax,[poffset]
	  mov dx,[poffset+2]
	  mov cx,1
	  mov bx,0x3C00
	  call read

; Determine how much of a cluster we're going to read at once.
	  mov al,[0x3C00+0x0D]	; cluster size
	  xor ah,ah
	  cmp ax,32		; we can't read more than 32 sectors at once
	  jbe .noadjust
	  mov ax,32
.noadjust mov [readsize],ax

; Now calculate the offset of the root directory and FAT.
	  mov ax,[0x3C00+0x0E]	; reserved sectors
	  mov dx,[poffset+2]
	  add ax,[poffset]
	  adc dx,0
	  mov [foffset],ax
	  mov [foffset+2],dx
	  mov al,[0x3C00+0x10]	; number of FATs
	  xor ah,ah
	  mov bx,[0x3C00+0x16]	; sectors per FAT
	  mul bx
	  add ax,[foffset]
	  adc dx,[foffset+2]
	  mov [poffset],ax	; now poffset holds root dir
	  mov [poffset+2],dx	; offset instead...

; Repeatedly load a sector from the root dir, and scan it.
	  mov cx,[0x3C00+0x11]	; number of root dir entries
	  shr cx,1		; divide by 16: 16 entries/sector
	  shr cx,1
	  shr cx,1
	  shr cx,1
	  mov ax,0		; sector offset into rootdir
rd_sec	  push cx		; save the loop count
	  push ax		; and the sector offset
	  mov dx,[poffset+2]
	  add ax,[poffset]
	  adc dx,0
	  mov bx,0x3E00		; no FAT around yet - we can use this
	  mov cx,1
	  call read		; read the root dir sector
	  mov si,0x3E00
	  mov cx,16		; number of entries to try
	  cld
.scan	  push cx
	  mov cx,8+3		; filename length
	  mov di,[name]		; the filename
	  repe cmpsb
	  je gotcha
	  add si,cx		; advance past this name
	  add si,32-(8+3)	; advance to next name
	  pop cx
	  loop .scan
	  pop ax
	  inc ax
	  pop cx
	  loop rd_sec

; We haven't found the file!
	  mov ax,'?!'
	  jmp panic

; We have found the file.
gotcha	  pop ax
	  pop ax
	  pop ax

; update [poffset] to hold the offset of the first cluster, not the
; start of the root dir.
	  mov ax,[0x3C00+0x11]	; number of root dir entries
	  shr ax,1		; divide by 16: 16 entries/sector
	  shr ax,1
	  shr ax,1
	  shr ax,1
	  add [poffset],ax
	  pushf
	  xor ax,ax
	  popf
	  adc [poffset+2],ax

	  add si,15		; point to cluster number
	  lodsw
	  mov [cluster],ax
	  mov ax,[si]
	  mov dx,[si+2]
	  add ax,511		; round up to next sector count
	  adc dx,0
	  mov al,ah
	  mov ah,dl
	  shr ax,1		; number of sectors
	  mov [total],ax
	  mov word [csect],0	; no sectors ready
	  mov byte [fatsect],0xFF ; we've trashed our FAT space
	  ret

; Load a sector.
; Plan: if [csect] is non-zero, return a sector from the
; current cluster buffer. Otherwise, fill up the cluster
; buffer from the current cluster (cluster number is
; [cluster], position within it is [clustpos]) and, if we
; have reached the end of the cluster, refer to the FAT
; to point [cluster] at the next cluster.
;
; On entry: AX contains the *segment* address at which to
; store the sector.

getsect	  cmp word [csect],0	; cluster buffer empty?
	  je .notgot
	  jmp .gotone
.notgot	  push ax
	  mov ax,[cluster]
	  sub ax,2		; fudge factor - this is real!
	  mov bl,[0x3C00+0x0D]	; cluster size
	  xor bh,bh
	  mul bx
	  add ax,[poffset]
	  adc dx,[poffset+2]
	  add ax,[clustpos]	; position within this cluster
	  adc dx,0
	  mov bx,cs
	  mov es,bx
	  mov bx,0x4000		; cluster buffer
	  mov cx,[readsize]	; read as much as we can
	  call read
	  mov cx,[readsize]
	  mov [csect],cx	; cluster buffer now full
	  add [clustpos],cx	; and advance cluster position
	  mov al,[0x3C00+0x0D]
	  xor ah,ah
	  cmp [clustpos],ax	; have we reached end of cluster?
	  jb .pregot		; quit out now, if so
	  mov ax,[cluster]
	  cmp [fatsect],ah	; do we have the right FAT sector?
	  je .gotfat
	  mov al,ah
	  xor ah,ah
	  mov dx,[foffset+2]
	  add ax,[foffset]
	  adc dx,0
	  mov bx,0x3E00		; FAT buffer
	  mov cx,1		; one sector
	  call read
	  mov ax,[cluster]
	  mov [fatsect],ah
.gotfat	  mov bl,al
	  xor bh,bh
	  shl bx,1
	  mov ax,[bx+0x3E00]	; next cluster
	  mov [cluster],ax
	  mov word [clustpos],0
.pregot	  mov word [cptr],0x4000
	  pop ax
.gotone	  mov es,ax
	  mov si,[cptr]
	  xor di,di
	  cld
	  mov cx,256		; sector = 256 *words*
	  rep movsw
	  dec word [csect]
	  dec word [total]
	  mov [cptr],si
	  ret

; This routine reads from a physical BIOS disk.
; On entry: DX:AX hold the absolute sector number,
; CX holds the number of sectors to read, and
; ES:BX point to the place to put it.

read:	  push dx
	  push ax
	  div word [dg_sec]	; see whether we have
	  mov di,[dg_sec]	; to split the read to
	  sub di,dx		; avoid crossing a track
	  pop ax		; boundary
	  pop dx
	  cmp di,cx		; no, we're ok
	  jge .realread		; so go ahead
	  push ax
	  push bx
	  push cx
	  push dx
	  push di
	  mov cx,di		; only read the first DI
	  call .realread	; sectors
	  pop di
	  pop dx
	  pop cx
	  pop bx	
	  pop ax
	  sub cx,di		; update all registers
	  add ax,di
	  adc dx,0
	  xchg bx,di
	  mov bh,bl
	  xor bl,bl
	  shl bx,1		; multiply by 512
	  add bx,di
	  jmp read		; back to Square One

.realread:
	  push cx		; keep the sector count for later
	  div word [dg_sec]	; DX holds sector #, AX cyl+head
	  mov cl,dl
	  inc cl		; sectors are numbered from 1 ?!
	  xor dx,dx
	  div word [dg_head]	; DX holds head #, AX cyl #
	  mov ch,al
	  shl ah,1
	  shl ah,1
	  shl ah,1
	  shl ah,1
	  shl ah,1
	  shl ah,1
	  and cl,00111111b
	  and ah,11000000b
	  or cl,ah
	  mov dh,dl
	  mov dl,[bios]		; now CX/DX are set up
	  pop ax		; now AL is sector count
	  mov ah,2		; function number: read disk
	  int 0x13
	  jc read_error
	  ret

; We've had a read error. Convert the status in AH into two hex
; digits, print them, and die.

read_error:
	  call print_hex
	  jmp panic

; Format AH -> AX as two hex digits. (AL high, AH low)

print_hex:
	  mov al,ah
	  shr al,4
	  and ax,0x0F0F
	  add ax,'00'
	  cmp al,'9'
	  jbe .temp1
	  add al,7
.temp1	  cmp ah,'9'
	  jbe .temp2
	  add ah,7
.temp2	  ret

; Print two characters: AL then AH.

printtwo:
	  push ax
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
	  pop ax
	  mov al,ah
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
	  ret

; Print a '/', then two chars in AX, then die.
panic:	  push ax
	  mov ax,0xE00+'/'
	  mov bx,7
	  int 0x10
	  pop ax
	  call printtwo
stop	  jmp stop

; Print a null-terminated message in DS:SI.

msg	  lodsb
	  cmp al,0
	  je .done
	  push si
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
	  pop si
	  jmp msg
.done	  ret

csect	  dw 0
cluster	  dw 0
clustpos  dw 0
readsize  dw 0
total	  dw 0
cptr	  dw 0
foffset	  dd 0
fatsect	  db 0
name	  dw 0
cmdline	  dw 0
retries	  dw 0

signon	  db ' v0.1 by Simon Tatham',13,10,0

booting_msg:
	  db 'Booting ',0

; End of code.
; Next thing must be:
; o  Doubleword giving boot partition offset from start of disk, in
;    sectors.
; o  Word giving # sectors/track on boot device.
; o  Word giving # heads on boot device.
; o  Byte giving BIOS device no. of boot partition.
; o  Byte containing 1 if no default boot option is to be used.
; o  One word pointing to the prompt string (ASCIZ, offset from BOF).
; o  Config entries: each one consists of:
;    -  word pointing to the key list (zero-terminated, first key in
;       list is what gets printed).
;    -  word denoting type of boot option (1=kernel, 2=bootsector,
;       3=bootfile).
;    -  word pointing to parameter block (boot option specific).
;    -  word pointing to title string (ASCIZ).
; o  Zero word to terminate config entries.
; o  All the data referred to by any pointers above.
;
; Parameter block for kernel option:
; o  Word pointing to kernel file name (in root dir of boot partn).
; o  Word pointing to kernel command line.
;
; Parameter block for boot sector option:
; o  Two words containing CX and DX for loading the MBR.
; o  Word containing value for SI on passing control to boot sector.
; o  Two words containing CX and DX for loading the boot sector.
; o  Word pointing to scan codes to stick in keyboard buffer. May
;    be NULL; scancodes are a list of words, terminated by zero word.
;    Note: CX=FFFF for the MBR means don't bother loading an MBR.
;
; Parameter block for boot file option:
; o  Word pointing to bootsector file name (in root dir of boot partn)
; o  Word pointing to scan codes to stick in keyboard buffer.

poffset	  dd '____'		; marker so install prog can strip the rest
dg_sec	  dw 0
dg_head	  dw 0
bios	  db 0			; BIOS drive number
nodef	  db 0			; no default option, if TRUE

msgptr	  dw 0

config:
