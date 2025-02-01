#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 500
#define MAX_LABEL_LENGTH 50
#define MAX_LABEL_COUNT 100
#define MAX_INSTRUCTIONS 4096
#define MAX_DMEM_SIZE 4096

typedef struct {
    char label[MAX_LABEL_LENGTH];
    int address;
} Label;

typedef struct {
    char opcode[10];
    char rd[10], rs[10], rt[10], rm[10];
    char imm1[50], imm2[50];
} Instruction;

// Function prototypes
void processWordDirective(char *line, int *dataMemory);
int parseInstruction(char *line, Instruction *instr);
unsigned long long encodeInstruction(const Instruction *instr, Label labels[], int labelCount);
int resolveImmediate(const char *imm, Label labels[], int labelCount);
void writeImemFile(const unsigned long long *instructions, int instructionCount, const char *filename);
void writeDmemFile(const int *dataMemory, const char *filename);
int getOpcode(const char *mnemonic);
int getRegister(const char *reg);
int readLabels(FILE *inputFile, Label labels[], int *labelCount);
void secondPass(const char *inputFilename, const char *imemFilename, const char *dmemFilename, Label labels[], int labelCount);

// First pass to read labels
int readLabels(FILE *inputFile, Label labels[], int *labelCount) {
    char line[MAX_LINE_LENGTH];
    int lineAddress = 0;
    *labelCount = 0;

    while (fgets(line, MAX_LINE_LENGTH, inputFile)) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *trimmedLine = line;
        while (isspace(*trimmedLine)) trimmedLine++;
        if (*trimmedLine == '\0') continue;

        char *colon = strchr(trimmedLine, ':');
        if (colon) {
            *colon = '\0';
            char *labelName = trimmedLine;

            char *end = labelName + strlen(labelName) - 1;
            while (end > labelName && isspace(*end)) *end-- = '\0';

            if (*labelCount >= MAX_LABEL_COUNT) {
                fprintf(stderr, "Error: Too many labels\n");
                return 1;
            }

            strncpy(labels[*labelCount].label, labelName, MAX_LABEL_LENGTH - 1);
            labels[*labelCount].label[MAX_LABEL_LENGTH - 1] = '\0';
            labels[*labelCount].address = lineAddress;
            (*labelCount)++;
        } else if (strstr(trimmedLine, ".word") == NULL) {
            lineAddress++; // Count only instructions, not .word
        }
    }

    return 0;
}

// Second pass: Process instructions and encode them
void secondPass(const char *inputFilename, const char *imemFilename, const char *dmemFilename, Label labels[], int labelCount) {
    FILE *inputFile = fopen(inputFilename, "r");
    if (!inputFile) {
        perror("Error opening input file");
        exit(1);
    }

    unsigned long long instructions[MAX_INSTRUCTIONS] = {0};
    int dataMemory[MAX_DMEM_SIZE] = {0};
    int instructionCount = 0;

    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, inputFile)) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        if (strstr(line, ".word") != NULL) {
            processWordDirective(line, dataMemory);
            continue; // Skip processing as an instruction
        }

        if (strchr(line, ':')) continue; // Skip label declarations

        Instruction instr = {0};
        if (parseInstruction(line, &instr)) {
            instructions[instructionCount++] = encodeInstruction(&instr, labels, labelCount);
        }
    }

    fclose(inputFile);

    writeImemFile(instructions, instructionCount, imemFilename);
    writeDmemFile(dataMemory, dmemFilename);
}

// Process `.word` directive
void processWordDirective(char *line, int *dataMemory) {
    char addressStr[50], valueStr[50];
    int address, value;

    while (isspace(*line)) line++; // Skip leading whitespace
    if (strncmp(line, ".word", 5) == 0) {
        line += 5; // Skip ".word"
        while (isspace(*line)) line++;

        if (sscanf(line, "%49s %49s", addressStr, valueStr) == 2) {
            address = (int)strtol(addressStr, NULL, 0); // Supports hex and decimal
            value = (int)strtol(valueStr, NULL, 0);

            if (address >= 0 && address < MAX_DMEM_SIZE) {
                dataMemory[address] = value; // Store value in data memory
            } else {
                fprintf(stderr, "Error: Address %d out of bounds for .word\n", address);
            }
        } else {
            fprintf(stderr, "Error: Invalid .word format: %s\n", line);
        }
    } else {
        fprintf(stderr, "Error: Line is not a valid .word directive: %s\n", line);
    }
}

// Parse instruction into an `Instruction` struct
int parseInstruction(char *line, Instruction *instr) {
    while (isspace(*line)) line++;

    char *token = strtok(line, " ,");
    if (!token) return 0;
    strncpy(instr->opcode, token, sizeof(instr->opcode) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->rd, token, sizeof(instr->rd) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->rs, token, sizeof(instr->rs) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->rt, token, sizeof(instr->rt) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->rm, token, sizeof(instr->rm) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->imm1, token, sizeof(instr->imm1) - 1);

    token = strtok(NULL, " ,");
    if (token) strncpy(instr->imm2, token, sizeof(instr->imm2) - 1);

    return 1;
}

// Encode instruction
unsigned long long encodeInstruction(const Instruction *instr, Label labels[], int labelCount) {
    int opcodeBin = getOpcode(instr->opcode);
    int rdBin = getRegister(instr->rd);
    int rsBin = getRegister(instr->rs);
    int rtBin = getRegister(instr->rt);
    int rmBin = getRegister(instr->rm);

    int imm1 = resolveImmediate(instr->imm1, labels, labelCount);
    int imm2 = resolveImmediate(instr->imm2, labels, labelCount);

    return ((unsigned long long)opcodeBin << 40) |
           ((unsigned long long)rdBin << 36) |
           ((unsigned long long)rsBin << 32) |
           ((unsigned long long)rtBin << 28) |
           ((unsigned long long)rmBin << 24) |
           ((unsigned long long)(imm1 & 0xFFF) << 12) |
           (unsigned long long)(imm2 & 0xFFF);
}

