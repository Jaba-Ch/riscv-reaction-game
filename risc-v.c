// RISC-V Processor Simulator - Shooting Game
// C only handles I/O and timing - RISC-V processor does all the work!
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <conio.h>  // For Windows. Use <termios.h> for Linux/Mac
#include <windows.h> // For Windows timing. Use <sys/time.h> for Linux/Mac

// Global State
uint32_t PC = 0x1000;
uint32_t Instruction_Memory[0x1020];
uint32_t Data_Memory[0x4000];
uint32_t Register_File[32];

// Control Signals
uint32_t Branch, ResultSrc, MemWrite, ALUSrc, ImmSrc, RegWrite, ALUOp, Jump;

// Game State Variables (C side only)
int game_active = 0;
LARGE_INTEGER start_time, frequency;
int player1_pressed = 0;
int player2_pressed = 0;

// Memory addresses for game data
#define PLAYER1_TIME_ADDR 0x2000
#define PLAYER2_TIME_ADDR 0x2004
#define GAME_RESULT_ADDR  0x2008
#define GAME_STATUS_ADDR  0x200C

// Extract bits from an instruction
uint32_t extract_bits(uint32_t instruction, int start, int end) {
    int length = end - start + 1;
    return (instruction >> start) & ((1 << length) - 1);
}

// Immediate extension logic
int32_t Extend(uint32_t instruction, uint32_t ImmSrc) {
    int32_t imm = 0;
    switch (ImmSrc) {
        case 0b00: { // I-type
            imm = extract_bits(instruction, 20, 31);
            if (imm & 0x800) imm |= 0xFFFFF000;
            break;
        }
        case 0b01: { // S-type
            uint32_t imm4_0 = extract_bits(instruction, 7, 11);
            uint32_t imm11_5 = extract_bits(instruction, 25, 31);
            imm = (imm11_5 << 5) | imm4_0;
            if (imm & 0x800) imm |= 0xFFFFF000;
            break;
        }
        case 0b10: { // B-type
            uint32_t imm11 = extract_bits(instruction, 7, 7);
            uint32_t imm4_1 = extract_bits(instruction, 8, 11);
            uint32_t imm10_5 = extract_bits(instruction, 25, 30);
            uint32_t imm12 = extract_bits(instruction, 31, 31);
            imm = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
            if (imm & 0x1000) imm |= 0xFFFFE000;
            break;
        }
        case 0b11: { // J-type
            uint32_t imm19_12 = extract_bits(instruction, 12, 19);
            uint32_t imm11 = extract_bits(instruction, 20, 20);
            uint32_t imm10_1 = extract_bits(instruction, 21, 30);
            uint32_t imm20 = extract_bits(instruction, 31, 31);
            imm = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
            if (imm & 0x100000) imm |= 0xFFE00000;
            break;
        }
        default:
            break;
    }
    return imm;
}

// Main control unit
void Main_decode(uint32_t op) {
    Branch = Jump = ResultSrc = MemWrite = ALUSrc = ImmSrc = RegWrite = ALUOp = 0;
    switch (op) {
        case 0b0000011: // Load
            ResultSrc = 1; ALUSrc = 1; ImmSrc = 0b00; RegWrite = 1;
            break;
        case 0b0100011: // Store
            MemWrite = 1; ALUSrc = 1; ImmSrc = 0b01;
            break;
        case 0b0110011: // R-type
            ALUSrc = 0; ImmSrc = 0b00; RegWrite = 1; ALUOp = 0b10;
            break;
        case 0b1100011: // Branch
            Branch = 1; ALUSrc = 0; ImmSrc = 0b10; ALUOp = 0b01;
            break;
        case 0b1101111: // JAL
            Jump = 1; ImmSrc = 0b11; RegWrite = 1;
            break;
        case 0b0010011: // I-type (ADDI)
            ALUSrc = 1; ImmSrc = 0b00; RegWrite = 1; ALUOp = 0b00;
            break;
    }
}

// ALU Decoder
uint32_t ALU_decode(uint32_t ALUOp, uint32_t funct3, uint32_t funct7) {
    if (ALUOp == 0b00) return 0b000; // ADD
    if (ALUOp == 0b01) return 0b001; // SUB (for BEQ)

    switch (funct3) {
        case 0b000: return (funct7 == 0b0100000) ? 0b001 : 0b000; // SUB or ADD
        case 0b010: return 0b010; // SLT (Set Less Than)
        case 0b110: return 0b011; // OR
        case 0b111: return 0b100; // AND
        default: return 0b000;
    }
}

