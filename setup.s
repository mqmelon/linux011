!取硬盘、内存及显卡信息，保存到0x90000-0x901FF处，因为此处为bootsect.s，
!已不再需要了。

! NOTE! These had better be the same as in bootsect.s!

INITSEG  = 0x9000	! we move boot here - out of the way
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).
SETUPSEG = 0x9020	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.

	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax

	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches

	mov	[0],dx		! it from 0x90000.

! Get memory size (extended mem, kB)

	mov	ah,#0x88
	int	0x15
	mov	[2],ax

! Get video-card data:

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

! check for EGA/VGA and some config parameters

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx
	mov	[12],cx

! Get hd0 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080
	mov	cx,#0x10
	rep
	movsb

!显示信息后停止运行。
	mov	ax,#SETUPSEG
	mov	es,ax
	
	mov	ax,#0x03
	xor	bh,bh
	int	0x10

!参数：cx=字符数量,bh=页号,bl=字符显示属性7普通,es:bp=字符串地址,ah=0x13,al=0x01
	mov	ax,#0x1301
	mov	cx,#26
	mov	bp,#msg2
	mov	bx,#0x0007
	int	0x10

	mov	ah,#0x03
	xor	bh,bh
	int	0x10
!show msg
!参数：cx=字符数量,bh=页号,bl=字符显示属性7普通,es:bp=字符串地址,ah=0x13,al=0x01
	mov	ax,#0x1301
	mov	cx,#32
	mov	bp,#msg3
	mov	bx,#0x0007
	int	0x10
	
	
	mov	ax,#INITSEG
	mov	ds,ax
	mov	ax,[0]
	push	ax
	call	print_hex

!系统停止运行
stop_here:
	jmp	stop_here


!以16进制方式打印栈顶的16位数
print_hex:
	mov    cx,#4         ! 4个十六进制数字
	mov    dx,(bp)     ! 将(bp)所指的值放入dx中，如果bp是指向栈顶的话
	print_digit:
	rol    dx,#4        ! 循环以使低4比特用上 !! 取dx的高4比特移到低4比特处。
	mov    ax,#0xe0f     ! ah = 请求的功能值，al = 半字节(4个比特)掩码。
	and    al,dl        ! 取dl的低4比特值。
	add    al,#0x30     ! 给al数字加上十六进制0x30
	cmp    al,#0x3a
	jl    outp        !是一个不大于十的数字
	    add    al,#0x07      !是a～f，要多加7
outp: 
	int    0x10
	    loop    print_digit
	    ret

!打印回车
print_nl:
	mov    ax,#0xe0d     ! CR
	int    0x10
	mov    al,#0xa     ! LF
	int    0x10
	    ret
!提示信息
msg2:
	.byte	13,10
	.ascii	"Now, we are in SETUP ."
	.byte	13,10

msg3:
	.byte	13,10
	.ascii	"Cursor position:"
.text
endtext:
.data
enddata:
.bss
endbss:
