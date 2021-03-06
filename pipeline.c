/*  CISC 340 Project 3 -- Pipeline.c
 *  A pipelined implementation of the UST-3400 ISA.
 *  Drew Wilken, Nathan Taylor
 *  9 April 2018
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 /* JALR – not implemented in this project */
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

typedef struct IFIDstruct{
    int instr;
    int pcplus1;
} IFIDType;

typedef struct IDEXstruct{
    int instr;
    int pcplus1;
    int readregA;
    int readregB;
    int offset;
} IDEXType;

typedef struct EXMEMstruct{
    int instr;
    int branchtarget;
    int aluresult;
    int readreg;
} EXMEMType;

typedef struct MEMWBstruct{
    int instr;
    int writedata;
} MEMWBType;

typedef struct WBENDstruct{
    int instr;
    int writedata;
} WBENDType;

typedef struct statestruct{
    int pc;
    int instrmem[NUMMEMORY];
    int datamem[NUMMEMORY];
    int reg[NUMREGS];
    int nummemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    int cycles; /* Number of cycles run so far */
    int fetched; /* Total number of instructions fetched */
    int retired; /* Total number of completed instructions */
    int branches; /* Total number of branches executed */
    int mispreds; /* Number of branch mispredictions*/
} statetype;

int field0(int instruction);
int field1(int instruction);
int field2(int instruction);
int opcode(int instruction);
void printinstruction(int instr);
void printstate(statetype *stateptr);
int signextend(int num);
void printBits(int pack);

/*
 * Pipeline stage functions
 */
void IFStage(statetype* state, statetype* newstate);
void IDStage(statetype* state, statetype* newstate);
void EXStage(statetype* state, statetype* newstate, int* retired);
void MEMStage(statetype* state, statetype* newstate, int* retired);
void WBStage(statetype* state, statetype* newstate, int* retired);

statetype state;
statetype newstate;

int main(int argc, char *argv[]){
    
    // Taken verbatim from "sim.c" from Canvas.
    /** Get command line arguments **/
    char* fname;
    
    if(argc == 1){
        fname = (char*)malloc(sizeof(char)*100);
        printf("Enter the name of the machine code file to simulate: ");
        fgets(fname, 100, stdin);
        fname[strlen(fname)-1] = '\0'; // gobble up the \n with a \0
    }
    else if (argc == 2){
        
        int strsize = strlen(argv[1]);
        
        fname = (char*)malloc(strsize);
        fname[0] = '\0';
        
        strcat(fname, argv[1]);
    }else{
        printf("Please run this program correctly\n");
        exit(-1);
    }
    
    FILE *fp = fopen(fname, "r");
    if (fp == NULL) {
        printf("Cannot open file '%s' : %s\n", fname, strerror(errno));
        return -1;
    }
    
    /* count the number of lines by counting newline characters */
    int line_count = 0;
    int c;
    while (EOF != (c=getc(fp))) {
        if ( c == '\n' ){
            line_count++;
        }
    }
    // reset fp to the beginning of the file
    rewind(fp);
    
    //statetype* state = (statetype*)malloc(sizeof(statetype));
    
    state.pc = 0;
    memset(state.datamem, 0, NUMMEMORY*sizeof(int));
    memset(state.reg, 0, NUMREGS*sizeof(int));
    
    state.nummemory = line_count;
    
    char line[256];
    
    int i = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* note that fgets doesn't strip the terminating \n, checking its
         presence would allow to handle lines longer that sizeof(line) */
        state.datamem[i] = atoi(line);
        state.instrmem[i] = atoi(line);
        i++;
    }
    fclose(fp);
    
    //count retired
    int retired_count = 0;
    
    // Init all PC reg to 0.
    
    state.cycles = 0;
    state.fetched = 0;
    state.retired = 0;
    state.branches = 0;
    state.mispreds = 0;
    
    // Init all instr to NOOP.
    
    state.IFID.instr = NOOPINSTRUCTION;
    state.IDEX.instr = NOOPINSTRUCTION;
    state.EXMEM.instr = NOOPINSTRUCTION;
    state.MEMWB.instr = NOOPINSTRUCTION;
    state.WBEND.instr = NOOPINSTRUCTION;
    
    // The bulk of main is a loop, where each iteration is a clock cycle.
    // At the start of each iteration, printstate().
    
    //stateType state;
    
    while(1){
        printstate(&state);
        /* check for halt */
        if(HALT == opcode(state.MEMWB.instr)) {
            printf("machine halted\n");
            printf("total of %d cycles executed\n", state.cycles);
            printf("total of %d instructions fetched\n", state.fetched);
            printf("total of %d instructions retired\n", state.retired);
            printf("total of %d branches executed\n", state.branches);
            printf("total of %d branch mispredictions\n", state.mispreds);
            exit(0);
        }//if halt
        
        newstate = state;
        newstate.cycles++;
        
        /*------------------ IF stage ----------------- */
        IFStage(&state, &newstate);
        
        
        /*------------------ ID stage ----------------- */
        IDStage(&state, &newstate);
        
        
        /*------------------ EX stage ----------------- */
        EXStage(&state, &newstate, &retired_count);
        
        /*------------------ MEM stage ----------------- */
        MEMStage(&state, &newstate, &retired_count);
        
        /*------------------ WB stage ----------------- */
        WBStage(&state, &newstate, &retired_count);
        /*ENDStage(&state, &newstate);*/
        
        
        state = newstate; /* this is the last statement before the end of the loop.
                           It marks the end of the cycle and updates the current
                           state with the values calculated in this cycle
                           – AKA “Clock Tick”. */
    }//while
}//main