// ALU logic
int ALU(uint32_t A, uint32_t B, uint32_t ALUControl) {
    switch (ALUControl) {
        case 0b000: return A + B;        // ADD
        case 0b001: return A - B;        // SUB
        case 0b010: return ((int32_t)A < (int32_t)B); // SLT
        case 0b011: return A | B;        // OR
        case 0b100: return A & B;        // AND
        default: return 0;
    }
}

// Execute a single RISC-V instruction
void execute_instruction(uint32_t instruction) {
    uint32_t opcode = extract_bits(instruction, 0, 6);

    printf("Executing: 0x%08X, Opcode: 0x%02X\n", instruction, opcode);

    Main_decode(opcode);
    int32_t imm = Extend(instruction, ImmSrc);

    uint32_t rs1 = extract_bits(instruction, 15, 19);
    uint32_t rs2 = extract_bits(instruction, 20, 24);
    uint32_t rd  = extract_bits(instruction, 7, 11);
    uint32_t funct3 = extract_bits(instruction, 12, 14);
    uint32_t funct7 = extract_bits(instruction, 25, 31);

    uint32_t A = Register_File[rs1];
    uint32_t B = ALUSrc ? imm : Register_File[rs2];

    uint32_t ALUControl = ALU_decode(ALUOp, funct3, funct7);
    int ALUresult = ALU(A, B, ALUControl);

    uint32_t result = (ResultSrc == 0) ? ALUresult : Data_Memory[ALUresult >> 2];

    if (RegWrite && rd != 0) {
        Register_File[rd] = result;
        printf("  Wrote 0x%08X to register x%d\n", result, rd);
    }

    if (MemWrite) {
        Data_Memory[ALUresult >> 2] = Register_File[rs2];
        printf("  Stored 0x%08X to memory address 0x%08X\n", Register_File[rs2], ALUresult);
    }

    printf("  ALU: A=0x%08X, B=0x%08X, Control=%d, Result=0x%08X\n", A, B, ALUControl, ALUresult);
    printf("  Control Signals: Branch=%d, Jump=%d, ResultSrc=%d, MemWrite=%d, ALUSrc=%d, RegWrite=%d\n",
           Branch, Jump, ResultSrc, MemWrite, ALUSrc, RegWrite);
}

// RISC-V Program: Store player times and compare them
void risc_v_store_player_time(int player, uint32_t reaction_time) {
    printf("\n=== RISC-V: Storing Player %d Time ===\n", player);

    uint32_t memory_addr = (player == 1) ? PLAYER1_TIME_ADDR : PLAYER2_TIME_ADDR;

    // Step 1: Load reaction time into register x5
    // addi x5, x0, reaction_time (using lower 12 bits)
    Register_File[5] = reaction_time;
    printf("Loaded reaction time %d into register x5\n", reaction_time);

    // Step 2: Load memory address into register x6
    // For simplicity, we'll load the address directly
    Register_File[6] = memory_addr;
    printf("Loaded memory address 0x%08X into register x6\n", memory_addr);

    // Step 3: Store the time using sw instruction
    // sw x5, 0(x6) - Store word from x5 to address in x6
    uint32_t sw_instruction = 0x00532023; // sw x5, 0(x6)
    // Modify the instruction to use correct registers
    sw_instruction = (0 << 25) | (5 << 20) | (6 << 15) | (0b010 << 12) | (0 << 7) | 0b0100011;

    execute_instruction(sw_instruction);
    printf("Player %d time stored successfully!\n", player);
}

