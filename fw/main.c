
static inline __attribute__((always_inline)) int get_tid(void)
{
	register int tid __asm__("x4");
	return tid;
}

int main()
{
	volatile int res = 5 + get_tid();
	volatile int val = 4;

	volatile int my_tid = get_tid();

	if(get_tid() == 0) {
		val = res + val;
	}

	while(1) {
		__asm__ volatile ("nop");
	}
	return 0;
}
