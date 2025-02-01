#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>



// IO Registers
enum IORegisters {
	IRQ0ENABLE, IRQ1ENABLE, IRQ2ENABLE, IRQ0STATUS, IRQ1STATUS, IRQ2STATUS,
	IRQHANDLER, IRQRETURN, CLKS, LEDS, DISPLAY7SEG, TIMERENABLE,
	TIMERCURRENT, TIMERMAX, DISKCMD, DISKSECTOR, DISKBUFFER, DISKSTATUS, RESERVED0,  RESERVED1 ,
	MONITORADDR, MONITORDATA, MONITORCMD
};


#define MEMORY_SIZE 4096
#define REG_SIZE 16
#define IO_REG_SIZE 23
#define PC_ADDR_SIZE 1
#define SECTOR_SIZE 128
#define DISK_SIZE 128
#define MONITOR_SIZE 256


struct log
{
	struct status
	{
		unsigned long cycle;
		uint16_t pc;
		uint64_t inst;
		int32_t r[REG_SIZE];
		struct status* next;

	} *status_head, *status_tail;

	struct hw_access
	{
		unsigned long cycle;
		uint8_t rw;    // read:1, write:2
		uint8_t IOReg; // 0 <= IOReg <= 22
		uint32_t data;
		struct hw_access* next;

	} *hw_head, *hw_tail;

	struct irq2in
	{
		// interrupt 2 in at cycle 'cycle'.
		unsigned long cycle;
		struct irq2in* next;

	} *irq2in_head, *irq2in_tail;
};



// error message macro
#define err_msg(msg) \
    fprintf(stderr, "\nError: %s\npc: %d\nline: %d\n\n", msg, pc, __LINE__);

uint16_t pc;
uint8_t irq_busy;
unsigned long disk_last_cmd_cycle;
uint64_t i_mem[MEMORY_SIZE];
int32_t d_mem[MEMORY_SIZE];
int32_t r[REG_SIZE];
uint32_t IORegister[IO_REG_SIZE];
uint32_t disk[DISK_SIZE][SECTOR_SIZE];
// disk have 128 sectors, each sector have 512 bytes or 128 lines, each line have 4 bytes
uint8_t monitor[MONITOR_SIZE][MONITOR_SIZE]; // 256x256 pixel monitor, each pixel 8-bit
struct log data_log;
unsigned long cycles;



//Function declarations
const char* get_IO_reg_name(uint8_t io_addr);
int check_irq2in();
int interrupt_service_routine();
int handle_timer();
int sector_copy(uint32_t* dest, uint32_t* src);
int handle_disk();
int handle_monitor();
int tick_clk();
int read_file_diskin(char* diskin_file);
int write_file_diskout(char* diskout_file);
int write_file_monitor(char* monitor_file, uint8_t is_binary);
int update_log_status();
int update_log_hw_access(uint8_t rw, uint8_t IOReg);
int free_log_status();
int free_log_hw_access();
int free_irq2in();
int read_file_irq2in(char* irq2in_file);
int read_file_dmem_imem(char* dmem_file, char* imem_file);
int write_file_dmemout(char* dmemout_file);
int write_file_trace(char* trace_file);
int write_file_hwregtrace_leds_display7seg(char* hwregtrace_file, char* leds_file, char* display7seg_file);
int write_file_cycles_regout(char* cycles_file, char* regout_file);
uint32_t extend_sign(uint32_t reg, uint8_t sign_bit);
int execute_instruction();
int init(char* imemin_path, char* dmemin_path, char* diskin_path, char* irq_path);
int finalization(char* dmemout_path, char* regout_path, char* trace_path, char* hwregtrace_path, char* cycles_path, char* leds_path, char* display7seg_path, char* diskout_path, char* monitor_txt_path, char* monitor_yuv_path);



