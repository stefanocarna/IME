#define valid_vector(vec)	(vec > 0 && vec < NR_VECTORS)
#define NMI_NAME	"ime"
#define MAX_ID_PMC 3


int enable_nmi(void);

void disable_nmi(void);

void cleanup_pmc(void);

int enable_on_apic(void);

void disable_on_apic(void);

void print_reg(void);
