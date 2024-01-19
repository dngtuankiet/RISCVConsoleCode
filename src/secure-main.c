/* Copyright (c) 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See the file LICENSE for further information */

#include "main.h"
#include "encoding.h"
//#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "libfdt/libfdt.h"
#include "uart/uart.h"
#include <kprintf/kprintf.h>
#include <stdio.h>



#include <platform.h>
#include <stdatomic.h>
#include <plic/plic_driver.h>

//Kiet custom
#include "user_settings.h"
#include "utils/wolf_utils.h"
//#include <wolfssl/wolfcrypt/settings.h>

//#include <wolfssl/ssl.h>
//#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/openssl/ec.h>
//#include <wolfssl/wolfcrypt/random.h>
//#include <wolfssl/wolfcrypt/sp_int.h>
//#include <wolfssl/wolfcrypt/integer.h>
//#include <wolfssl/wolfcrypt/wolfmath.h>

#define ECQV_CURVE ECC_SECP256K1
#define KEYSIZE 32
#define STATIC_MEM_SIZE (200*1024)

volatile unsigned long dtb_target;

// Structures for registering different interrupt handlers
// for different parts of the application.
void no_interrupt_handler (void) {};
function_ptr_t g_ext_interrupt_handlers[32];
function_ptr_t g_time_interrupt_handler = no_interrupt_handler;
plic_instance_t g_plic;// Instance data for the PLIC.

#define RTC_FREQ 1000000 // TODO: This is now extracted

#ifdef WOLFSSL_STATIC_MEMORY
//    static WOLFSSL_HEAP_HINT* HEAP_HINT_TEST;
    static byte gTestMemory[STATIC_MEM_SIZE];
//#else
//    #define HEAP_HINT_TEST NULL
#endif

typedef struct ecc_spec{
    const ecc_set_type* spec;
    mp_int prime;
    mp_int af;
    mp_int order;
    ecc_point* G;
    int idx;
} ecc_spec;

void my_bio_dump_line(uint32_t cnt, unsigned char* s, int len){
  kputs("\r");
  uart_put_hex((void*) uart_reg, cnt*16);
  kputs(" - ");
  for(int i = 0; i < len; i++){
      uart_put_hex_1b((void*) uart_reg, (uint8_t) s[i]);
  }
  kprintf("\n");
}

void my_bio_dump(unsigned char* s, int len){
  int cnt = len/16;
  for (int line = 0; line <cnt; line++){
      my_bio_dump_line(line, s+line*16, 16);
  }
  int mod = len %(cnt*16);
  if(mod != 0){
      my_bio_dump_line(cnt+1, s+cnt*16,mod);
  }
}


void boot_fail(long code, int trap)
{
  kputs("BOOT FAILED\r\nCODE: ");
  uart_put_hex((void*)uart_reg, code);
  kputs("\r\nTRAP: ");
  uart_put_hex((void*)uart_reg, trap);
  while(1);
}

void handle_m_ext_interrupt(){
  int int_num  = PLIC_claim_interrupt(&g_plic);
  if ((int_num >=1 ) && (int_num < 32/*plic_ndevs*/)) {
    g_ext_interrupt_handlers[int_num]();
  }
  else {
    kputs("\rhandle_m_ext_interrupt\r\n");
    boot_fail((long) read_csr(mcause), 1);
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
  }
  PLIC_complete_interrupt(&g_plic, int_num);
}

void handle_m_time_interrupt() {
  clear_csr(mie, MIP_MTIP);

  // Reset the timer for 1s in the future.
  // This also clears the existing timer interrupt.

  volatile unsigned long *mtime    = (unsigned long*)(CLINT_CTRL_ADDR + CLINT_MTIME);
  volatile unsigned long *mtimecmp = (unsigned long*)(CLINT_CTRL_ADDR + CLINT_MTIMECMP);
  unsigned long now = *mtime;
  unsigned long then = now + RTC_FREQ;
  *mtimecmp = then;

  g_time_interrupt_handler();

  // Re-enable the timer interrupt.
  set_csr(mie, MIP_MTIP);
}

