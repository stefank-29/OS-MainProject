// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"


int
strleng(char *s){
	int n;

	for(n = 0; s[n]; n++)
		;
	return n;

}

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}


#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define TTY_BUF 2000

struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
	char baft[TTY_BUF];
	uint limit;
} tty[6];

#define C(x)  ((x)-'@')  // Control-x

#define A(x) ((x) + 'Z') // Alt-x

int currtty = 0;

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			int k = strleng(tty[currtty].baft);
			tty[currtty].baft[tty[currtty].limit++ % TTY_BUF] = c;
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700; // ovde currColor za taj terminal
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\currtty'); uartputc(' '); uartputc('\currtty');
	} else
		uartputc(c);
	cgaputc(c);
}

void
changeTty(){
		crt = (ushort*)P2V(0xb8000);
		memset(crt, 0, sizeof(crt[0])*(25*80));
		//consputc('a', 0x0700);
		//cprintf("aaaaa");
		//consolewrite(ip, buf, 7);
		outb(CRTPORT, 14);
		outb(CRTPORT+1, crt);
		outb(CRTPORT, 15);
		outb(CRTPORT+1, crt);

		if (tty[currtty].limit < 2000){
			for(int i = 0; i != tty[currtty].limit; i++){
				cgaputc(tty[currtty].baft[i] & 0xff);
			}
		}
		else{
			// for(int i = ((tty[currtty].limit) % INPUT_BUF); (i % INPUT_BUF) < (tty[currtty].limit-1 % INPUT_BUF) ; i++){
			// 	cgaputc(tty[currtty].baft[i % INPUT_BUF] & 0xff);
			// }

			for(int j = 0; j < 2000; j++){
				cgaputc(tty[currtty].baft[((tty[currtty].limit % TTY_BUF) + j) % TTY_BUF] & 0xff);
			}
		}
}


void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(tty[currtty].e != tty[currtty].w &&
			      tty[currtty].buf[(tty[currtty].e-1) % INPUT_BUF] != '\n'){
				tty[currtty].e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(tty[currtty].e != tty[currtty].w){
				tty[currtty].e--;
				consputc(BACKSPACE);
			}
			break;
		case A('1'):
			currtty = 0;
		    changeTty();
			break;
		case A('2'):
			currtty = 1;
		    changeTty();
			break;
		case A('3'):
		    currtty = 2;
		     changeTty();
			break;
		case A('4'):
			currtty = 3;
		    changeTty();
			break;
		case A('5'):
		    currtty = 4;
		    changeTty();
			break;
		case A('6'):
			currtty = 5;
		    changeTty();
			break;
		default:
			if(c != 0 && tty[currtty].e-tty[currtty].r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				tty[currtty].buf[tty[currtty].e++ % INPUT_BUF] = c;
				consputc(c);
				if(c == '\n' || c == C('D') || tty[currtty].e == tty[currtty].r+INPUT_BUF){
					tty[currtty].w = tty[currtty].e;
					wakeup(&tty[currtty].r); // budi proces koji sleep-ujem u while-u
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	if (ip->minor == 1){
		currtty = 0;
	}
	if (ip->minor == 2){
		currtty = 1;
	}
	if (ip->minor == 3){
		currtty = 2;
	}
	if (ip->minor == 4){
		currtty = 3;
	}
	if (ip->minor == 5){
		currtty = 4;
	}
	if (ip->minor == 6){
		currtty = 5;
	}


	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){  // zavrsi se ako napunim dst ili naidjem na enter
		while(tty[currtty].r == tty[currtty].w){ // jednaki su ako nemam sta da citam (na enter w skoci na kraj tj. na e)
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&tty[currtty].r, &cons.lock); // blokira trenutni proces (prosledim broj koji je adresa od input.r)
		}
		c = tty[currtty].buf[tty[currtty].r++ % INPUT_BUF];
		tty[currtty].baft[tty[currtty].limit++ % TTY_BUF] = c; // cuvam u tty baferu
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	// r ne mora da bude == w na kraju f-je (ako je korisnicki bafer manji od input bafera) onda ponovo pozove read pa se citanje nastavlja od r indeksa
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;
	// TODO ako je aktivan terminal rokaj na consolu ako nije samo puni bafer
	// TODO kruzni bafer za tty bufer

	if (ip->minor == 1){
		int k = strleng(tty[0].baft);
		for (int j = 0; j < n; j++)
			tty[0].baft[tty[0].limit++ % TTY_BUF] = buf[j];

	}
	if (ip->minor == 2){
		int k = strleng(tty[1].baft);
		for (int j = 0; j < n; j++)
			tty[1].baft[tty[1].limit++ % TTY_BUF] = buf[j];
	}
	if (ip->minor == 3){
		int k = strleng(tty[2].baft);
		for (int j = 0; j < n; j++)
			tty[2].baft[tty[2].limit++ % TTY_BUF] = buf[j];
	}
	if (ip->minor == 4){
		int k = strleng(tty[3].baft);
		for (int j = 0; j < n; j++)
			tty[3].baft[tty[3].limit++ % TTY_BUF] = buf[j];
	}
	if (ip->minor == 5){
		int k = strleng(tty[4].baft);
		for (int j = 0; j < n; j++){
			tty[4].baft[tty[4].limit++ % TTY_BUF] = buf[j];
		}
	}
	if (ip->minor == 6){
		int k = strleng(tty[5].baft);
		for (int j = 0; j < n; j++)
			tty[5].baft[tty[5].limit++ % TTY_BUF] = buf[j];
	}

	if(ip->minor == currtty + 1){ // ako je aktivni terminal
		iunlock(ip);
		acquire(&cons.lock);
		for(i = 0; i < n; i++)
			consputc(buf[i] & 0xff);
		release(&cons.lock);
		ilock(ip);
	}

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