int field0(int instruction){
    return( (instruction>>19) & 0x7);
}

int field1(int instruction){
    return( (instruction>>16) & 0x7);
}

int field2(int instruction){
    return(instruction & 0xFFFF);
}

int opcode(int instruction){
    return(instruction>>22);
}

void printinstruction(int instr) {
    char opcodestring[10];
    if (opcode(instr) == ADD) {
        strcpy(opcodestring, "add");
    } else if (opcode(instr) == NAND) {
        strcpy(opcodestring, "nand");
    } else if (opcode(instr) == LW) {
        strcpy(opcodestring, "lw");
    } else if (opcode(instr) == SW) {
        strcpy(opcodestring, "sw");
    } else if (opcode(instr) == BEQ) {
        strcpy(opcodestring, "beq");
    } else if (opcode(instr) == JALR) {
        strcpy(opcodestring, "jalr");
    } else if (opcode(instr) == HALT) {
        strcpy(opcodestring, "halt");
    } else if (opcode(instr) == NOOP) {
        strcpy(opcodestring, "noop");
    } else {
        strcpy(opcodestring, "data");
    }
    
    if(opcode(instr) == ADD || opcode(instr) == NAND){
        printf("%s %d %d %d\n", opcodestring, field2(instr), field0(instr), field1(instr));
    }else if(0 == strcmp(opcodestring, "data")){
        printf("%s %d\n", opcodestring, signextend(field2(instr)));
    }else{
        printf("%s %d %d %d\n", opcodestring, field0(instr), field1(instr), signextend(field2(instr)));
    }
}

void printstate(statetype *stateptr){
    int i;
    printf("\n@@@\nstate before cycle %d starts\n", stateptr->cycles);
    printf("\tpc %d\n", stateptr->pc);
    
    printf("\tdata memory:\n");
    for (i=0; i<stateptr->nummemory; i++) {
        printf("\t\tdatamem[ %d ] %d\n", i, stateptr->datamem[i]);
    }
    printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, stateptr->reg[i]);
    }
    printf("\tIFID:\n");
    printf("\t\tinstruction ");
    printinstruction(stateptr->IFID.instr);
    printf("\t\tpcplus1 %d\n", stateptr->IFID.pcplus1);
    printf("\tIDEX:\n");
    printf("\t\tinstruction ");
    printinstruction(stateptr->IDEX.instr);
    printf("\t\tpcplus1 %d\n", stateptr->IDEX.pcplus1);
    printf("\t\treadregA %d\n", stateptr->IDEX.readregA);
    printf("\t\treadregB %d\n", stateptr->IDEX.readregB);
    printf("\t\toffset %d\n", stateptr->IDEX.offset);
    printf("\tEXMEM:\n");
    printf("\t\tinstruction ");
    printinstruction(stateptr->EXMEM.instr);
    printf("\t\tbranchtarget %d\n", stateptr->EXMEM.branchtarget);
    printf("\t\taluresult %d\n", stateptr->EXMEM.aluresult);
    printf("\t\treadregB %d\n", stateptr->EXMEM.readreg);
    printf("\tMEMWB:\n");
    printf("\t\tinstruction ");
    printinstruction(stateptr->MEMWB.instr);
    printf("\t\twritedata %d\n", stateptr->MEMWB.writedata);
    printf("\tWBEND:\n");
    printf("\t\tinstruction ");
    printinstruction(stateptr->WBEND.instr);
    printf("\t\twritedata %d\n", stateptr->WBEND.writedata);
}
int signextend(int num){
    if(num & (1<<15)) {
        num -=(1<<16);
    }
    return(num);
}


