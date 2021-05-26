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

int init = 1;

struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
	char baft[TTY_BUF];
	uint limit;
	ushort ttyColor;
	char commands[8][128];
	uint currCommand;
	uint numOfCommands;
} tty[6];

static int currtty = 0;


#define C(x)  ((x)-'@')  // Control-x

#define A(x) ((x) + 'Z') // Alt-x

static int tty6Init = 0;




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
		crt[pos++] = (c&0xff) | tty[currtty].ttyColor;

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		//memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
		memmove(crt+pos, crt+pos+80,  sizeof(crt[0])*40);
		memmove(crt+pos+40, crt+pos+80,  sizeof(crt[0])*40);


	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | tty[currtty].ttyColor; // ovde currColor za taj terminal
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
ttyName(){
	char ttyName[] = "[tty *]";
	int start = 24 * 80 + 72;
	int j = 0;
	ttyName[5] = (currtty + 1) + '0';
	for(int i = start; i < start + 7; i++){
		crt[i] = (ttyName[j++] & 0xff) | tty[currtty].ttyColor;
	}
}

void
changeTty(){
		crt = (ushort*)P2V(0xb8000);
		memset(crt, 0, sizeof(crt[0])*(25*80));
		outb(CRTPORT, 14);
		outb(CRTPORT+1, crt);
		outb(CRTPORT, 15);
		outb(CRTPORT+1, crt);

		for(int i = 0; i < 80*25; i++) { // bg
			crt[i] |= tty[currtty].ttyColor;
		}

		if (tty[currtty].limit < 2000){ // ttyBuf
			for(int i = 0; i != tty[currtty].limit; i++){
				cgaputc(tty[currtty].baft[i] & 0xff);
			}
		}
		else{
			for(int j = 0; j < 2000; j++){
				cgaputc(tty[currtty].baft[((tty[currtty].limit % TTY_BUF) + j) % TTY_BUF] & 0xff);
			}
		}
		for(int i = tty[currtty].r % INPUT_BUF; i != tty[currtty].e % INPUT_BUF; i++){ // curr inst
			cgaputc(tty[currtty].buf[i]);
		}
		ttyName();

}

void
setDefaultColors(){
	tty[0].ttyColor = 0x3700;
	tty[1].ttyColor = 0x7400;
	tty[2].ttyColor = 0x5C00;
	tty[3].ttyColor = 0xA600;
	tty[4].ttyColor = 0x2100;
	tty[5].ttyColor = 0x6A00;

	for(int i = 0; i < 6; i++){
		for(int j = 0; j < 8; j++){
			strncpy(tty[i].commands[j], 0, 0);
		}
		tty[i].currCommand = 0;
	}



	for(int i = 0; i < 80*25; i++) { // bg
		crt[i] |= tty[0].ttyColor;
	}
}

void
moveCommands(int curr){
	for(int i = 7; i >= 1; i--){
		if(i == 7){
			memset(tty[curr].commands[i], 0, strlen(tty[curr].commands[i]));
		}
		strncpy(tty[curr].commands[i], tty[curr].commands[i-1], strlen(tty[curr].commands[i-1]));
		memset(tty[curr].commands[i-1], 0, strlen(tty[curr].commands[i-1]));
	}
	if(tty[curr].numOfCommands < 8){
		tty[curr].numOfCommands++;
	}
	tty[curr].currCommand = 0;
}

void
upperCommand(){
	int c;

	if(tty[currtty].currCommand < 7 && tty[currtty].currCommand < tty[currtty].numOfCommands){
		tty[currtty].currCommand++;
	}

	while(tty[currtty].e != tty[currtty].w &&
			tty[currtty].buf[(tty[currtty].e-1) % INPUT_BUF] != '\n'){
				tty[currtty].e--;
				consputc(BACKSPACE);
	}
	for(int i = 0; i < strlen(tty[currtty].commands[tty[currtty].currCommand]); i++){
		c = tty[currtty].commands[tty[currtty].currCommand][i];
		tty[currtty].buf[tty[currtty].e++ % INPUT_BUF] = c;
		consputc(c);
	}
}

void
downCommand(){
	int c;

	if(tty[currtty].currCommand > 0){
		tty[currtty].currCommand--;
	}

	while(tty[currtty].e != tty[currtty].w &&
			tty[currtty].buf[(tty[currtty].e-1) % INPUT_BUF] != '\n'){
				tty[currtty].e--;
				consputc(BACKSPACE);
	}
	for(int i = 0; i < strlen(tty[currtty].commands[tty[currtty].currCommand]); i++){
		c = tty[currtty].commands[tty[currtty].currCommand][i];
		tty[currtty].buf[tty[currtty].e++ % INPUT_BUF] = c;
		consputc(c);
	}
}


void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case 0xE2: // KEY_UP
			upperCommand();
			break;
		case 0xE3:
			downCommand();
			break;
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
	static int i = 0;
	char comm[128] = "";



	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){  // zavrsi se ako napunim dst ili naidjem na enter
		while(tty[ip->minor-1].r == tty[ip->minor-1].w){ // jednaki su ako nemam sta da citam (na enter w skoci na kraj tj. na e)
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&tty[ip->minor-1].r, &cons.lock); // blokira trenutni proces (prosledim broj koji je adresa od input.r)
		}

		c = tty[ip->minor-1].buf[tty[ip->minor-1].r++ % INPUT_BUF];
		tty[ip->minor-1].baft[tty[ip->minor-1].limit++ % TTY_BUF] = c; // cuvam u tty baferu

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
		if(c == '\n'){
			moveCommands(ip->minor-1);
			i = 0;

			break;
		}

		tty[ip->minor-1].commands[0][i++] = c; // cuvam komandu

	}
	// r ne mora da bude == w na kraju f-je (ako je korisnicki bafer manji od input bafera) onda ponovo pozove read pa se citanje nastavlja od r indeksa
	release(&cons.lock);
	ilock(ip);
//	cprintf("%s", tty[ip->minor-1].commands[tty[ip->minor-1].currCommand]);
	//
	return target - n;
}



int
consolewrite(struct inode *ip, char *buf, int n)
{


	int i;

	for (int j = 0; j < n; j++){
		tty[ip->minor-1].baft[tty[ip->minor-1].limit++ % TTY_BUF] = buf[j];
	}

	if(ip->minor == currtty + 1){ // ako je aktivni terminal
		if (init){
			setDefaultColors();
			init = 0;
		}
		// for(int i = 0; i < 80*25; i++) { // bg
		// 	crt[i] |= tty[currtty].ttyColor;
		// }
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

void
setColour(ushort colour, int type){
	ushort mask;
	ushort colourMask;
	if(type == 0){
		mask = 0x00ff;
	}else if(type == 1){
		mask = 0x0fff;
	}else if(type == 2){
		mask = 0xf0ff;
	}

	tty[currtty].ttyColor = (tty[currtty].ttyColor & mask) | colour;
	changeTty();
}


// 0 - both
// 1 - bg
// 2 - fg

int
sys_colour(void){

	ushort colour;
	int type;


	if(argint(0, &colour) || argint(1, &type)){
		return -1;
	}
	colour <<= 8;
	setColour(colour, type);
	//cprintf("%d", type);


	return 0;
}