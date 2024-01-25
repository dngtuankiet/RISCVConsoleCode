/* Copyright (c) 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See the file LICENSE for further information */

#include "main.h"
#include "encoding.h"
#include <stdint.h>
// #include <stdlib.h>
#include <string.h>
// #include <stdatomic.h>
#include "libfdt/libfdt.h"
#include "uart/uart.h"
#include "trng/trng.h"
#include <kprintf/kprintf.h>
// #include <stdio.h>
#include "utils/wc_func.h"


#include <platform.h>
#include <stdatomic.h>
#include <plic/plic_driver.h>

//Kiet custom
#include "user_settings.h"
// #include "utils/wolf_utils.h"
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

#define ECQV_CURVE ECC_SECP256K1
#define KEYSIZE 32

volatile unsigned long dtb_target;

// Structures for registering different interrupt handlers
// for different parts of the application.
void no_interrupt_handler (void) {};
function_ptr_t g_ext_interrupt_handlers[32];
function_ptr_t g_time_interrupt_handler = no_interrupt_handler;
plic_instance_t g_plic;// Instance data for the PLIC.

#define RTC_FREQ 1000000 // TODO: This is now extracted

// #ifdef WOLFSSL_STATIC_MEMORY
//     #define STATIC_MEM_SIZE (1*1024*1024)
//     static WOLFSSL_HEAP_HINT* HEAP_HINT;
//     static byte gMemory[STATIC_MEM_SIZE];
// #endif


// typedef struct ecc_spec{
//     const ecc_set_type* spec;
//     mp_int prime;
//     mp_int af;
//     mp_int order;
//     ecc_point* G;
//     int idx;
//     /* Montgomery parameters */
//     mp_int mu; // Montgomery normalizer
//     mp_digit mp; // The "b" value from montgomery_setup()
// } ecc_spec;

// typedef struct ecc_spec{
//     const ecc_set_type* spec;
//     mp_int prime;
//     mp_int af;
//     mp_int order;
//     ecc_point* G;
//     int idx;
// } ecc_spec;

// void my_bio_dump_line(uint32_t cnt, unsigned char* s, int len){
//   kputs("\r");
//   uart_put_hex((void*) uart_reg, cnt*16);
//   kputs(" - ");
//   for(int i = 0; i < len; i++){
//       uart_put_hex_1b((void*) uart_reg, (uint8_t) s[i]);
//   }
//   kprintf("\n");
// }