//maps the io_addr index to the register name
const char* get_IO_reg_name(uint8_t io_addr) {
	switch (io_addr) {
	case IRQ0ENABLE: return "irq0enable";
	case IRQ1ENABLE: return "irq1enable";
	case IRQ2ENABLE: return "irq2enable";
	case IRQ0STATUS: return "irq0status";
	case IRQ1STATUS: return "irq1status";
	case IRQ2STATUS: return "irq2status";
	case IRQHANDLER: return "irqhandler";
	case IRQRETURN: return "irqreturn";
	case CLKS: return "clks";
	case LEDS: return "leds";
	case DISPLAY7SEG: return "display7seg";
	case TIMERENABLE: return "timerenable";
	case TIMERCURRENT: return "timercurrent";
	case TIMERMAX: return "timermax";
	case DISKCMD: return "diskcmd";
	case DISKSECTOR: return "disksector";
	case DISKBUFFER: return "diskbuffer";
	case DISKSTATUS: return "diskstatus";
	case MONITORADDR: return "monitoraddr";
	case MONITORDATA: return "monitordata";
	case MONITORCMD: return "monitorcmd";
	default: return "UNKNOWN";
	}
}

//if reg irq2in exists in the current cycle, return 1, else return 0
int check_irq2in()
{
    struct irq2in* ptr0;
    struct irq2in* ptr1 = data_log.irq2in_head;
    while (ptr1 != NULL && (ptr1->cycle) < cycles)
    {
        // free current ptr1 and jump to the next one (current ptr1 doesn't neccessary anymore)
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        data_log.irq2in_head = ptr1;
        free(ptr0);
    }

    if (ptr1 != NULL)
        // return 1 if the next irq2in occurrs at the current cycle
        return (ptr1->cycle) == cycles;

    // there are no irq2in interrupts left
    return 0;
}

// handels interrupts-ISR
int interrupt_service_routine()
{
    if (irq_busy)
        return 0;

    IORegister[IRQ2STATUS] = check_irq2in(); // set irq2status interrupt

    int irq = (IORegister[IRQ0ENABLE] & IORegister[IRQ0STATUS]) |
        (IORegister[IRQ1ENABLE] & IORegister[IRQ1STATUS]) |
        (IORegister[IRQ2ENABLE] & IORegister[IRQ2STATUS]);

    if (irq == 1)
    {
        irq_busy = 1;
        IORegister[IRQRETURN] = pc;
        pc = IORegister[IRQHANDLER] & 0xfff; // pc is 12-bit
    }

    return 0;
}

//if the timer is enabeld and timercurrent == timermax, return 1, else return 0
int handle_timer()
{
    if (IORegister[TIMERENABLE] == 0)
        return 0;

    if (IORegister[TIMERCURRENT] == IORegister[TIMERMAX])
    {
        // timercurrent == timermax, raise irq0status to 1
        IORegister[TIMERCURRENT] = 0;
        IORegister[IRQ0STATUS] = 1;
    }
    else
        IORegister[TIMERCURRENT]++;

    return 0;
}

// copy src to dest for SECTOR_SIZE
int sector_copy(uint32_t* dest, uint32_t* src)
{
    uint8_t i;
    for (i = 0; i < SECTOR_SIZE; i++)
        dest[i] = src[i];
    return 0;
}

// copy src to dest for SECTOR_SIZE
int handle_disk()
{
    if (cycles - disk_last_cmd_cycle == 1024)
    {
        // 1024 cycles passed since the last disk read/write command.
        IORegister[DISKSTATUS] = 0; // free diskstatus
        IORegister[IRQ1STATUS] = 1; // Notify the proccessor: disk finished read or write command
    }
    
    if (IORegister[DISKSTATUS] || !IORegister[DISKCMD])
        // disk busy or doesn't want to do cmd
        return 0;


    disk_last_cmd_cycle = cycles;
    IORegister[DISKSTATUS] = 1;

    int32_t* buffer = &(d_mem[IORegister[DISKBUFFER]]);

    if (IORegister[DISKCMD] == 1)
        // diskcmd == read
        sector_copy(buffer, disk[IORegister[DISKSECTOR]]);

    else if (IORegister[DISKCMD] == 2)
        // diskcmd == write
        sector_copy(disk[IORegister[DISKSECTOR]], buffer);


    IORegister[DISKCMD] = 0; // set diskcmd=no command
    return 0;
}

//read/write from/to disk instructions
int handle_monitor()
{
    if (!IORegister[MONITORCMD])
        // monitorcmd == 0
        return 0;

    IORegister[MONITORCMD] = 0; // reset monitorcmd since the method about to execute write inst'.

    uint16_t monitoraddr = IORegister[MONITORADDR];
    uint8_t monitordata = IORegister[MONITORDATA];
    
    uint8_t row = monitoraddr >> 8;   // row is the 8-MSB of monitoraddr
    uint8_t col = monitoraddr & 0xff; // col is the 8-LSB of monitoraddr

    monitor[row][col] = monitordata;
    return 0;
}