// Resolve immediate value
int resolveImmediate(const char *imm, Label labels[], int labelCount) {
    if (!imm || strlen(imm) == 0) return 0;

    char trimmedImm[50];
    strncpy(trimmedImm, imm, sizeof(trimmedImm) - 1);
    trimmedImm[sizeof(trimmedImm) - 1] = '\0';

    // Remove leading and trailing spaces
    char *start = trimmedImm;
    while (isspace(*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) *end-- = '\0';

    // If the immediate is a number (hex or decimal)
    if (isdigit(start[0]) || start[0] == '-' || start[0] == '+') {
        if (strncmp(start, "0x", 2) == 0 || strncmp(start, "0X", 2) == 0) {
            return (int)strtol(start, NULL, 16); // Hexadecimal
        }
        return atoi(start); // Decimal
    }

    // If the immediate is a label, resolve it
    for (int i = 0; i < labelCount; i++) {
        if (strcmp(labels[i].label, start) == 0) {
            return labels[i].address;
        }
    }

    fprintf(stderr, "Error: Undefined immediate or label '%s'\n", start);
    exit(1);
}


// Write to instruction memory file
void writeImemFile(const unsigned long long *instructions, int instructionCount, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening imem file");
        exit(1);
    }

    for (int i = 0; i < instructionCount; i++) {
        fprintf(file, "%012llX\n", instructions[i]);
    }

    for (int i = instructionCount; i < MAX_INSTRUCTIONS; i++) {
        fprintf(file, "000000000000\n");
    }

    fclose(file);
}

// Write to data memory file
void writeDmemFile(const int *dataMemory, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening dmem file");
        exit(1);
    }

    for (int i = 0; i < MAX_DMEM_SIZE; i++) {
        fprintf(file, "%08X\n", dataMemory[i]);
    }

    fclose(file);
}

// Helper functions for registers and opcodes
int getRegister(const char *reg) {
    if (strcmp(reg, "$zero") == 0) return 0;
    if (strcmp(reg, "$imm1") == 0) return 1;
    if (strcmp(reg, "$imm2") == 0) return 2;
    if (strcmp(reg, "$v0") == 0) return 3;
    if (strcmp(reg, "$a0") == 0) return 4;
    if (strcmp(reg, "$a1") == 0) return 5;
    if (strcmp(reg, "$a2") == 0) return 6;
    if (strcmp(reg, "$t0") == 0) return 7;
    if (strcmp(reg, "$t1") == 0) return 8;
    if (strcmp(reg, "$t2") == 0) return 9;
    if (strcmp(reg, "$s0") == 0) return 10;
    if (strcmp(reg, "$s1") == 0) return 11;
    if (strcmp(reg, "$s2") == 0) return 12;
    if (strcmp(reg, "$gp") == 0) return 13;
    if (strcmp(reg, "$sp") == 0) return 14;
    if (strcmp(reg, "$ra") == 0) return 15;
    fprintf(stderr, "Error: Invalid register '%s'\n", reg);
    exit(1);
}

int getOpcode(const char *mnemonic) {
    if (strcmp(mnemonic, "add") == 0) return 0;
    if (strcmp(mnemonic, "sub") == 0) return 1;
    if (strcmp(mnemonic, "mac") == 0) return 2;
    if (strcmp(mnemonic, "and") == 0) return 3;
    if (strcmp(mnemonic, "or") == 0) return 4;
    if (strcmp(mnemonic, "xor") == 0) return 5;
    if (strcmp(mnemonic, "sll") == 0) return 6;
    if (strcmp(mnemonic, "sra") == 0) return 7;
    if (strcmp(mnemonic, "srl") == 0) return 8;
    if (strcmp(mnemonic, "beq") == 0) return 9;
    if (strcmp(mnemonic, "bne") == 0) return 10;
    if (strcmp(mnemonic, "blt") == 0) return 11;
    if (strcmp(mnemonic, "bgt") == 0) return 12;
    if (strcmp(mnemonic, "ble") == 0) return 13;
    if (strcmp(mnemonic, "bge") == 0) return 14;
    if (strcmp(mnemonic, "jal") == 0) return 15;
    if (strcmp(mnemonic, "lw") == 0) return 16;
    if (strcmp(mnemonic, "sw") == 0) return 17;
    if (strcmp(mnemonic, "reti") == 0) return 18;
    if (strcmp(mnemonic, "in") == 0) return 19;
    if (strcmp(mnemonic, "out") == 0) return 20;
    if (strcmp(mnemonic, "halt") == 0) return 21;
    fprintf(stderr, "Error: Invalid opcode '%s'\n", mnemonic);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input.asm> <imemin.txt> <dmemin.txt>\n", argv[0]);
        return 1;
    }

    const char *inputFilename = argv[1];
    const char *imemFilename = argv[2];
    const char *dmemFilename = argv[3];

    Label labels[MAX_LABEL_COUNT];
    int labelCount = 0;

    FILE *inputFile = fopen(inputFilename, "r");
    if (!inputFile) {
        perror("Error opening input file");
        return 1;
    }

    if (readLabels(inputFile, labels, &labelCount) != 0) {
        fprintf(stderr, "Error processing labels in first pass.\n");
        fclose(inputFile);
        return 1;
    }
    fclose(inputFile);

    secondPass(inputFilename, imemFilename, dmemFilename, labels, labelCount);

    return 0;
}
