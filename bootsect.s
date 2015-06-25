!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
!
SYSSIZE = 0x3000

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4				! nr of setup-sectors
BOOTSEG  = 0x07c0			! original address of boot-sector
INITSEG  = 0x9000			! we move boot here - out of the way
SETUPSEG = 0x9020			! setup starts here
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).
ENDSEG   = SYSSEG + SYSSIZE		! where to stop loading

! ROOT_DEV:	0x000 - same type of floppy as boot.
!		0x301 - first partition on first drive etc
ROOT_DEV = 0x306

entry _start
_start:
	mov	ax,#BOOTSEG
	mov	ds,ax
	mov	ax,#INITSEG
	mov	es,ax
	mov	cx,#256
	sub	si,si
	sub	di,di
	rep
	movw
	jmpi	go,INITSEG
go:	mov	ax,cs
	mov	ds,ax
	mov	es,ax
! put stack at 0x9ff00.
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >>512

!get cursor position
!参数：ah=0x03,bh=页号
	mov	ah,#0x03
	xor	bh,bh
	int	0x10
!show msg
!参数：cx=字符数量,bh=页号,bl=字符显示属性7普通,es:bp=字符串地址,ah=0x13,al=0x01
	mov	ax,#0x1301
	mov	cx,#32
	mov	bp,#msg1
	mov	bx,#0x0007
	int	0x10

!load setup.s
!读取磁盘两个扇区到SETUPSEG(0x9020)
!参数： ah=0x02为读扇区,al=要读取的扇区数量,ch=磁道号的低8位,cl=0-5位是开始扇区，6-7位是磁道号高2位
!	dh=磁头号,dl=驱动器号,0表示软区，es:bx=数据存放地址。
!	mov	ax,#SETUPSEG
!	mov	es,ax
load_setup:
	mov	ax,#0x0202		!此处原程序读取4个即＃0x200+SETUPLEN
	mov	bx,#0x0200		!此处表示读入到0x9000段的512字节后，即本程序的后面
	mov	cx,#0x0002
	mov	dx,#0x0000
	int	0x13
!读取完毕，跳转执行
	jnc	ok_loadup
!如果出错，则磁盘驱动器复位，重新读取
!参数：ah=0x00为复位驱动器，dl=要复位的驱动器号，软盘为0
	mov	ax,#0x0000
	mov	dx,#0x0000
	int	0x13
	jmp	load_setup

!读取后跳到此处执行
ok_loadup:
	jmpi	0,SETUPSEG		!此处跳到setup.s处执行，
	
stop:
	jmp	stop

sectors:
	.word 0

msg1:
	.byte 13,10
	.ascii "LiOs system is Loading ..."
	.byte 13,10,13,10

.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