uintptr_t handle_trap(uintptr_t mcause, uintptr_t epc)
{
  // External Machine-Level interrupt from PLIC
  if ((mcause & MCAUSE_INT) && ((mcause & MCAUSE_CAUSE) == IRQ_M_EXT)) {
    handle_m_ext_interrupt();
    // External Machine-Level interrupt from PLIC
  } else if ((mcause & MCAUSE_INT) && ((mcause & MCAUSE_CAUSE) == IRQ_M_TIMER)){
    handle_m_time_interrupt();
  }
  else {
    kputs("\rhandle_trap\r\n");
    boot_fail((long) read_csr(mcause), 1);
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
  }
  return epc;
}

// Helpers for fdt

void remove_from_dtb(void* dtb_target, const char* path) {
  int nodeoffset;
  int err;
	do{
    nodeoffset = fdt_path_offset((void*)dtb_target, path);
    if(nodeoffset >= 0) {
      kputs("\r\nINFO: Removing ");
      kputs(path);
      err = fdt_del_node((void*)dtb_target, nodeoffset);
      if (err < 0) {
        kputs("\r\nWARNING: Cannot remove a subnode ");
        kputs(path);
      }
    }
  } while (nodeoffset >= 0) ;
}

static int fdt_translate_address(void *fdt, uint64_t reg, int parent,
				 unsigned long *addr)
{
	int i, rlen;
	int cell_addr, cell_size;
	const fdt32_t *ranges;
	uint64_t offset = 0, caddr = 0, paddr = 0, rsize = 0;

	cell_addr = fdt_address_cells(fdt, parent);
	if (cell_addr < 1)
		return -FDT_ERR_NOTFOUND;

	cell_size = fdt_size_cells(fdt, parent);
	if (cell_size < 0)
		return -FDT_ERR_NOTFOUND;

	ranges = fdt_getprop(fdt, parent, "ranges", &rlen);
	if (ranges && rlen > 0) {
		for (i = 0; i < cell_addr; i++)
			caddr = (caddr << 32) | fdt32_to_cpu(*ranges++);
		for (i = 0; i < cell_addr; i++)
			paddr = (paddr << 32) | fdt32_to_cpu(*ranges++);
		for (i = 0; i < cell_size; i++)
			rsize = (rsize << 32) | fdt32_to_cpu(*ranges++);
		if (reg < caddr || caddr >= (reg + rsize )) {
			//kprintf("invalid address translation\n");
			return -FDT_ERR_NOTFOUND;
		}
		offset = reg - caddr;
		*addr = paddr + offset;
	} else {
		/* No translation required */
		*addr = reg;
	}

	return 0;
}

int fdt_get_node_addr_size(void *fdt, int node, unsigned long *addr,
			   unsigned long *size)
{
	int parent, len, i, rc;
	int cell_addr, cell_size;
	const fdt32_t *prop_addr, *prop_size;
	uint64_t temp = 0;

	parent = fdt_parent_offset(fdt, node);
	if (parent < 0)
		return parent;
	cell_addr = fdt_address_cells(fdt, parent);
	if (cell_addr < 1)
		return -FDT_ERR_NOTFOUND;

	cell_size = fdt_size_cells(fdt, parent);
	if (cell_size < 0)
		return -FDT_ERR_NOTFOUND;

	prop_addr = fdt_getprop(fdt, node, "reg", &len);
	if (!prop_addr)
		return -FDT_ERR_NOTFOUND;
	prop_size = prop_addr + cell_addr;

	if (addr) {
		for (i = 0; i < cell_addr; i++)
			temp = (temp << 32) | fdt32_to_cpu(*prop_addr++);
		do {
			if (parent < 0)
				break;
			rc  = fdt_translate_address(fdt, temp, parent, addr);
			if (rc)
				break;
			parent = fdt_parent_offset(fdt, parent);
			temp = *addr;
		} while (1);
	}
	temp = 0;

	if (size) {
		for (i = 0; i < cell_size; i++)
			temp = (temp << 32) | fdt32_to_cpu(*prop_size++);
		*size = temp;
	}

	return 0;
}