// in clk
int tick_clk(){
    IORegister[CLKS]++;
    cycles++;
    return 0;
}

//read diskin_file into disk
int read_file_diskin(char* diskin_file){
    FILE* fdiskin;
    fdiskin = fopen(diskin_file, "r");
    if (fdiskin == NULL)
    {
        err_msg("open file");
        return 1;
    }
    int sector, i, eof_flag = 0;
    for (sector = 0; !eof_flag && sector < DISK_SIZE; sector++)
    {
        for (i = 0; !eof_flag && i < SECTOR_SIZE; i++)
        {
            if (fscanf(fdiskin, "%X", &(disk[sector][i])) != 1)
                eof_flag = 1;
        }
    }

    if (fclose(fdiskin) != 0)
        err_msg("close file");
    return 0;
}

//parth diskout_file to valid file with disk data
int write_file_diskout(char* diskout_file){
    FILE* fdiskout;
    int last_nonzero_line = -1, sector, i, eof_flag = 0;

    for (sector = 0; sector < DISK_SIZE; sector++)
    {
        for (i = 0; i < SECTOR_SIZE; i++)
        {
            if (disk[sector][i] != 0)
            {
                last_nonzero_line = SECTOR_SIZE * sector + i;
            }
        }
    }

    fdiskout = fopen(diskout_file, "w");
    if (fdiskout == NULL)
    {
        err_msg("open file");
        return 1;
    }

    if (last_nonzero_line == -1)
        eof_flag = 1;

    for (sector = 0; sector < DISK_SIZE && !eof_flag; sector++)
    {
        for (i = 0; i < SECTOR_SIZE && !eof_flag; i++)
        {
            fprintf(fdiskout, "%08X\n", disk[sector][i]);

            if (SECTOR_SIZE * sector + i >= last_nonzero_line)
                // this current line is the last line != 0, stop fprintf
                eof_flag = 1;

        }
    }

    if (fclose(fdiskout) != 0)
        err_msg("close file");
    return 0;
}

//write monitor data to monitor_file
int write_file_monitor(char* monitor_file, uint8_t is_binary){
    FILE* fmonitor;
    if (is_binary)
        fmonitor = fopen(monitor_file, "wb");
    else
        fmonitor = fopen(monitor_file, "w");

    if (fmonitor == NULL)
    {
        err_msg("open file");
        return 1;
    }

    if (is_binary)
    {
        fwrite(monitor, sizeof(uint8_t), MONITOR_SIZE * MONITOR_SIZE, fmonitor);

        if(fclose(fmonitor) != 0)
            err_msg("close file");

        return 0;
    }


    uint16_t i, j;
    for (i = 0; i < MONITOR_SIZE; i++)
    {
        for (j = 0; j < MONITOR_SIZE; j++)
        {

            fprintf(fmonitor, "%02X\n", monitor[i][j]);
        }
    }

    if (fclose(fmonitor) != 0)
        err_msg("close file");
    return 0;
}

//update log status to linked list
int update_log_status(){
    int i;
    struct status* status_p = (struct status*)malloc(sizeof(struct status));
    if (status_p == NULL)
    {
        err_msg("malloc");
        return 1;
    }
    status_p->pc = pc;
    status_p->inst = i_mem[pc];
    status_p->next = NULL;
    for (i = 0; i < REG_SIZE; i++)
        status_p->r[i] = r[i];

    if (data_log.status_head == NULL)
    {
        data_log.status_head = status_p;
        data_log.status_tail = status_p;
    }
    else
    {
        data_log.status_tail->next = status_p;
        data_log.status_tail = status_p;
    }

    return 0;
}

//update log io regester access
int update_log_hw_access(uint8_t rw, uint8_t IOReg){
    uint32_t data = IORegister[IOReg];
    struct hw_access* hw_acc_p = (struct hw_access*)malloc(sizeof(struct hw_access));
    if (hw_acc_p == NULL)
    {
        err_msg("malloc");
        return 1;
    }
    hw_acc_p->cycle = cycles;
    hw_acc_p->rw = rw;
    hw_acc_p->IOReg = IOReg;
    hw_acc_p->data = data;

    hw_acc_p->next = NULL;

    // Insert the new hw_access to end of linked list
    if (data_log.hw_head == NULL)
    {
        data_log.hw_head = hw_acc_p;
        data_log.hw_tail = hw_acc_p;
    }
    else
    {
        data_log.hw_tail->next = hw_acc_p;
        data_log.hw_tail = hw_acc_p;
    }
    return 0;
}