/*
 * PIPELINE STAGE FUNCTIONS
 */
void IFStage(statetype* state, statetype* newstate) {
    /*
     * Grab instruction from memory and store in new state's pipeline register for IF
     */
    if(state->IFID.instr == state->instrmem[state->pc-1]) {
        newstate->fetched = state->fetched + 1;
    }
    newstate->pc = state->pc+1;
    newstate->IFID.pcplus1 = state->pc + 1;
    newstate->IFID.instr = state->instrmem[state->pc];
    newstate->pc = state->pc+1;
    
    if(state->IFID.instr == state->instrmem[state->pc]) {
        printinstruction(state->IFID.instr);
        printinstruction(state->instrmem[state->pc]);
        newstate->fetched = state->fetched + 1;
    }
}
void IDStage(statetype* state, statetype* newstate) {
    /*
     * Decode instruction and store in
     */
    newstate->IDEX.instr = state->IFID.instr;
    newstate->IDEX.pcplus1 = state->IFID.pcplus1;
    newstate->IDEX.readregA = state->reg[field0(state->IFID.instr)]; //grab registers from instr
    newstate->IDEX.readregB = state->reg[field1(state->IFID.instr)]; //grab register from instr
    newstate->IDEX.offset = signextend(field2(state->IFID.instr)); //grab offset/dst_reg from instr
}
void EXStage(statetype* state, statetype* newstate, int* retired) {
    newstate->EXMEM.instr = state->IDEX.instr;
    newstate->EXMEM.branchtarget = 0;
    newstate->EXMEM.aluresult = 0;
    
    //temporary read regs
    int temp_readregA = state->IDEX.readregA;
    int temp_readregB = state->IDEX.readregB;
    
    /*
     * ADD // NAND
     */
    if(opcode(state->IDEX.instr) == ADD || opcode(state->IDEX.instr) == NAND || opcode(state->IDEX.instr) == LW || opcode(state->IDEX.instr) == SW || opcode(state->IDEX.instr) == BEQ) {
        
        //IF PREVIOUS instr is LW
        if( opcode(state->EXMEM.instr) == LW) {
            
            //If either regA or regB are being loaded into in future instr
            if(field0(state->EXMEM.instr) == field0(state->IDEX.instr) || field0(state->EXMEM.instr) == field1(state->IDEX.instr)) {
                
                //load stall
                newstate->pc -= 1;
                newstate->EXMEM.instr = NOOPINSTRUCTION;
                newstate->IDEX = state->IDEX;
                newstate->IFID = state->IFID;
                *retired = *retired-1;
            }
        }
        else {
            //readregA
            if(field2(state->EXMEM.instr) == field0(state->IDEX.instr)) {
                temp_readregA = state->EXMEM.aluresult;
            }else if(field2(state->MEMWB.instr) == field0(state->IDEX.instr)) {
                temp_readregA = state->MEMWB.writedata;
            }else if(field2(state->WBEND.instr) == field0(state->IDEX.instr)) {
                temp_readregA = state->WBEND.writedata;
            }
            
            //readregB
            if(field2(state->EXMEM.instr) == field1(state->IDEX.instr)) {
                temp_readregB = state->EXMEM.aluresult;
            }else if(field2(state->MEMWB.instr) == field1(state->IDEX.instr)) {
                temp_readregB = state->MEMWB.writedata;
            }else if(field2(state->WBEND.instr) == field1(state->IDEX.instr)) {
                temp_readregB = state->WBEND.writedata;
            }
            
            //LW DATA GRABBING
            if( opcode(state->MEMWB.instr) == LW || opcode(state->WBEND.instr) == LW) {
                if( field0(state->MEMWB.instr) == field0(state->IDEX.instr) ) {
                    temp_readregA = state->MEMWB.writedata;
                }
                else if( field0(state->WBEND.instr) == field0(state->IDEX.instr) ) {
                    temp_readregA = state->WBEND.writedata;
                }
                
                if( field0(state->MEMWB.instr) == field1(state->IDEX.instr) ) {
                    temp_readregB = state->MEMWB.writedata;
                }
                else if( field0(state->WBEND.instr) == field1(state->IDEX.instr) ) {
                    temp_readregB = state->WBEND.writedata;
                }
            }
        }
        
        //Operations with updated register values
        if(opcode(state->IDEX.instr) == ADD) {
            newstate->EXMEM.aluresult = temp_readregA + temp_readregB;
        }else if(opcode(state->IDEX.instr) == NAND) {
            newstate->EXMEM.aluresult = ~(temp_readregA&temp_readregB);
        }else if(opcode(state->IDEX.instr) == LW){
            newstate->EXMEM.aluresult = temp_readregB + state->IDEX.offset;
        }else if(opcode(state->IDEX.instr) == SW){
            newstate->EXMEM.aluresult = temp_readregB + state->IDEX.offset;
        }else if(opcode(state->IDEX.instr) == BEQ){
            newstate->EXMEM.aluresult = temp_readregA - temp_readregB; //difference between. If 0, they are equal
            newstate->EXMEM.branchtarget = state->IDEX.offset + state->IDEX.pcplus1;
        }
    }
    
    newstate->EXMEM.readreg = temp_readregA; //for store word, this would be the only thing we would use : register a
}
void MEMStage(statetype* state, statetype* newstate, int* retired) {
    newstate->MEMWB.instr = state->EXMEM.instr;
    newstate->MEMWB.writedata = state->EXMEM.aluresult;
    
    if(opcode(state->EXMEM.instr) == SW) {
        newstate->datamem[state->EXMEM.aluresult] = state->EXMEM.readreg;
    }else if(opcode(state->EXMEM.instr) == LW) {
        newstate->MEMWB.writedata = state->datamem[state->EXMEM.aluresult];
    }else if(opcode(state->EXMEM.instr) == BEQ) {
        
        //if the regs are equal
        if(state->EXMEM.aluresult == 0) {
            newstate->pc = state->EXMEM.branchtarget; //jump to branch target
            
            newstate->mispreds = state->mispreds + 1;
            
            //clear previous buffers
            newstate->IFID.instr = NOOPINSTRUCTION;
            newstate->IDEX.instr = NOOPINSTRUCTION;
            newstate->EXMEM.instr = NOOPINSTRUCTION;
            
            //account for flushed pipeline
            *retired = *retired-3;
            
        }else {
            newstate->branches = state->branches + 1;
        }
        newstate->MEMWB.writedata = 0; //??
    }
}
void WBStage(statetype* state, statetype* newstate, int* retired) {
    newstate->WBEND.instr = state->MEMWB.instr;
    newstate->WBEND.writedata = state->MEMWB.writedata;
    
    if(opcode(state->MEMWB.instr) == ADD || opcode(state->MEMWB.instr) == NAND) {
        newstate->reg[field2(state->MEMWB.instr)] = state->MEMWB.writedata;
    }else if(opcode(state->MEMWB.instr) == LW){
        newstate->reg[field0(state->MEMWB.instr)] = state->MEMWB.writedata;
    }
    
    //retire instruction
    *retired = *retired+1;
    newstate->retired = *retired;
}
void printBits(int pack) {
    int i;
    int temp = pack;
    
    for(i=31;i>=0;i--) {
        if(((temp>>i)&1) == 1) {
            printf("1");
        }
        else {
            printf("0");
        }
    }
    putchar('\n');
}