int fdt_parse_hart_id(void *fdt, int cpu_offset, uint32_t *hartid)
{
	int len;
	const void *prop;
	const fdt32_t *val;

	if (!fdt || cpu_offset < 0)
		return -FDT_ERR_NOTFOUND;

	prop = fdt_getprop(fdt, cpu_offset, "device_type", &len);
	if (!prop || !len)
		return -FDT_ERR_NOTFOUND;
	if (strncmp (prop, "cpu", strlen ("cpu")))
		return -FDT_ERR_NOTFOUND;

	val = fdt_getprop(fdt, cpu_offset, "reg", &len);
	if (!val || len < sizeof(fdt32_t))
		return -FDT_ERR_NOTFOUND;

	if (len > sizeof(fdt32_t))
		val++;

	if (hartid)
		*hartid = fdt32_to_cpu(*val);

	return 0;
}

int fdt_parse_max_hart_id(void *fdt, uint32_t *max_hartid)
{
	uint32_t hartid;
	int err, cpu_offset, cpus_offset;

	if (!fdt)
		return -FDT_ERR_NOTFOUND;
	if (!max_hartid)
		return 0;

	*max_hartid = 0;

	cpus_offset = fdt_path_offset(fdt, "/cpus");
	if (cpus_offset < 0)
		return cpus_offset;

	fdt_for_each_subnode(cpu_offset, fdt, cpus_offset) {
		err = fdt_parse_hart_id(fdt, cpu_offset, &hartid);
		if (err)
			continue;

		if (hartid > *max_hartid)
			*max_hartid = hartid;
	}

	return 0;
}

int fdt_find_or_add_subnode(void *fdt, int parentoffset, const char *name)
{
  int offset;

  offset = fdt_subnode_offset(fdt, parentoffset, name);

  if (offset == -FDT_ERR_NOTFOUND)
    offset = fdt_add_subnode(fdt, parentoffset, name);

  if (offset < 0) {
  	uart_puts((void*)uart_reg, fdt_strerror(offset));
  	uart_puts((void*)uart_reg, "\r\n");
  }

  return offset;
}
int timescale_freq = 0;

// Register to extract
unsigned long uart_reg = 0;
int tlclk_freq;
unsigned long plic_reg;
int plic_max_priority;
int plic_ndevs;
int timescale_freq;