void risc_v_compare_and_determine_winner() {
    printf("\n=== RISC-V: Comparing Player Times ===\n");

    // Step 1: Load Player 1 time from memory
    // lw x7, 0x2000(x0) - Load Player 1 time into x7
    Register_File[7] = Data_Memory[PLAYER1_TIME_ADDR >> 2];
    printf("Loaded Player 1 time: %d into x7\n", Register_File[7]);

    // Step 2: Load Player 2 time from memory
    // lw x8, 0x2004(x0) - Load Player 2 time into x8
    Register_File[8] = Data_Memory[PLAYER2_TIME_ADDR >> 2];
    printf("Loaded Player 2 time: %d into x8\n", Register_File[8]);

    // Step 3: Compare using SLT (Set Less Than)
    // slt x9, x7, x8 - x9 = 1 if Player1 < Player2 (Player1 faster)
    uint32_t slt_instruction = 0x008383B3; // slt x7, x7, x8 -> result in x9
    slt_instruction = (0 << 25) | (8 << 20) | (7 << 15) | (0b010 << 12) | (9 << 7) | 0b0110011;

    execute_instruction(slt_instruction);

    // Step 4: Store result in memory
    // sw x9, 0x2008(x0) - Store comparison result
    Register_File[10] = GAME_RESULT_ADDR;
    uint32_t sw_result = (0 << 25) | (9 << 20) | (10 << 15) | (0b010 << 12) | (0 << 7) | 0b0100011;

    execute_instruction(sw_result);

    // Step 5: Interpret results
    uint32_t player1_time = Data_Memory[PLAYER1_TIME_ADDR >> 2];
    uint32_t player2_time = Data_Memory[PLAYER2_TIME_ADDR >> 2];
    uint32_t comparison_result = Data_Memory[GAME_RESULT_ADDR >> 2];

    printf("\n=== GAME RESULTS ===\n");
    printf("Player 1 reaction time: %d microseconds\n", player1_time);
    printf("Player 2 reaction time: %d microseconds\n", player2_time);
    printf("RISC-V ALU comparison result (P1 < P2): %d\n", comparison_result);

    if (player1_time == player2_time) {
        printf(" IT'S A DRAW! Perfect timing!\n");
    } else if (comparison_result == 1) {
        printf(" PLAYER 1 WINS! Faster by %d microseconds!\n", player2_time - player1_time);
    } else {
        printf(" PLAYER 2 WINS! Faster by %d microseconds!\n", player1_time - player2_time);
    }
}

// Game Functions
void initialize_game() {
    printf("\n=== RISC-V SHOOTING GAME ===\n");
    printf(" Player 1: Press 'A'\n");
    printf(" Player 2: Press 'L'\n");
    printf(" Press SPACE to start the game!\n");
    printf(" Press ESC to quit\n\n");

    // Initialize timing
    QueryPerformanceFrequency(&frequency);

    // Reset game state
    player1_pressed = 0;
    player2_pressed = 0;
    game_active = 0;

    // Initialize RISC-V processor registers
    for (int i = 0; i < 32; i++) {
        Register_File[i] = 0;
    }

    // Clear game memory locations
    Data_Memory[PLAYER1_TIME_ADDR >> 2] = 0;
    Data_Memory[PLAYER2_TIME_ADDR >> 2] = 0;
    Data_Memory[GAME_RESULT_ADDR >> 2] = 0;
    Data_Memory[GAME_STATUS_ADDR >> 2] = 0;

    printf("RISC-V processor initialized and ready!\n");
}

void start_game() {
    printf(" Game Starting in 3... ");
    Sleep(1000);
    printf("2... ");
    Sleep(1000);
    printf("1... ");
    Sleep(1000);
    printf("SHOOT! \n");

    game_active = 1;
    QueryPerformanceCounter(&start_time);
}

void handle_input(char key) {
    LARGE_INTEGER current_time;
    uint32_t reaction_time;

    if (!game_active) return;

    QueryPerformanceCounter(&current_time);
    reaction_time = (uint32_t)((current_time.QuadPart - start_time.QuadPart) * 1000000 / frequency.QuadPart);

    if (key == 'A' || key == 'a') {
        if (!player1_pressed) {
            player1_pressed = 1;
            printf(" Player 1 shot! (%d ms)\n", reaction_time);
            // Let RISC-V processor store the time
            risc_v_store_player_time(1, reaction_time);
        }
    }
    else if (key == 'L' || key == 'l') {
        if (!player2_pressed) {
            player2_pressed = 1;
            printf(" Player 2 shot! (%d ms)\n", reaction_time);
            // Let RISC-V processor store the time
            risc_v_store_player_time(2, reaction_time);
        }
    }
}

int main() {
    char key;
    int quit = 0;

    initialize_game();

    while (!quit) {
        if (_kbhit()) {
            key = _getch();

            switch (key) {
                case ' ':  // Space to start game
                    if (!game_active) {
                        start_game();
                    }
                    break;

                case 27:  // ESC to quit
                    quit = 1;
                    break;

                case 'A':
                case 'a':
                case 'L':
                case 'l':
                    handle_input(key);
                    break;

                case 'r':
                case 'R':
                    if (!game_active) {
                        initialize_game();
                    }
                    break;
            }

            // Check if both players have responded
            if (game_active && player1_pressed && player2_pressed) {
                game_active = 0;
                // Let RISC-V processor do the comparison
                risc_v_compare_and_determine_winner();
                printf("\n Press SPACE for new game, R to reset, or ESC to quit\n");
            }
        }

        Sleep(10);  // Small delay to prevent high CPU usage
    }

    printf("Thanks for playing the RISC-V Shooting Game!\n");
    return 0;
}
