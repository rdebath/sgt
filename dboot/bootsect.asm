; Boot sector loader.

[org 0]
zero:
	  jmp 0x7C0:(start-zero) ; get CS:IP predictable

; this data gets modified by the install program.

sigpt	  dw sig		; pointer
sig	  db 'DBOOT'		; signature to print
bios	  db 0			; BIOS drive number
dg_sec	  dw 0
dg_head	  dw 0
poffset	  dd 0			; partition offset on physical disk
name	  times 11 db 0		; the file to load cluster one of

start:				; actual code begins.

; Get some segment registers predictable.

	  mov ax,cs
	  mov ds,ax
	  mov es,ax		; ... and assume the stack is OK already :)

; Assure the user we're here.

	  call sign

; Start by loading the bootsector of the DOS partition.

	  mov ax,[poffset]
	  mov dx,[poffset+2]
	  mov cx,1
	  mov bx,512		; load it just beyond the end of this code
	  call read

	  call sign

; Now calculate the offset of the root directory.

	  mov al,[512+0x10]	; number of FATs
	  xor ah,ah
	  mov bx,[512+0x16]	; sectors per FAT
	  mul bx
	  add ax,[512+0x0E]	; reserved sectors
	  adc dx,byte 0
	  add ax,[poffset]
	  adc dx,[poffset+2]
	  mov [poffset],ax	; now poffset holds root dir
	  mov [poffset+2],dx	; offset instead...

; Load the root directory. (DX:AX already set up.)

	  mov cx,[512+0x11]	; number of root dir entries
	  shr cx,1		; divide by 16: 16 entries/sector
	  shr cx,1
	  shr cx,1
	  shr cx,1
	  xor bx,bx		; while we're here, update [poffset] to
	  add [poffset],cx	; hold the offset of the first _cluster_
	  adc [poffset+2],bx	; not just the root dir start.
	  mov bx,1024		; past the end of the bootsector
	  call read

	  call sign

; Scan the root dir for the filename.

	  mov si,1024
	  mov cx,[512+0x11]	; number of entries to try
	  cld
scan	  push cx
	  mov cx,8+3		; filename length
	  mov di,name		; the filename
	  repe cmpsb
	  je gotcha
	  add si,cx		; advance past this name
	  add si,byte 32-(8+3)	; advance to next name
	  pop cx
	  loop scan

; We didn't find it.

	  mov ax,'??'
	  jmp panic

; We did find it.

gotcha	  call sign

	  add si,byte 15	; point to cluster number
	  lodsw
	  sub ax,byte 2		; bizarre fudge factor needed by msdos fs!
	  mov bl,[512+0x0D]	; cluster size
	  xor bh,bh
	  mul bx
	  add ax,[poffset]
	  adc dx,[poffset+2]

; So load the cluster in.

	  mov bx,0x9800
	  mov es,bx
	  xor bx,bx		; load at 0x9800:0
	  mov cl,[512+0x0D]	; read one cluster
	  xor ch,ch
	  call read

	  call sign

; Transfer control to the software thus loaded.

	  jmp 0x9800:0

; This routine reads from a physical BIOS disk.
; On entry: DX:AX hold the absolute sector number,
; CX holds the number of sectors to read, and
; ES:BX point to the place to put it.

read:	  push dx
	  push ax
	  mov di,[dg_sec]
	  call divide		; see whether we have
	  mov di,[dg_sec]	; to split the read to
	  sub di,dx		; avoid crossing a track
	  pop ax		; boundary
	  pop dx
	  cmp di,cx		; no, we're ok
	  jge realread		; so go ahead
	  push ax
	  push bx
	  push cx
	  push dx
	  push di
	  mov cx,di		; only read the first DI
	  call realread		; sectors
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

realread:
	  push cx		; keep the sector count for later
	  mov di,[dg_sec]
	  call divide		; DX holds sector #, DI:AX cyl+head
	  mov cl,dl
	  inc cl		; sectors are numbered from 1 ?!
	  mov dx,di		; now DX:AX is cyl+head
	  div word [dg_head]	; DX holds head #, AX cyl #
	  mov ch,al
	  shr ax,1
	  shr ax,1
	  and cl,00111111b
	  and al,11000000b
	  or cl,al
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
	  mov al,ah
	  shr al,1
	  shr al,1
	  shr al,1
	  shr al,1
	  and ax,0x0F0F
	  add ax,'00'
	  cmp al,'9'
	  jbe .temp1
	  add al,7
.temp1	  cmp ah,'9'
	  jbe .temp2
	  add ah,7
.temp2	  jmp panic

; Print two characters, and panic.

panic:
	  push ax
	  mov ax,0xE00+'/'
	  mov bx,7
	  int 0x10
	  pop ax
	  push ax
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
	  pop ax
	  mov al,ah
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
stop	  jmp stop

; Print the next byte of the signature

sign	  push si		; we need this register saved
	  mov si,[sigpt]
	  inc word [sigpt]
	  mov al,[si]
	  mov ah,0x0E
	  mov bx,7
	  int 0x10
	  pop si
	  ret

; Perform 32/16 -> 32 r 16 division (as opposed to 32/16 -> 16 r 16
; done by the DIV instruction). Needed for large disks: dividing
; absolute sector number by sectors/track can yield >64K cyl+head count.

; Input: DX:AX dividend, DI divisor.
; Output: DX remainder, DI:AX quotient. Nothing else clobbered.
;
; Method: Divide 00:HI by DIV to get Q1 and R1. Divide R1:LO by DIV to
; get Q2 and R2. Quotient is Q1:Q2. Remainder is R2.

divide	  push bx		; we'll need a temp register
	  mov bx,ax		; in which we save LO
	  mov ax,dx		; put HI in ax
	  xor dx,dx		; put 00 in dx
	  div di		; divide 00:HI by DIV. R1 now in dx.
	  xchg ax,bx		; put Q1 in bx, LO in ax.
	  div di		; divide R1:LO by DIV. R2 now in dx.
	  mov di,bx		; put Q1 in di. (Q2 already in ax.)
	  pop bx
	  ret