int free_log_status(){
    struct status* ptr0, * ptr1 = data_log.status_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

int free_log_hw_access(){
    struct hw_access* ptr0, * ptr1 = data_log.hw_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

int free_irq2in(){
    // free only irq2in that we havn't reached to them due to lower cycles from their occourences.
    struct irq2in* ptr0, * ptr1 = data_log.irq2in_head;

    while (ptr1 != NULL)
    {
        ptr0 = ptr1;
        ptr1 = ptr1->next;
        free(ptr0);
    }

    return 0;
}

//read irq2in_file into linked list each row is a node
int read_file_irq2in(char* irq2in_file){
    FILE* firq2in;
    firq2in = fopen(irq2in_file, "r");
    if (firq2in == NULL)
    {
        err_msg("open file");
        return 1;
    }
    unsigned long i;
    while (fscanf(firq2in, "%d", &i) == 1)
    {
        // add i to the end of the linked list
        struct irq2in* irq2in_p = (struct irq2in*)malloc(sizeof(struct irq2in));
        if (irq2in_p == NULL)
        {
            err_msg("malloc");
            return 1;
        }
        irq2in_p->cycle = i;
        irq2in_p->next = NULL;

        if (data_log.irq2in_head == NULL)
        {
            data_log.irq2in_head = irq2in_p;
            data_log.irq2in_tail = irq2in_p;
        }
        else
        {
            data_log.irq2in_tail->next = irq2in_p;
            data_log.irq2in_tail = irq2in_p;
        }
    }

    if (fclose(firq2in) != 0)
        err_msg("close file");
    return 0;
}

//read dmem_file,imem_file into d_mem, i_mem
int read_file_dmem_imem(char* dmem_file, char* imem_file){
    FILE* fdmem, * fimem;
    fdmem = fopen(dmem_file, "r");
    fimem = fopen(imem_file, "r");
    if (fdmem == NULL || fimem == NULL)
    {
        err_msg("open file");
        return 1;
    }
    uint16_t i;
    for (i = 0; i < MEMORY_SIZE && fscanf(fdmem, "%x", &d_mem[i]) == 1; i++)
        ;
    for (i = 0; i < MEMORY_SIZE && fscanf(fimem, "%llx", &i_mem[i]) == 1; i++)
        ;

    if (fclose(fdmem) != 0 || fclose(fimem) != 0)
        err_msg("close file");
    return 0;
}

//write d_mem to dmemout_file each line contains 8-hex digits
int write_file_dmemout(char* dmemout_file)
{
    int i;

    int last_nonzero_line = -1;

    for (i = 0; i < MEMORY_SIZE; i++)
    {
        if (d_mem[i] != 0)
            last_nonzero_line = i;
    }

    FILE* fdmemout;
    fdmemout = fopen(dmemout_file, "w");

    if (fdmemout == NULL)
    {
        err_msg("open file");
        return 1;
    }
    for (i = 0; i <= last_nonzero_line; i++)
        fprintf(fdmemout, "%08X\n", d_mem[i]);

    if (fclose(fdmemout) != 0)
        err_msg("close file");
    return 0;
}

//write trace file containing pc instruction and registers
int write_file_trace(char* trace_file){
    FILE* ftrace;
    ftrace = fopen(trace_file, "w");

    if (ftrace == NULL)
    {
        err_msg("open file");
        return 1;
    }
    uint8_t i;
    struct status* status_p = data_log.status_head;
    while (status_p != NULL)
    {
        // write to trace file
        fprintf(ftrace, "%03X ", status_p->pc);     // pc
        fprintf(ftrace, "%012llX ", status_p->inst); // inst
        for (i = 0; i < REG_SIZE - 1; i++)
            fprintf(ftrace, "%08x ", status_p->r[i]); // R[0] ... R[14]
        fprintf(ftrace, "%08x\n", status_p->r[i]);    // R[15]

        // jump to next status
        status_p = status_p->next;
    }

    if (fclose(ftrace) != 0)
        err_msg("close file");
    return 0;
}

//write hwregtrace_file, leds_file, display7seg_file(need IOrege while running)
int write_file_hwregtrace_leds_display7seg(char* hwregtrace_file, char* leds_file, char* display7seg_file)
{
    FILE* fhwregtrace, * fleds, * fdisplay7seg;
    fhwregtrace = fopen(hwregtrace_file, "w");
    fleds = fopen(leds_file, "w");
    fdisplay7seg = fopen(display7seg_file, "w");
    if (fhwregtrace == NULL || fleds == NULL || fdisplay7seg == NULL)
    {
        err_msg("open file");
        return 1;
    }

    struct hw_access* hw_p = data_log.hw_head;
    char text_read[] = "READ", text_write[] = "WRITE";
    while (hw_p != NULL)
    {
        fprintf(fhwregtrace, "%lu ", hw_p->cycle);

        if (hw_p->rw == 1)
            fprintf(fhwregtrace, "%s ", text_read);
        else if (hw_p->rw == 2)
            fprintf(fhwregtrace, "%s ", text_write);
		fprintf(fhwregtrace, "%s ", get_IO_reg_name(hw_p->IOReg));
        fprintf(fhwregtrace, "%08x\n", hw_p->data);

        if (hw_p->rw == 2){
            if (hw_p->IOReg == LEDS){
                fprintf(fleds, "%lu ", hw_p->cycle);
                fprintf(fleds, "%08x\n", hw_p->data);
            }else if (hw_p->IOReg == DISPLAY7SEG) {
                fprintf(fdisplay7seg, "%lu ", hw_p->cycle);
                fprintf(fdisplay7seg, "%08x\n", hw_p->data);
            }
        }
        hw_p = hw_p->next;
    }
    if (fclose(fhwregtrace) != 0 || fclose(fleds) != 0 || fclose(fdisplay7seg) != 0)
        err_msg("close file");

    return 0;
}

//write to files cycles number and registers at the end
int write_file_cycles_regout(char* cycles_file, char* regout_file){
    FILE* fcycles, * fregout;
    fcycles = fopen(cycles_file, "w");
    fregout = fopen(regout_file, "w");

    if (fcycles == NULL || fregout == NULL)
    {
        err_msg("open file");
        return 1;
    }

    fprintf(fcycles, "%lu\n", cycles);
    uint8_t i;
    for (i = 3; i < REG_SIZE; i++)
        fprintf(fregout, "%08x\n", r[i]);

    if (fclose(fcycles) != 0 || fclose(fregout) != 0)
        err_msg("close file");

    return 0;
}

//write to files cycles number and registers at the end
uint32_t extend_sign(uint32_t reg, uint8_t sign_bit){
    int sign = (reg >> sign_bit) & 1;
    if (sign)
        reg |= ~0 << sign_bit;
    return reg;
}

//execute instruction
int execute_instruction(){
    uint64_t inst = i_mem[pc];
    uint16_t prev_pc = pc; 
    uint16_t opcode, rd, rs, rt, rm, imm1, imm2;
    imm2 = (inst >> 0) & 0xfff;
    imm1 = (inst >> 12) & 0xfff;
    rm = (inst >> 24) & 0xf;
    rt = (inst >> 28) & 0xf;
    rs = (inst >> 32) & 0xf;
    rd = (inst >> 36) & 0xf;
    opcode = (inst >> 40) & 0xff;

    if (opcode < 0 || opcode > 21)
        return 2;

    r[0] = 0;                     
    r[1] = extend_sign(imm1, 11); 
    r[2] = extend_sign(imm2, 11); 

    update_log_status();

    switch (opcode)
    { 
    case 0:// add
        r[rd] = r[rs] + r[rt] + r[rm];
        break;
    case 1:// sub
        r[rd] = r[rs] - r[rt] - r[rm];
        break;
    case 2:// mac
        r[rd] = r[rs] * r[rt] + r[rm];
        break;
    case 3:// and
        r[rd] = r[rs] & r[rt] & r[rm];
        break;
    case 4:// or
        r[rd] = r[rs] | r[rt] | r[rm];
        break;
    case 5:// xor
        r[rd] = r[rs] ^ r[rt] ^ r[rm];
        break;
    case 6:// sll
        r[rd] = r[rs] << r[rt];
        break;
    case 7:// sra
        r[rd] = r[rs] >> r[rt];
        r[rd] = extend_sign(r[rd], 31 - r[rt]);
        break;
    case 8:// srl
        r[rd] = r[rs] >> r[rt];
        break;
    case 9:// beq
        if (r[rs] == r[rt])
            pc = r[rm] & 0xfff;
        break;
    case 10:// bne
        if (r[rs] != r[rt])
            pc = r[rm] & 0xfff;
        break;
    case 11:// blt
        if (r[rs] < r[rt])
            pc = r[rm] & 0xfff;
        break; 
    case 12:// bgt
        if (r[rs] > r[rt])
            pc = r[rm] & 0xfff;
        break;
    case 13:// ble
        if (r[rs] <= r[rt])
            pc = r[rm] & 0xfff;
        break;
    case 14: // bge
        if (r[rs] >= r[rt])
            pc = r[rm] & 0xfff;
        break;
    case 15:// jal
        r[rd] = (pc + 1) & 0xfff;
        pc = r[rm] & 0xfff;
        break; 
    case 16:// lw
        r[rd] = d_mem[(r[rs] + r[rt]) & 0xfff] + r[rm];
        break;
    case 17:// sw
        d_mem[(r[rs] + r[rt]) & 0xfff] = r[rm] + r[rd];
        break; 
    case 18:// reti
        pc = IORegister[IRQRETURN];
        irq_busy = 0;
        break;
    case 19:// in
        if (r[rs] + r[rt] >= IO_REG_SIZE)
            break;
        r[rd] = IORegister[r[rs] + r[rt]];
        update_log_hw_access(1, r[rs] + r[rt]);
        break;
    case 20:// out
        if (r[rs] + r[rt] >= IO_REG_SIZE)
            break;
        IORegister[r[rs] + r[rt]] = r[rm];
        update_log_hw_access(2, r[rs] + r[rt]);
        break;
        
    case 21:// halt
        return 1;
    }
    if (prev_pc == pc)
        pc = (pc + PC_ADDR_SIZE) & 0xfff; // ,ask to 12-bit
    r[0] = 0; 
    return 0;
}

//read input files abd put into structures
int init(char* imemin_path, char* dmemin_path, char* diskin_path, char* irq_path){
    pc = 0;
    cycles = 0;
    data_log.status_head = NULL;
    data_log.hw_head = NULL;
    data_log.irq2in_head = NULL;
    irq_busy = 0;
    disk_last_cmd_cycle = ~0;

    if (read_file_dmem_imem(dmemin_path, imemin_path) != 0 ||
        read_file_diskin(diskin_path) != 0 ||
        read_file_irq2in(irq_path))
        return 1;

    return 0;
}

//write output files and free memory
int finalization(char* dmemout_path, char* regout_path, char* trace_path, char* hwregtrace_path, char* cycles_path,char* leds_path, char* display7seg_path, char* diskout_path, char* monitor_txt_path, char* monitor_yuv_path){

    if (write_file_dmemout(dmemout_path) != 0 ||
        write_file_diskout(diskout_path) != 0 ||
        write_file_trace(trace_path) != 0 ||
        write_file_hwregtrace_leds_display7seg(hwregtrace_path, leds_path, display7seg_path) != 0 ||
        write_file_cycles_regout(cycles_path, regout_path) != 0 ||
        write_file_monitor(monitor_txt_path, 0) != 0 ||
        write_file_monitor(monitor_yuv_path, 1) != 0)
        return 1;

    free_log_status();
    free_log_hw_access();
    free_irq2in();
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc != 15)
        return 1;

    if (init(argv[1], argv[2], argv[3], argv[4]) != 0)
        return 1;
    int halt_flag = 0;

    while (pc < MEMORY_SIZE && !halt_flag)
    {
        switch (execute_instruction())
        {
        case 1:
            // HALT
            halt_flag = 1;
            break;

        case 2:
            //invalid opcode.
            err_msg("Invalid opcode");
            return 1;
        }

        handle_monitor();
        handle_timer();
        handle_disk();

        interrupt_service_routine();

        tick_clk(); // iteration's end!
    }

    if (finalization(argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], argv[12], argv[13], argv[14]) != 0)
        return 1;

    return 0;
}