// void my_bio_dump(unsigned char* s, int len){
//   int cnt = len/16;
//   for (int line = 0; line <cnt; line++){
//       my_bio_dump_line(line, s+line*16, 16);
//   }
//   int mod = len %(cnt*16);
//   if(mod != 0){
//       my_bio_dump_line(cnt+1, s+cnt*16,mod);
//   }
// }


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
unsigned long trng_reg = 0;
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

  // custom peripheral get reg values
  nodeoffset = fdt_node_offset_by_compatible((void*)dtb, 0, "console,trng0");
  if (nodeoffset < 0) {
    kputs("\r\nCannot find a node with compatible 'console,trng0'\r\nAborting...");
    while(1);
  }
  err = fdt_get_node_addr_size((void*)dtb_target, nodeoffset, &trng_reg, NULL);
  if(err < 0){
    kputs("\r\nCannot get reg space from compatible 'console,trng0'\r\nAborting...");
    while(1);
  }


  // TODO: From this point, insert any code
  kputs("\r\n\n\nWelcome Kiet!\r\n\n");
  uint32_t rand = 0;
  int ret = 0;

  char demo[10];
  memset(demo, 1, 10);
  for (size_t i = 0; i < 10; i++)
  {
    /* code */
    kprintf("demo[%d]: %d\n", i, demo[i]);
  }
  
  ret = trng_setup((void*)trng_reg, (0x1 << 11));
  if(ret != TRNG_OKAY){
    kprintf("Error setup trng\n");
    goto end;
  }else{
    for(int i = 0; i < 10; i++){
      rand = trng_get_random((void*)trng_reg);
      if(rand == TRNG_ERROR_RANDOM){
        kprintf("Errot gen random\n");
        goto end;
      }
      kprintf("random number %d: %d \n",i, rand);
    }
  }


  #ifdef WOLFSSL_STATIC_MEMORY
    WOLFSSL_HEAP_HINT* HEAP_HINT = NULL;
  #endif


  // If finished, stay in a infinite loop
  kputs("\rTest Program with WolfSSl baremetal\r\n\n");
  #ifdef WOLFSSL_STATIC_MEMORY
    // WOLFSSL_HEAP_HINT* HEAP_HINT_TEST = NULL;
    if(wc_LoadStaticMemory(&HEAP_HINT, gMemory, sizeof(gMemory), WOLFMEM_GENERAL, 0) != 0){
      kputs("\rUnable to load static memory\n");
      goto end;
    }else{
      kputs("\rSuccessfully load static memory\n");
    }
    if(HEAP_HINT == NULL){
      kprintf("HEAP_HINT is still NULL\n");
      goto end;
    }else{
      kprintf("Successfully loaded static memory address to HEAP_HINT\n");
      kprintf("HEAP_HINT location: %x\n",HEAP_HINT);
    }
  #endif
  wolfCrypt_Init();


  kputs("\r\nDemo wolfcrypt without openssl layer\n");
  kputs("\r1. Gen curve specs\n");
  ecc_spec curve;
  curve.idx = wc_ecc_get_curve_idx(ECQV_CURVE);
  // kputs("\r\ncurve idx: ");
  // uart_put_dec((void*)uart_reg, curve.idx);
  kprintf("curve idx: %d\n", curve.idx);
  curve.spec = wc_ecc_get_curve_params(curve.idx);
  // kputs("\r\ncurve size: ");
  // uart_put_dec((void*)uart_reg, curve.spec->size);
  kprintf("curve size: %d\n", curve.spec->size);

  //get base point data (order, af, prime, G) from data in the library
  mp_init_multi(&(curve.af),&(curve.prime),&(curve.order),NULL,NULL,NULL);
  curve.G = NULL;
  curve.G = wc_ecc_new_point_h((void*)HEAP_HINT);

  ecc_point test;
  wc_ecc_get_generator(&test, curve.idx); //require --enable-opensslall to work
  kprintf("\t[Server] Test base G\n");
  print_point(&curve, &test);

  ecc_point test2;
  mp_int Gx;
  mp_int Gy;
  mp_init(&Gx);
  mp_init(&Gy);
  mp_read_radix(&Gx, "79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798", 16); //convert const char* to big number base 16
  mp_read_radix(&Gy, "483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8", 16); //convert const char* to big number base 16
  mp_copy(&Gx, test2.x);
  mp_copy(&Gy, test2.y);
  mp_set(&(test2.z), 1);
  kprintf("\t[Server] Test2 base G\n");
  print_point(&curve, &test2);

  if(curve.G == NULL){
    kprintf("ERROR Base point NULL");
    goto end;
  }else{
    kprintf("Base point created\n");
  }
  mp_read_radix(&(curve.order), curve.spec->order, 16); //convert const char* to big number base 16
  mp_read_radix(&(curve.af), curve.spec->Af, 16); //convert const char* to big number base 16
  mp_read_radix(&(curve.prime), curve.spec->prime, 16); //convert const char* to big number base 16
  wc_ecc_get_generator(curve.G, curve.idx); //require --enable-opensslall to work
  

  print_sp("\t[Server] Curve af", &(curve.af));
  print_sp("\t[Server] Curve order", &(curve.order));
  print_sp("\t[Server] Curve prime", &(curve.prime));
  printf("\t[Server] Curve base G\n");
  print_point(&curve, curve.G);

  /* Calculate the Montgomery normalizer. */
  if (mp_montgomery_calc_normalization(&(curve.mu), &(curve.prime)) != MP_OKAY) {
      kprintf("mp_montgomery_calc_normalization error");
      ret = 0;
  }
  print_sp("\tMontgomery mu", &(curve.mu));

  mp_montgomery_setup(&(curve.prime), &(curve.mp));
  kprintf("\tMontgomery mp: %ld\n", curve.mp);



  kputs("\r\n\n2. Generate keys from curve specs\n");
  ecc_key key;
  WC_RNG rng;
  ret = wc_ecc_init_ex(&key, HEAP_HINT, INVALID_DEVID);
  if(ret != MP_OKAY){
      kputs("\rInit key failed\n");
      goto end;
  }
  kputs("\rInit key with HEAP_HINT OK\n");

  ret = wc_InitRng_ex(&rng, HEAP_HINT, INVALID_DEVID);
//    ret = wc_InitRng(&rng);
  if(ret != MP_OKAY){
    kputs("\rInit RNG failed\n");
    if(ret == DRBG_CONT_FIPS_E){
      kprintf("rng DRBG_CONT_FIPS_E\n");
    }else if(ret == RNG_FAILURE_E){
      kprintf("rng RNG_FAILURE_E\n");
    }else{
      kprintf("rng stop at %d\n", ret);
    }
    goto end;
  }
  kputs("\rInit key with HEAP_HINT OK\n");

  ret = wc_ecc_gen_k(&rng, KEYSIZE, key.k, &(curve.order));
  kprintf("private key generated OK\n");


  ret = wc_ecc_mulmod_ex(key.k, curve.G, &(key.pubkey), &(curve.af), &(curve.prime), 1,HEAP_HINT);
  kprintf("public key generated OK\n");

  print_sp("Private key",key.k);
  kprintf("Public key: \n");
  print_point(&curve, &(key.pubkey));  

  kprintf("Verifying key...\n");

  ret = wc_ecc_check_key(&key);
  if(ret != MP_OKAY){
      kprintf("-> Server key check FAILED at %d\n", ret);
      goto end;
  }else{
      kprintf("\t[Server] key check SUCCESS\n");
      print_sp("\t[Server] d_ca",key.k);
      kprintf("\t[Server] Q_ca:\n");
      print_point(&curve, &(key.pubkey));
  }


  /*==========================*/

  // server S;
  // node N;
  // ecc_spec curve;
  // // WC_RNG rng;
  // char temp[ID_SIZE] = {'a','b','c','d','e','f','g','h'};

  // kprintf("\nSetup Phase:\n");
  // kprintf("1. User set its own UID\n");

  // memcpy(N.id,temp, ID_SIZE);
  // kprintf("\tAssign Node ID: ");
  // for(int i = 0; i < ID_SIZE; i++){
  //     kprintf("%x", N.id[i]);
  // }
  // kprintf("\n");

  // kprintf("2. Server generate its private key d_ca and public key Q_ca\n");
  
  // ret = initial_setup(&curve, &S, &N);
  // if(ret != MP_OKAY){
  //     kprintf("Setup phase FAILED\n");
  //     return -1;
  // }








end:
  kputs("\r\nComplete test library\n");
  wolfCrypt_Cleanup();
  while(1);

  //dead code
  return 0;
}