//HART 0 runs main
int main(int id, unsigned long dtb)
{
  // Use the FDT to get some devices
  int nodeoffset;
  int err = 0;
  int len;
	const fdt32_t *val;

  // 1. Get the uart reg
  nodeoffset = fdt_path_offset((void*)dtb, "/soc/serial");
  if (nodeoffset < 0) while(1);
  err = fdt_get_node_addr_size((void*)dtb, nodeoffset, &uart_reg, NULL);
  if (err < 0) while(1);
  // NOTE: If want to force UART, uncomment these
  //uart_reg = 0x64000000;
  //tlclk_freq = 20000000;
  _REG32(uart_reg, UART_REG_TXCTRL) = UART_TXEN;
  _REG32(uart_reg, UART_REG_RXCTRL) = UART_RXEN;

  // 2. Get tl_clk
  nodeoffset = fdt_path_offset((void*)dtb, "/soc/subsystem_pbus_clock");
  if (nodeoffset < 0) {
    kputs("\r\nCannot find '/soc/subsystem_pbus_clock'\r\nAborting...");
    while(1);
  }
  val = fdt_getprop((void*)dtb, nodeoffset, "clock-frequency", &len);
  if(!val || len < sizeof(fdt32_t)) {
    kputs("\r\nThere is no clock-frequency in '/soc/subsystem_pbus_clock'\r\nAborting...");
    while(1);
  }
  if (len > sizeof(fdt32_t)) val++;
  tlclk_freq = fdt32_to_cpu(*val);
  _REG32(uart_reg, UART_REG_DIV) = uart_min_clk_divisor(tlclk_freq, 115200);

  // 3. Get the mem_size
  nodeoffset = fdt_path_offset((void*)dtb, "/memory");
  if (nodeoffset < 0) {
    kputs("\r\nCannot find '/memory'\r\nAborting...");
    while(1);
  }
  unsigned long mem_base, mem_size;
  err = fdt_get_node_addr_size((void*)dtb, nodeoffset, &mem_base, &mem_size);
  if (err < 0) {
    kputs("\r\nCannot get reg space from '/memory'\r\nAborting...");
    while(1);
  }
  unsigned long ddr_size = (unsigned long)mem_size; // TODO; get this
  unsigned long ddr_end = (unsigned long)mem_base + ddr_size;

  // 4. Get the number of cores
  uint32_t num_cores = 0;
  err = fdt_parse_max_hart_id((void*)dtb, &num_cores);
  num_cores++; // Gives maxid. For max cores we need to add 1

  // 5. Get the plic parameters
  nodeoffset = fdt_path_offset((void*)dtb, "/soc/interrupt-controller");
  if (nodeoffset < 0) {
    kputs("\r\nCannot find '/soc/interrupt-controller'\r\nAborting...");
    while(1);
  }

  err = fdt_get_node_addr_size((void*)dtb, nodeoffset, &plic_reg, NULL);
  if (err < 0) {
    kputs("\r\nCannot get reg space from '/soc/interrupt-controller'\r\nAborting...");
    while(1);
  }

  val = fdt_getprop((void*)dtb, nodeoffset, "riscv,ndev", &len);
  if(!val || len < sizeof(fdt32_t)) {
    kputs("\r\nThere is no riscv,ndev in '/soc/interrupt-controller'\r\nAborting...");
    while(1);
  }
  if (len > sizeof(fdt32_t)) val++;
  plic_ndevs = fdt32_to_cpu(*val);

  val = fdt_getprop((void*)dtb, nodeoffset, "riscv,max-priority", &len);
  if(!val || len < sizeof(fdt32_t)) {
    kputs("\r\nThere is no riscv,max-priority in '/soc/interrupt-controller'\r\nAborting...");
    while(1);
  }
  if (len > sizeof(fdt32_t)) val++;
  plic_max_priority = fdt32_to_cpu(*val);

  // Disable the machine & timer interrupts until setup is done.
  clear_csr(mstatus, MSTATUS_MIE);
  clear_csr(mie, MIP_MEIP);
  clear_csr(mie, MIP_MTIP);

  if(plic_reg != 0) {
    PLIC_init(&g_plic,
              plic_reg,
              plic_ndevs,
              plic_max_priority);
  }

  // Display some information
#define DEQ(mon, x) ((cdate[0] == mon[0] && cdate[1] == mon[1] && cdate[2] == mon[2]) ? x : 0)
  const char *cdate = __DATE__;
  int month =
    DEQ("Jan", 1) | DEQ("Feb",  2) | DEQ("Mar",  3) | DEQ("Apr",  4) |
    DEQ("May", 5) | DEQ("Jun",  6) | DEQ("Jul",  7) | DEQ("Aug",  8) |
    DEQ("Sep", 9) | DEQ("Oct", 10) | DEQ("Nov", 11) | DEQ("Dec", 12);

  char date[11] = "YYYY-MM-DD";
  date[0] = cdate[7];
  date[1] = cdate[8];
  date[2] = cdate[9];
  date[3] = cdate[10];
  date[5] = '0' + (month/10);
  date[6] = '0' + (month%10);
  date[8] = cdate[4];
  date[9] = cdate[5];

  // Post the serial number and build info
  extern const char * gitid;

  kputs("\r\nRATONA Demo:       ");
  kputs(date);
  kputs("-");
  kputs(__TIME__);
  kputs("-");
  kputs(gitid);
  kputs("\r\nGot TL_CLK: ");
  uart_put_dec((void*)uart_reg, tlclk_freq);
  kputs("\r\nGot NUM_CORES: ");
  uart_put_dec((void*)uart_reg, num_cores);

  // Copy the DTB
  dtb_target = ddr_end - 0x200000UL; // - 2MB
  err = fdt_open_into((void*)dtb, (void*)dtb_target, 0x100000UL); // - 1MB only for the DTB
  if (err < 0) {
    kputs(fdt_strerror(err));
    kputs("\r\n");
    boot_fail(-err, 4);
  }
  //memcpy((void*)dtb_target, (void*)dtb, fdt_size(dtb));

  // Put the choosen if non existent, and put the bootargs
  nodeoffset = fdt_find_or_add_subnode((void*)dtb_target, 0, "chosen");
  if (nodeoffset < 0) boot_fail(-nodeoffset, 2);

  const char* str = "console=hvc0 earlycon=sbi";
  err = fdt_setprop((void*)dtb_target, nodeoffset, "bootargs", str, strlen(str) + 1);
  if (err < 0) boot_fail(-err, 3);

  // Get the timebase-frequency for the cpu@0
  nodeoffset = fdt_path_offset((void*)dtb_target, "/cpus/cpu@0");
  if (nodeoffset < 0) {
    kputs("\r\nCannot find '/cpus/cpu@0'\r\nAborting...");
    while(1);
  }
  val = fdt_getprop((void*)dtb_target, nodeoffset, "timebase-frequency", &len);
  if(!val || len < sizeof(fdt32_t)) {
    kputs("\r\nThere is no timebase-frequency in '/cpus/cpu@0'\r\nAborting...");
    while(1);
  }
  if (len > sizeof(fdt32_t)) val++;
  timescale_freq = fdt32_to_cpu(*val);
  kputs("\r\nGot TIMEBASE: ");
  uart_put_dec((void*)uart_reg, timescale_freq);

	// Put the timebase-frequency for the cpus
  nodeoffset = fdt_subnode_offset((void*)dtb_target, 0, "cpus");
	if (nodeoffset < 0) {
	  kputs("\r\nCannot find 'cpus'\r\nAborting...");
    while(1);
	}
	err = fdt_setprop_u32((void*)dtb_target, nodeoffset, "timebase-frequency", 1000000);
	if (err < 0) {
	  kputs("\r\nCannot set 'timebase-frequency' in 'timebase-frequency'\r\nAborting...");
    while(1);
	}

	// Pack the FDT and place the data after it
	fdt_pack((void*)dtb_target);


  // TODO: From this point, insert any code
  kputs("\r\n\n\nWelcome Kiet!\r\n\n");
//  char demo[3] = "abc";
//  char test[3];
////  char* test = (char*)malloc(sizeof(char)*4);
//  memcpy(test,demo,3);
//  *(test+4) = '\n';
//  kputs(test);
//  kputs("Test memcpy\n");
//  int r = memcmp(test,demo,3);
//  if(r == 0){
//    kputs("Identical mem\n");
//  }else{
//    kputs("Non-identical mem\n");
//  }


//  int testNum = 1997;
//  int cpNum;
//  memcpy((void*)cpNum, (void*)testNum, 4);
//  kputs("\r\nTest memcpy (should be 1997): ");
//  uart_put_dec((void*)uart_reg, cpNum);


  // If finished, stay in a infinite loop
  kputs("\rTest Program with WolfSSl baremetal\r\n\n");
  #ifdef WOLFSSL_STATIC_MEMORY
    WOLFSSL_HEAP_HINT* HEAP_HINT_TEST = NULL;
    if(wc_LoadStaticMemory(&HEAP_HINT_TEST, gTestMemory, sizeof(gTestMemory), WOLFMEM_GENERAL, 1) != 0){
      kputs("\rUnable to load static memory\n");
      while(1);
    }else{
      kputs("\rSuccessfully load static memory\n");
    }
  #endif
wolfCrypt_Init();

  /*==========================*/
  kputs("\nTest sha256\n");
  Sha256 sha256;
  byte data[] = {0x61,0x62,0x63};
  byte result[32];
  word32 data_len = sizeof(data);
  int ret;

  if ((ret = wc_InitSha256(&sha256)) != 0) {
      kputs("\rError init sha256!\n");
  }
  else {
    wc_Sha256Update(&sha256, data, data_len);
    wc_Sha256Final(&sha256, result); //result finished here
    wc_Sha256Free(&sha256); //free allocated
  }
  my_bio_dump(result, 32);
  kputs("\rComplete hash test\n");
  /*==========================*/

    kputs("\r\nDemo wolfcrypt without openssl layer\n");
    kputs("\r1. Gen curve specs\n");
    ecc_spec curve;
    curve.idx = wc_ecc_get_curve_idx(ECQV_CURVE);
    kputs("\r\ncurve idx: ");
    uart_put_dec((void*)uart_reg, curve.idx);
    curve.spec = wc_ecc_get_curve_params(curve.idx);
    kputs("\r\ncurve size: ");
    uart_put_dec((void*)uart_reg, curve.spec->size);

    //get base point data (order, af, prime, G) from data in the library
    mp_init_multi(&(curve.af),&(curve.prime),&(curve.order),NULL,NULL,NULL);
    curve.G = wc_ecc_new_point();
    mp_read_radix(&(curve.order), curve.spec->order, 16); //convert const char* to big number base 16
    mp_read_radix(&(curve.af), curve.spec->Af, 16); //convert const char* to big number base 16
    mp_read_radix(&(curve.prime), curve.spec->prime, 16); //convert const char* to big number base 16
    wc_ecc_get_generator(curve.G, curve.idx);

    kputs("\r\n\n2. Generate keys from curve specs\n");
    ecc_key key;
    WC_RNG rng;
    ret = wc_ecc_init(&key);
    if(ret != MP_OKAY){
        kputs("\rInit key failed\n");
        goto end;
    }
    kputs("\rInit key OK\n");

    ret = wc_InitRng_ex(&rng, HEAP_HINT_TEST, INVALID_DEVID);
//    ret = wc_InitRng(&rng);
    if(ret != MP_OKAY){
      kputs("\rInit RNG failed\n");
      goto end;
    }
    kputs("\rInit RGN OK\n");

    kputs("\rInitialzied completed\n");
    ret = wc_ecc_set_curve(&key, KEYSIZE, ECQV_CURVE);
//    ret = wc_ecc_gen_k(&rng, KEYSIZE, key.k, &(curve.order));

//    ret = wc_ecc_make_key(&rng, 32, &key);
    ret = wc_ecc_make_key_ex(&rng, 32, &key, ECQV_CURVE);

    if(ret != MP_OKAY){
        kputs("\r\nError gen private key: ");
        uart_put_dec((void*)uart_reg, ret);
    }else{
        kputs("\r\nGen key successful\n");
    }








//    ecc_key key;
//    wc_ecc_init(&key);
//    WC_RNG rng;
//    wc_InitRng(&rng);
//    int curveID = ECC_SECP256K1;
//    int keySize = wc_ecc_get_curve_size_from_id(curveID);
//    ret = wc_ecc_make_key_ex(&rng, keySize, &key, curveID);
//    if(ret != MP_OKAY){
//        kputs("\r\nError gen key \n");
//    }
//
//    ret = wc_ecc_check_key(&key);
//    if(ret != MP_OKAY){
//        kputs("\r\nServer key gen failed\n");
//
//    }else{
//        kputs("\r\nServer key gen success\n");
//    }


//    user U;
//    server S;
//    WOLFSSL_EC_GROUP *group = wolfSSL_EC_GROUP_new_by_curve_name(NID_secp256k1);

//    byte UID[3] = "abc";
//    U.UID = UID;
//    U.key = EC_KEY_new();
//    wolfSSL_EC_KEY_set_group(U.key, group);


end:
  kputs("\r\nComplete test library\n");
wolfCrypt_Cleanup();
  while(1);

  //dead code
  return 0;
}
