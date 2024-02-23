//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//

#include <stdint.h>
#include <stdio.h>

#include "cpu6502.h"
#include "motherboard.h"

// Interrupt vectors
#define IRQ_VECTOR 0xFFFE
#define RST_VECTOR 0xFFFC
#define NMI_VECTOR 0xFFFA

#define B_Flag_Mask 0x10

static int cycles;
static uint8_t IRQ_PIN_LEVEL = 1; // IRQ Pin level

// 6502 registers
static uint8_t A;   // Accumulator
static uint8_t X;   // Index register X
static uint8_t Y;   // Index register Y
static uint8_t SP;  // Stack Pointer
static uint16_t PC; // Program counter

// Status register (P)
//   7   6   5   4   3   2   1   0
//   N   V   1  (B)  D   I   Z   C
static uint8_t N_Flag; // Sign/Negative flag
static uint8_t V_Flag; // Overflow flag
static uint8_t D_Flag; // Decimal flag
static uint8_t I_Flag; // Interrupt enable/disable flag
static uint8_t Z_Flag; // Zero flag
static uint8_t C_Flag; // Carry flag

// From https://www.masswerk.at/6502/6502_instruction_set.html
//
// Note: The break flag is not an actual flag implemented in a register, and rather
// appears only, when the status register is pushed onto or pulled from the stack.
// When pushed, it will be 1 when transfered by a BRK or PHP instruction, and
// zero otherwise (i.e., when pushed by a hardware interrupt).
// When pulled into the status register (by PLP or on RTI), it will be ignored.
//
// In other words, the break flag will be inserted, whenever the status register
// is transferred to the stack by software (BRK or PHP), and will be zero, when
// transferred by hardware. Since there is no actual slot for the break flag, it
// will be always ignored, when retrieved (PLP or RTI).
// The break flag is not accessed by the CPU at anytime and there is no internal
// representation. Its purpose is more for patching, to discern an interrupt caused
// by a BRK instruction from a normal interrupt initiated by hardware.



// Splits a byte into status register flags
static void set_P(uint8_t data){
    N_Flag = data & 0x80;
    V_Flag = data & 0x40;
    D_Flag = data & 0x08;
    I_Flag = data & 0x04;
    Z_Flag = data & 0x02;
    C_Flag = data & 0x01;
}



// Assemble the Status Register into a byte
static uint8_t get_P(void){
    uint8_t P = 0x20;
    if (N_Flag) P |= 0x80;
    if (V_Flag) P |= 0x40;
    if (D_Flag) P |= 0x08;
    if (I_Flag) P |= 0x04;
    if (Z_Flag) P |= 0x02;
    if (C_Flag) P |= 0x01;
    return P;
}



// Some common tasks inside CPU



// Compose a 16-bit word from two bytes
static uint16_t word(uint8_t high, uint8_t low){
    return (((uint16_t)high << 8) | (uint16_t)low);
}



// Read a byte
static uint8_t read_byte(uint16_t address){
    return motherboard_readbyte(address);
}



// Write a byte
static void write_byte(uint16_t address, uint8_t data){
    motherboard_writebyte(address, data);
}



// Read a word
static uint16_t read_word(uint16_t address){
    uint8_t low = read_byte(address++);
    uint8_t high = read_byte(address);
    return word(high, low);
}



// Fetch a byte from Program Counter and avance it acordingly
static uint8_t fetch(void){
    return read_byte(PC++);
}



// Fetch a word from Program Counter and avance it acordingly
static uint16_t fetch16(void){
    uint8_t low = fetch();
    uint8_t high = fetch();
    return word(high, low);
}



// Compute N and Z flags. Yes, they
// are always computed together
static void update_NZ(uint8_t data){
    N_Flag = data & 0x80; // Computes Negative/Sign flag
    Z_Flag = (data == 0); // Computes Zero flag
}



// Push a byte into the stack
static void push(uint8_t data){
    write_byte((0x0100 | SP), data);
    SP--;
}



// Pop a byte from the stack
static uint8_t pop(void){
    SP++;
    return read_byte(0x0100 | SP);
}



// Push a word into the stack
static void push16(uint16_t data){
    push((data >> 8) & 0xFF);
    push(data & 0xFF);
}



// Pop a word from the stack
static uint16_t pop16(void){
    uint8_t low = pop();
    uint8_t high = pop();
    return word(high, low);
}



// Update C, Z, N flags acording a comparison
static void compare(uint8_t a, uint8_t b){
    C_Flag = (a >= b);
    Z_Flag = !(a - b);
    N_Flag = (a - b) & 0x80;
}



// Performs an Interrupt Request
static void do_irq(uint16_t vector){
    push16(PC);             // Push program counter
    push(get_P());          // Push Status register
    I_Flag = 0x01;          // Set Interrupt Disable flag
    PC = read_word(vector); // Load PC from vector
    cycles += 7;
}



// Addressing modes
// https://wiki.cdot.senecacollege.ca/wiki/6502_Addressing_Modes

// Absolute
// Data is accessed using 16-bit address specified as a constant.
static uint16_t absolute(void){
    return fetch16();
}



// Absolute, X
// Data is accessed using a 16-bit address specified as a constant,
// to which the value of the X register is added (with carry).
static uint16_t absoluteX(void){
    return fetch16() + X;
}



// Absolute, X
// Add 1 cycle if page boundary is crossed
static uint16_t absoluteX_1(void){
    uint16_t address = fetch16();
    uint16_t e_address = address + X;
    if ((address & 0xFF00) != (e_address & 0xFF00)) cycles++;
    return e_address;
}


// Absolute, Y
// Data is accessed using a 16-bit address specified as a constant,
// to which the value of the Y register is added (with carry).
static uint16_t absoluteY(void){
    return fetch16() + Y;
}



// Absolute, Y
// Add 1 cycle if page boundary is crossed
static uint16_t absoluteY_1(void){
    uint16_t address = fetch16();
    uint16_t e_address = address + Y;
    if ((address & 0xFF00) != (e_address & 0xFF00)) cycles++;
    return e_address;
}



// Inmediate
// Operand is pointed by PC after fetching opcode
static uint16_t inmediate(void){
    return PC++;
}



// Indirect
// Data is accessed using a pointer. The 16-bit address of the pointer
// is given in the two bytes following the opcode.
static uint16_t indirect(void){
    return read_word(fetch16());
}



// X, Indirect
// An 8-bit zero-page address and the X register are added, without carry
// (if the addition overflows, the address wraps around within page 0)
// The resulting address is used as a pointer to the data being accessed.
static uint16_t indirectX(void){
    uint8_t addr = fetch() + X;
    uint8_t low = read_byte(addr++);
    uint8_t high = read_byte(addr);
    return word(high, low);
}



// Indirect, Y
// An 8-bit address identifies a pointer. The value of the Y register is
// added to the address contained in the pointer.
static uint16_t indirectY(void){
    uint8_t addr = fetch();
    uint8_t low = read_byte(addr++);
    uint8_t high = read_byte(addr);
    return word(high, low) + Y;
}



// Indirect, Y
// Add 1 cycle if page boundary is crossed
static uint16_t indirectY_1(void){
    uint8_t addr = fetch();
    uint8_t low = read_byte(addr++);
    uint8_t high = read_byte(addr);
    uint16_t e_address = word(high, low) + Y;
    if ((e_address >> 8) != high) cycles++;
    return e_address;
}



// Zero page
// An 8-bit address is provided within the zero page. This is
// like an absolute address, but since the argument is only one
// byte, the CPU does not have to spend an additional cycle to
// fetch high byte.
static uint16_t zeropage(void){
    return (uint16_t)fetch();
}



// Zero page, X
// An 8-bit address is provided, to which the X register is added 
// without carry - if the addition overflows, the address wraps
// around within the zero page).
static uint16_t zeropageX(void){
    return ((fetch() + X) & 0x00FF);
}



// Zero page, Y
// An 8-bit address is provided, to which the Y register is added 
// without carry - if the addition overflows, the address wraps
// around within the zero page).
static uint16_t zeropageY(void){
    return ((fetch() + Y) & 0x00FF);
}



// Instruction cores
// https://www.masswerk.at/6502/6502_instruction_set.html
// ADC Add Memory to Accumulator with Carry
static void ADC(uint16_t address){
    uint8_t op1, op2;
    if(D_Flag){
        // Decimal mode
        op1 = A;
        op2 = read_byte(address);
        uint16_t dec_l = (op1 & 0x0F) + (op2 & 0x0F) + (C_Flag != 0x00);
        uint16_t dec_h = (op1 & 0xF0) + (op2 & 0xF0);
        Z_Flag = !((dec_l + dec_h) & 0xFF);
        if (dec_l > 0x09){
            dec_h += 0x10;
            dec_l += 0x06;
        }
        N_Flag = dec_h & 0x80;
        V_Flag = ~(op1 ^ op2) & (op1 ^ dec_h) & 0x80;
        if (dec_h > 0x90) dec_h += 0x60;
        C_Flag = dec_h >> 8;
        A = (dec_l & 0x000F) | (dec_h & 0x00F0);        
    } else {
        // Binary mode
        uint16_t aux;
        op1 = A;
        op2 = read_byte(address);
        aux = (uint16_t)op1 + (uint16_t) op2 + (uint16_t)(C_Flag != 0x00);
        A = aux & 0x00FF;
        update_NZ(A);
        C_Flag = aux >> 8;
        V_Flag = ((op1 & 0x80) == (op2 & 0x80)) & ((op1 & 0x80) != (A & 0x80));
    }
}



//AND: AND Memory with Accumulator
static void AND(uint16_t address){
    A &= read_byte(address);
    update_NZ(A);
}



// ASL: Shift Left One Bit 
static void ASL(uint16_t address){
    uint8_t data = read_byte(address);
    C_Flag = data & 0x80;
    data <<= 1;
    update_NZ(data);
    write_byte(address, data);
}



// ASL: Shift Left Accumulator One Bit
static void ASL_ACC(void){
    C_Flag = A & 0x80;
    A <<= 1;
    update_NZ(A);
}



// BIT: Test Bits in Memory with Accumulator (absolute)
// bits 7 and 6 of operand are transfered to bit 7 and 6 of
// SR (N,V). the zero-flag is set to the result of operand
// AND accumulator.
static void BIT(uint16_t address){
    uint8_t data = read_byte(address);
    N_Flag = data & 0x80;
    V_Flag = data & 0x40;
    Z_Flag = !(data & A);
}



// BRK: Force break
static void BRK(void){
    PC++;                         // Skip break mark byte
    push16(PC);                   // Push program counter
    push(get_P() | B_Flag_Mask);  // Set B flag
    I_Flag = 0x01;                // Set Interrupt flag
    PC = read_word(IRQ_VECTOR);      // Load PC from IRQ vector
}



// BXX: Branch on condition
static void BXX(uint8_t condition){
    uint16_t e_address;
    uint8_t reljmp = read_byte(PC++);
    if(condition){
        cycles++; // Branch taken
        e_address = PC + (uint16_t)((int8_t)reljmp); // Ugly!
        if ((e_address & 0xFF00) != (PC & 0xFF00)) cycles++; // Branch to different page
        PC = e_address;
    }
}



// CLC: Clear carry
static void CLC(void){
    C_Flag = 0x00;
}



// CLD: Clear decimal
static void CLD(void){
    D_Flag = 0x00;
}



// CLI:Clear interrupt disable
static void CLI(void){
    I_Flag = 0x00;
}



// CLV: Clear overflow
static void CLV(void){
    V_Flag = 0x00;
}



// CMP: Compare Memory with Accumulator
static void CMP(uint16_t address){
    compare(A, read_byte(address));
}



// CPX: Compare Memory and Index X
static void CPX(uint16_t address){
    compare(X, read_byte(address));
}



// CPY: Compare Memory and Index Y
static void CPY(uint16_t address){
    compare(Y, read_byte(address));
}



// DEC: Decrement Memory by One
static void DEC(uint16_t address){
    uint8_t data = read_byte(address);
    data--;
    update_NZ(data);
    write_byte(address, data);
}



// DEX: Decrement X
static void DEX(void){
    X--;
    update_NZ(X);    
}



// DEY: Decrement Y
static void DEY(void){
    Y--;
    update_NZ(Y);
}



// EOR: Exclusive-OR Memory with Accumulator
static void EOR(uint16_t address){
    A ^= read_byte(address);
    update_NZ(A);
}



// INC: Increment Memory by One
static void INC(uint16_t address){
    uint8_t data = read_byte(address);
    data++;
    update_NZ(data);
    write_byte(address, data);
}



// INX: Increment X
static void INX(void){
    X++;
    update_NZ(X);
}



// INY: Increment Y
static void INY(void){
    Y++;
    update_NZ(Y);
}



// JMP: Jump to new location
static void JMP(uint16_t address){
    PC = address;
}



// JSR: Jump to subroutine
static void JSR(uint16_t address){
    // https://retrocomputing.stackexchange.com/questions/19543/
    // why-does-the-6502-jsr-instruction-only-increment-the-re
    //turn-address-by-2-bytes
    push16(PC - 1);
    PC = address;    
}



// LDA: Load accumulator
static void LDA(uint16_t address){
    A = read_byte(address);
    update_NZ(A);
}



// LDX: Load X
static void LDX(uint16_t address){
    X = read_byte(address);
    update_NZ(X);
}



// LDY: Load Y
static void LDY(uint16_t address){
    Y = read_byte(address);
    update_NZ(Y);
}



// LSR: Shift One Bit Right
static void LSR(uint16_t address){
    uint8_t data = read_byte(address);
    C_Flag = data & 0x01;
    data >>= 1;
    update_NZ(data);
    write_byte(address, data);
}



// LSR: Shift Accumulator One Bit Right
static void LSR_ACC(void){
    C_Flag = A & 0x01;
    A >>= 1;
    update_NZ(A);
}



// ORA: OR Memory with Accumulator
static void ORA(uint16_t address){
    A |= read_byte(address);
    update_NZ(A);
}



// PHA: Push Accumulator
static void PHA(void){
    push(A);
}



// PHP: Push Processor Status on Stack
static void PHP(void){
    push(get_P() | B_Flag_Mask); // B Flag should be active
}



// PLA: Pull Accumulator
static void PLA(void){
    A = pop();
    update_NZ(A);
}



// PLP: Pull Processor Status from Stack
static void PLP(void){
    set_P(pop());
}



// ROL: Rotate Memory One Bit Left
static void ROL(uint16_t address){
    uint8_t data = read_byte(address);
    uint8_t oldC_Flag = C_Flag;
    C_Flag = data & 0x80;
    data <<= 1;
    if (oldC_Flag) data |= 0x01;
    update_NZ(data);
    write_byte(address, data);
}



// ROL: Rotate Accumulator One Bit Left
static void ROL_ACC(void){
    uint8_t oldC_Flag = C_Flag;
    C_Flag = A & 0x80;
    A <<= 1;
    if (oldC_Flag) A |= 0x01;
    update_NZ(A);    
}



// ROR: Rotate One Bit Right
static void ROR(uint16_t address){
    uint8_t data = read_byte(address);
    uint8_t oldC_Flag = C_Flag;
    C_Flag = data & 0x01;
    data >>= 1;
    if (oldC_Flag) data |= 0x80;
    update_NZ(data);
    write_byte(address, data);
}



static void ROR_ACC(void){
    uint8_t oldC_Flag = C_Flag;
    C_Flag = A & 0x01;
    A >>= 1;
    if (oldC_Flag) A |= 0x80;
    update_NZ(A);    
}



// RTI: Return from interrupt
static void RTI(void){
    set_P(pop());   // Pull status register
    PC = pop16();   // Pull program counter
}



// RTS: Return from subroutine
static void RTS(void){
    // https://retrocomputing.stackexchange.com/questions/19543/
    // why-does-the-6502-jsr-instruction-only-increment-the-re
    // turn-address-by-2-bytes
    PC = pop16() + 1;    
}



// SBC: Subtract with carry
static void SBC(uint16_t address){   
    uint8_t op1, op2;
    uint16_t aux;
    if(D_Flag){
        // Decimal mode
        op1 = A;
        op2 = read_byte(address);
        aux = op1 - op2 - (C_Flag == 0x00);
        uint16_t dec_l = (op1 & 0x0F) - (op2 & 0x0F) - (C_Flag == 0x00);
        uint16_t dec_h = (op1 & 0xF0) - (op2 & 0xF0);
        if (dec_l & 0x10){
            dec_l -= 6;
            dec_h--;
        }
        V_Flag = (op1 ^ op2) & (op1 ^ aux) & 0x80;
        C_Flag = !(aux & 0xFF00);
        Z_Flag = !(aux & 0x00FF);
        N_Flag = aux & 0x0080;
        if (dec_h & 0x0100) dec_h -= 0x60;
        A = (dec_l & 0x0F) | (dec_h & 0xF0);        
    } else {
        // Binary mode
        // Identical to ADC but with only one difference
        op1 = A;
        op2 = ~read_byte(address); // <--- This one!
        aux = (uint16_t)op1 + (uint16_t) op2 + (uint16_t)(C_Flag != 0);
        A = aux & 0x00FF;
        update_NZ(A);
        C_Flag = aux >> 8;
        V_Flag = ((op1 & 0x80) == (op2 & 0x80)) & ((op1 & 0x80) != (A & 0x80));
    }    
}



// SEC: Set carry
static void SEC(void){
    C_Flag = 0x01;
}



//SED: Set decimal
static void SED(void){
    D_Flag = 0x01;
}



//SEI: Set interrupt disable
static void SEI(void){
    I_Flag = 0x01;
}



//STA: Store accumulator
static void STA(uint16_t address){
    write_byte(address, A);
}



// STX: Store X
static void STX(uint16_t address){
    write_byte(address, X);
}



// STY: Store Y
static void STY(uint16_t address){
    write_byte(address, Y);
}



// TAX: Transfer accumulator to X
static void TAX(void){
    X = A;
    update_NZ(X);
}



// TAY: Transfer accumulator to Y
static void TAY(void){
    Y = A;
    update_NZ(Y);
}



// TSX: Transfer stack pointer to X
static void TSX(void){
    X = SP;
    update_NZ(X);    
}



// TXA: Transfer X to accumulator
static void TXA(void){
    A = X;
    update_NZ(A);    
}



// TXS: Transfer X to stack pointer
static void TXS(void){
    SP = X;    
}



// TYA: Transfer Y to accumulator
static void TYA(void){
    A = Y;
    update_NZ(A);
}



static void illegal_opcode(uint8_t opcode){
    fprintf(stderr, "\nError: illegal opcode 0x%02X at 0x%04X. Resseting CPU\n", opcode, PC - 1);
    cpu_reset();
}



// Fire IRQ
void cpu_irq(uint8_t level){
    // IRQ in 6502 is level triggered (at level 0) so
    // we save the pin level for later use
    IRQ_PIN_LEVEL = level;
}



// Fire NMI
void cpu_nmi(void){
    // NMI in 6502 is triggered by the falling edge of the NMI
    // pin, so we can do the interrupt whenever we call cpu_nmi()
    do_irq(NMI_VECTOR);
}



// Resets CPU
void cpu_reset(void){
    A = 0x00;
    X = 0x00;
    Y = 0x00;
    SP = 0xFD;
    set_P(0x36); // nv10dIZc
    PC = read_word(RST_VECTOR);
}



// Execute one instruction
int cpu_execute(void){
    cycles = 0;
    
    // Whenever IRQ_PIN_LEVEL is low and I flag is zero
    // an interrupt must be made
    if (!(IRQ_PIN_LEVEL || I_Flag)){
        do_irq(IRQ_VECTOR);
    }
 
    // Now, just interpret opcodes
    uint8_t opcode = read_byte(PC++);
    
    // Here we go... the giant switch-case. Let's hope the
    // compiler can optimize it into a jump table ;-)
    // SPOILER: It does!
    switch(opcode){
        
        case 0x00: // BRK: Force Break 
            BRK();
            cycles += 7;
            break;
        
        case 0x01: // ORA: OR Memory with Accumulator (indirect,X)
            ORA(indirectX());
            cycles += 6;
            break;
            
        case 0x02:
            illegal_opcode(opcode);
            break;

        case 0x03:
            illegal_opcode(opcode);
            break;
            
        case 0x04:
            illegal_opcode(opcode);
            break;
            
        case 0x05: //ORA: OR Memory with Accumulator (zeropage)
            ORA(zeropage());
            cycles += 3;
            break;
            
        case 0x06: //ASL: Shift Left One Bit (zeropage)
            ASL(zeropage());
            cycles += 5;
            break;
            
        case 0x07:
            illegal_opcode(opcode);
            break;
            
        case 0x08: // PHP: Push Processor Status on Stack
            PHP();
            cycles += 3;
            break;
            
        case 0x09: //ORA: OR Memory with Accumulator (inmediate)
            ORA(inmediate());
            cycles += 2;
            break;
            
        case 0x0A: // ASL: Shift Left One Bit (Accumulator)
            ASL_ACC();
            cycles += 2;
            break;
            
        case 0x0B:
            illegal_opcode(opcode);
            break;

        case 0x0C:
            illegal_opcode(opcode);
            break;            
            
        case 0x0D: //ORA: OR Memory with Accumulator (absolute)
            ORA(absolute());
            cycles += 4;
            break;
            
        case 0x0E: //ASL: Shift Left One Bit (absolute)
            ASL(absolute());
            cycles += 6;
            break;

        case 0x0F:
            illegal_opcode(opcode);
            break;            
            
        case 0x10: // BPL: Branch on Result Plus
            BXX(!N_Flag);
            cycles += 2;
            break;
        
        case 0x11: //ORA: OR Memory with Accumulator ((indirect),Y)
            ORA(indirectY_1());
            cycles += 5;
            break;
            
        case 0x12:
            illegal_opcode(opcode);
            break;

        case 0x13:
            illegal_opcode(opcode);
            break;
            
        case 0x14:
            illegal_opcode(opcode);
            break;
            
        case 0x15: //ORA: OR Memory with Accumulator (zeropageX)
            ORA(zeropageX());
            cycles += 4;
            break;

        case 0x16: //ASL: Shift Left One Bit (zeropageX)
            ASL(zeropageX());
            cycles += 6;
            break;

        case 0x17:
            illegal_opcode(opcode);
            break;
            
        case 0x18: // CLC: Clear Carry
            CLC();
            cycles += 2;
            break;
            
        case 0x19: //ORA: OR Memory with Accumulator (absoluteY)
            ORA(absoluteY_1());
            cycles += 4;
            break;

        case 0x1A:
            illegal_opcode(opcode);
            break;
            
        case 0x1B:
            illegal_opcode(opcode);
            break;

        case 0x1C:
            illegal_opcode(opcode);
            break;            
            
        case 0x1D: //ORA: OR Memory with Accumulator (absoluteX)
            ORA(absoluteX_1());
            cycles += 4;
            break;
        
        case 0x1E: //ASL: Shift Left One Bit (absoluteX)
            ASL(absoluteX());
            cycles += 7;
            break;
            
        case 0x1F:
            illegal_opcode(opcode);
            break;            
            
        case 0x20: // JSR: Jump Sub Routine
            JSR(absolute());
            cycles += 6;
            break;

        case 0x21: // AND: AND Memory with Accumulator (indirectX)
            AND(indirectX());
            cycles += 6;
            break;

        case 0x22:
            illegal_opcode(opcode);
            break;  

        case 0x23:
            illegal_opcode(opcode);
            break;  

        case 0x24: //BIT: Test Bits in Memory with Accumulator (zeropage)
            BIT(zeropage());
            cycles += 3;
            break;

        case 0x25: // AND: AND Memory with Accumulator (zeropage)
            AND(zeropage());
            cycles += 3;
            break;
            
        case 0x26: // ROL (zeropage)
            ROL(zeropage());
            cycles += 5;
            break;

        case 0x27:
            illegal_opcode(opcode);
            break;
            
        case 0x28: // PLP: Pull Processor Status from Stack
            PLP();
            cycles += 4;
            break;

        case 0x29: // AND: AND Memory with Accumulator (inmediate)
            AND(inmediate());
            cycles += 2;
            break;
            
        case 0x2A: // ROL: Rotate One Bit Left (accumulator)
            ROL_ACC();
            cycles += 2;
            break;
            
        case 0x2B:
            illegal_opcode(opcode);
            break;            
            
        case 0x2C: // BIT: Test Bits in Memory with Accumulator (absolute)
            BIT(absolute());
            cycles += 4;
            break;
            
        case 0x2D: // AND: AND Memory with Accumulator (absolute)
            AND(absolute());
            cycles += 4;
            break;
            
        case 0x2E: // ROL: Rotate One Bit Left (absolute)
            ROL(absolute());
            cycles += 6;
            break;

        case 0x2F:
            illegal_opcode(opcode);
            break;             
            
        case 0x30: // BMI: Branch on Result Minus
            BXX(N_Flag);
            cycles += 2;
            break;            

        case 0x31: // AND: AND Memory with Accumulator (indirectY)
            AND(indirectY_1());
            cycles += 5;
            break;
            
        case 0x32:
            illegal_opcode(opcode);
            break;
            
        case 0x33:
            illegal_opcode(opcode);
            break;
            
        case 0x34:
            illegal_opcode(opcode);
            break;
            
        case 0x35: // AND: AND Memory with Accumulator (zeropageX)
            AND(zeropageX());
            cycles += 4;
            break;
            
        case 0x36: // ROL: Rotate One Bit Left (zeropageX)
            ROL(zeropageX());
            cycles += 6;
            break;
            
        case 0x37:
            illegal_opcode(opcode);
            break;
            
        case 0x38: // SEC: Set Carry Flag
            SEC();
            cycles += 2;
            break;
            
        case 0x39: // AND: AND Memory with Accumulator (absoluteY)
            AND(absoluteY_1());
            cycles += 4;
            break;
            
        case 0x3A:
            illegal_opcode(opcode);
            break;
            
        case 0x3B:
            illegal_opcode(opcode);
            break;
            
        case 0x3C:
            illegal_opcode(opcode);
            break;
            
        case 0x3D: // AND: AND Memory with Accumulator (absoluteX)
            AND(absoluteX_1());
            cycles += 4;
            break;
            
        case 0x3E: // ROL: Rotate One Bit Left (absoluteX)
            ROL(absoluteX());
            cycles += 7;
            break;
            
        case 0x3F:
            illegal_opcode(opcode);
            break; 

        case 0x40: // RTI: Return from interruption
            RTI();
            cycles += 6;
            break;

        case 0x41: // EOR: Exclusive OR memory with accumulator (indirectX)
            EOR(indirectX());
            cycles += 6;
            break;
            
        case 0x42:
            illegal_opcode(opcode);
            break; 

        case 0x43:
            illegal_opcode(opcode);
            break; 

        case 0x44:
            illegal_opcode(opcode);
            break; 

        case 0x45: // EOR: Exclusive OR memory with accumulator (zeropage)
            EOR(zeropage());
            cycles += 3;
            break;
            
        case 0x46: // LSR: Shift one bit right (zeropage)
            LSR(zeropage());
            cycles += 5;
            break;
            
        case 0x47:
            illegal_opcode(opcode);
            break; 
            
        case 0x48: // PHA: Push Accumulator on Stack
            PHA();
            cycles += 3;
            break;
            
        case 0x49: // EOR: Exclusive OR memory with accumulator (inmediate)
            EOR(inmediate());
            cycles += 2;
            break;
            
        case 0x4A: // LSR: Shift one bit right (Accumulator)
            LSR_ACC();
            cycles += 2;
            break;
            
        case 0x4B:
            illegal_opcode(opcode);
            break; 

        case 0x4C: //JMP: Jump to New Location (absolute)
            JMP(absolute());
            cycles += 3;
            break;
            
        case 0x4D: // EOR: Exclusive OR memory with accumulator (absolute)
            EOR(absolute());
            cycles += 4;
            break;
            
        case 0x4E: // LSR: Shift one bit right (absolute)
            LSR(absolute());
            cycles += 6;
            break;
            
        case 0x4F:
            illegal_opcode(opcode);
            break; 
            
        case 0x50: // BVC: Branch on Overflow Clear
            BXX(!V_Flag);
            cycles += 2;
            break; 
            
        case 0x51: // EOR: Exclusive OR memory with accumulator (indirectY)
            EOR(indirectY_1());
            cycles += 5;
            break;
            
        case 0x52:
            illegal_opcode(opcode);
            break; 

        case 0x53:
            illegal_opcode(opcode);
            break; 

        case 0x54:
            illegal_opcode(opcode);
            break; 

        case 0x55: // EOR: Exclusive OR memory with accumulator (zeropageX)
            EOR(zeropageX());
            cycles += 4;
            break;
            
        case 0x56: // LSR: Shift one bit right (zeropageX)
            LSR(zeropageX());
            cycles += 6;
            break;

        case 0x57:
            illegal_opcode(opcode);
            break; 

        case 0x58: // CLI: Clears Interrupt flag
            CLI();
            cycles += 2;
            break;
            
        case 0x59: // EOR: Exclusive OR memory with accumulator (absoluteY)
            EOR(absoluteY_1());
            cycles += 4;
            break;
            
        case 0x5A:
            illegal_opcode(opcode);
            break; 

        case 0x5B:
            illegal_opcode(opcode);
            break; 

        case 0x5C:
            illegal_opcode(opcode);
            break; 

        case 0x5D: // EOR: Exclusive OR memory with accumulator (absoluteX)
            EOR(absoluteX_1());
            cycles += 4;
            break;
            
        case 0x5E: // LSR: Shift one bit right (absoluteX)
            LSR(absoluteX());
            cycles += 7;
            break;

        case 0x5F:
            illegal_opcode(opcode);
            break;             

        case 0x60: // RTS: Return from Subroutine
            RTS();
            cycles += 6;
            break;
            
        case 0x61: // ADC Add Memory to Accumulator with Carry (indirectX)
            ADC(indirectX());
            cycles += 6;
            break;

        case 0x62:
            illegal_opcode(opcode);
            break;             

        case 0x63:
            illegal_opcode(opcode);
            break;             

        case 0x64:
            illegal_opcode(opcode);
            break;             

        case 0x65: // ADC Add Memory to Accumulator with Carry (zeropage)
            ADC(zeropage());
            cycles += 3;
            break;

        case 0x66: // ROR: Rotate One Bit Right (zeropage)
            ROR(zeropage());
            cycles += 5;
            break;
            
        case 0x67:
            illegal_opcode(opcode);
            break;             
            
        case 0x68: // PLA: Pull Accumulator from Stack
            PLA();
            cycles += 4;
            break;
            
        case 0x69: // ADC Add Memory to Accumulator with Carry (inmediate)
            ADC(inmediate());
            cycles += 2;
            break;
            
        case 0x6A: // ROR: Rotate One Bit Right (accumulator)
            ROR_ACC();
            cycles += 2;
            break;
            
        case 0x6B:
            illegal_opcode(opcode);
            break;             

        case 0x6C: //JMP: Jump to New Location (indirect)
            JMP(indirect());
            cycles += 5;
            break;

        case 0x6D: // ADC Add Memory to Accumulator with Carry (absolute)
            ADC(absolute());
            cycles += 4;
            break;

        case 0x6E: // ROR: Rotate One Bit Right (absolute)
            ROR(absolute());
            cycles += 6;
            break;

        case 0x6F:
            illegal_opcode(opcode);
            break;
            
        case 0x70: // BVC: Branch on Overflow Set
            BXX(V_Flag);
            cycles += 2;
            break;
            
        case 0x71: // ADC Add Memory to Accumulator with Carry (indirectY)
            ADC(indirectY_1());
            cycles += 5;
            break;

        case 0x72:
            illegal_opcode(opcode);
            break;

        case 0x73:
            illegal_opcode(opcode);
            break;

        case 0x74:
            illegal_opcode(opcode);
            break;

        case 0x75: // ADC Add Memory to Accumulator with Carry (zeropageX)
            ADC(zeropageX());
            cycles += 4;
            break;

        case 0x76: // ROR: Rotate One Bit Right (zeropageX)
            ROR(zeropageX());
            cycles += 6;
            break;

        case 0x77:
            illegal_opcode(opcode);
            break;

        case 0x78: //SEI: Set Interrupt Disable Status
            SEI();
            cycles += 2;
            break;
            
        case 0x79: // ADC Add Memory to Accumulator with Carry (absoluteY)
            ADC(absoluteY_1());
            cycles += 4;
            break;

        case 0x7A:
            illegal_opcode(opcode);
            break;

        case 0x7B:
            illegal_opcode(opcode);
            break;

        case 0x7C:
            illegal_opcode(opcode);
            break;

        case 0x7D: // ADC Add Memory to Accumulator with Carry (absoluteX)
            ADC(absoluteX_1());
            cycles += 4;
            break;

        case 0x7E: // ROR: Rotate One Bit Right (absoluteX)
            ROR(absoluteX());
            cycles += 7;
            break;
            
        case 0x7F:
            illegal_opcode(opcode);
            break;             

        case 0x80:
            illegal_opcode(opcode);
            break;

        case 0x81: //STA: Store Accumulator in Memory (indirectX)
            STA(indirectX());
            cycles += 6;
            break;

        case 0x82:
            illegal_opcode(opcode);
            break;

        case 0x83:
            illegal_opcode(opcode);
            break;

        case 0x84: // STY: Store Index Y in Memory (zeropage)
            STY(zeropage());
            cycles += 3;
            break;
            
        case 0x85: //STA: Store Accumulator in Memory (zeropage)
            STA(zeropage());
            cycles += 3;
            break;
            
        case 0x86: // STX: Store Index X in Memory (zeropage)
            STX(zeropage());
            cycles += 3;
            break;
            
        case 0x87:
            illegal_opcode(opcode);
            break;
            
        case 0x88: // DEY: Decrement Index Y by One
            DEY();
            cycles += 2;
            break;
            
        case 0x89:
            illegal_opcode(opcode);
            break;

        case 0x8A: // TXA: Transfer Index X to Accumulator
            TXA();
            cycles += 2;
            break;
            
        case 0x8B:
            illegal_opcode(opcode);
            break;
       
        case 0x8C: // STY: Sore Index Y in Memory (absolute)
            STY(absolute());
            cycles += 4;
            break;
            
        case 0x8D: //STA: Store Accumulator in Memory (absolute)
            STA(absolute());
            cycles += 4;
            break;
         
        case 0x8E: // STX: Store Index X in Memory (absolute)
            STX(absolute());
            cycles += 4;
            break;           
        
        case 0x8F:
            illegal_opcode(opcode);
            break; 
            
        case 0x90: // BCC: Branch on Carry Clear
            BXX(!C_Flag);
            cycles += 2;
            break;
            
        case 0x91: //STA: Store Accumulator in Memory (indirectY)
            STA(indirectY());
            cycles += 6;
            break;

        case 0x92:
            illegal_opcode(opcode);
            break; 

        case 0x93:
            illegal_opcode(opcode);
            break; 

        case 0x94: // STY: Sore Index Y in Memory (zeropageX)
            STY(zeropageX());
            cycles += 4;
            break;

        case 0x95: // STA: Store Accumulator in Memory (zeropageX)
            STA(zeropageX());
            cycles += 4;
            break;

        case 0x96: // STX: Store Index X in Memory (zeropageY)
            STX(zeropageY());
            cycles += 4;
            break;

        case 0x97:
            illegal_opcode(opcode);
            break; 

        case 0x98: // TYA: Transfer Index Y to Accumulator
            TYA();
            cycles += 2;
            break;
            
        case 0x99: // STA: Store Accumulator in Memory (absoluteY)
            STA(absoluteY());
            cycles += 5;
            break;
            
        case 0x9A: // TXS: Transfer Index X to Stack Register
            TXS();
            cycles += 2;
            break;
            
        case 0x9B:
            illegal_opcode(opcode);
            break; 

        case 0x9C:
            illegal_opcode(opcode);
            break; 

        case 0x9D: //STA: Store Accumulator in Memory (absoluteX)
            STA(absoluteX());
            cycles += 5;
            break;

        case 0x9E:
            illegal_opcode(opcode);
            break; 

        case 0x9F:
            illegal_opcode(opcode);
            break; 
            
        case 0xA0: // LDY: Load Index Y with Memory (inmediate)
            LDY(inmediate());
            cycles += 2;
            break;
            
        case 0xA1: // LDA: Load Accumulator with Memory(indirectX)
            LDA(indirectX());
            cycles += 6;
            break;

        case 0xA2: // LDX: Load Index X with Memory (inmediate)
            LDX(inmediate());
            cycles += 2;
            break;
            
        case 0xA3:
            illegal_opcode(opcode);
            break; 

        case 0xA4: // LDY: Load Index Y with Memory (zeropage)
            LDY(zeropage());
            cycles += 3;
            break;

        case 0xA5: // LDA: Load Accumulator with Memory(zeropage)
            LDA(zeropage());
            cycles += 3;
            break;

        case 0xA6: // LDX: Load Index X with Memory (zeropage)
            LDX(zeropage());
            cycles += 3;
            break;

        case 0xA7:
            illegal_opcode(opcode);
            break; 

        case 0xA8: // TAY: Transfer Accumulator to Index Y
            TAY();
            cycles += 2;
            break;
            
        case 0xA9: // LDA: Load Accumulator with Memory(inmediate)
            LDA(inmediate());
            cycles += 2;
            break;
            
        case 0xAA: // TAX: Transfer Accumulator to Index X
            TAX();
            cycles += 2;
            break;
            
        case 0xAB:
            illegal_opcode(opcode);
            break; 

        case 0xAC: // LDY: Load Index Y with Memory (absolute)
            LDY(absolute());
            cycles += 4;
            break;

        case 0xAD: // LDA: Load Accumulator with Memory(absolute)
            LDA(absolute());
            cycles += 4;
            break;

        case 0xAE: // LDX: Load Index X with Memory (absolute)
            LDX(absolute());
            cycles += 4;
            break;

        case 0xAF:
            illegal_opcode(opcode);
            break; 

        case 0xB0: // BCS: Branch on Carry Set
            BXX(C_Flag);
            cycles += 2;
            break;

        case 0xB1:// LDA: Load Accumulator with Memory(indirectY)
            LDA(indirectY_1());
            cycles += 5;
            break;

        case 0xB2:
            illegal_opcode(opcode);
            break; 
 
        case 0xB3:
            illegal_opcode(opcode);
            break; 

        case 0xB4: // LDY: Load Index Y with Memory (zeropageX)
            LDY(zeropageX());
            cycles += 4;
            break;

        case 0xB5: // LDA: Load Accumulator with Memory(zeropageX)
            LDA(zeropageX());
            cycles += 4;
            break;

        case 0xB6: // LDX: Load Index X with Memory (zeropageY)
            LDX(zeropageY());
            cycles += 4;
            break;

        case 0xB7:
            illegal_opcode(opcode);
            break; 

        case 0xB8: // CLV: Clear Overflow Flag
            CLV();
            cycles += 2;
            break;
            
        case 0xB9: // LDA: Load Accumulator with Memory(absoluteY)
            LDA(absoluteY_1());
            cycles += 4;
            break;

        case 0xBA: // TSX: Transfer Stack Pointer to Index X
            TSX();
            cycles += 2;
            break;
            
        case 0xBB:
            illegal_opcode(opcode);
            break; 

        case 0xBC: // LDY: Load Index Y with Memory (absoluteX)
            LDY(absoluteX_1());
            cycles += 4;
            break;

        case 0xBD: // LDA: Load Accumulator with Memory(absoluteX)
            LDA(absoluteX_1());
            cycles += 4;
            break;

        case 0xBE: // LDX: Load Index X with Memory (absoluteY)
            LDX(absoluteY_1());
            cycles += 4;
            break;

        case 0xBF:
            illegal_opcode(opcode);
            break; 
            
        case 0xC0: // CPY: Compare Memory and Index Y (inmediate)
            CPY(inmediate());
            cycles += 2;
            break;

        case 0xC1: // CMP: Compare Memory with Accumulator (indirectX)
            CMP(indirectX());
            cycles += 6;
            break;

        case 0xC2:
            illegal_opcode(opcode);
            break; 

        case 0xC3:
            illegal_opcode(opcode);
            break; 

        case 0xC4: // CPY: Compare Memory and Index Y (zeropage)
            CPY(zeropage());
            cycles += 3;
            break;

        case 0xC5: // CMP: Compare Memory with Accumulator (zeropage)
            CMP(zeropage());
            cycles += 3;
            break;

        case 0xC6: // DEC: Decrement Memory by One (zeropage)
            DEC(zeropage());
            cycles += 5;
            break;
            
        case 0xC7:
            illegal_opcode(opcode);
            break; 

        case 0xC8: // INY: Increment Index Y by One
            INY();
            cycles += 2;
            break;
            
        case 0xC9: // CMP: Compare Memory with Accumulator (inmediate)
            CMP(inmediate());
            cycles += 2;
            break;
            
        case 0xCA: //DEX: Decrement Index X by One
            DEX();
            cycles += 2;
            break;
            
        case 0xCB:
            illegal_opcode(opcode);
            break; 

        case 0xCC: // CPY: Compare Memory and Index Y (absolute)
            CPY(absolute());
            cycles += 4;
            break;

        case 0xCD: // CMP: Compare Memory with Accumulator (absolute)
            CMP(absolute());
            cycles += 4;
            break;

        case 0xCE: // DEC: Decrement Memory by One (absolute)
            DEC(absolute());
            cycles += 6;
            break;

        case 0xCF:
            illegal_opcode(opcode);
            break; 
            
        case 0xD0: // BNE: Branch on Result not Zero
            BXX(!Z_Flag);
            cycles += 2;
            break;            
            
        case 0xD1: // CMP: Compare Memory with Accumulator (indirectY)
            CMP(indirectY_1());
            cycles += 5;
            break;

        case 0xD2:
            illegal_opcode(opcode);
            break; 

        case 0xD3:
            illegal_opcode(opcode);
            break; 

        case 0xD4:
            illegal_opcode(opcode);
            break; 

        case 0xD5: // CMP: Compare Memory with Accumulator (zeropageX)
            CMP(zeropageX());
            cycles += 4;
            break;

        case 0xD6: // DEC: Decrement Memory by One (zeropageX)
            DEC(zeropageX());
            cycles += 6;
            break;

        case 0xD7:
            illegal_opcode(opcode);
            break; 

        case 0xD8: // CLD: Clears Decimal Flag bit
            CLD();
            cycles += 2;
            break;
            
        case 0xD9: // CMP: Compare Memory with Accumulator (absoluteY)
            CMP(absoluteY_1());
            cycles += 4;
            break;

        case 0xDA:
            illegal_opcode(opcode);
            break; 

        case 0xDB:
            illegal_opcode(opcode);
            break; 

        case 0xDC:
            illegal_opcode(opcode);
            break; 

        case 0xDD: // CMP: Compare Memory with Accumulator (absoluteX)
            CMP(absoluteX_1());
            cycles += 4;
            break;

        case 0xDE: // DEC: Decrement Memory by One (absoluteX)
            DEC(absoluteX());
            cycles += 7;
            break;

        case 0xDF:
            illegal_opcode(opcode);
            break; 
           
        case 0xE0:// CPX: Compare Memory and Index X (inmediate)
            CPX(inmediate());
            cycles += 2;
            break;
            
        case 0xE1: // SBC: Subtract Memory from Accumulator with Borrow (indirectX)
            SBC(indirectX());
            cycles += 6;
            break;

        case 0xE2:
            illegal_opcode(opcode);
            break; 

        case 0xE3:
            illegal_opcode(opcode);
            break; 

        case 0xE4: // CPX: Compare Memory and Index X (zeropage)
            CPX(zeropage());
            cycles += 3;
            break;

        case 0xE5: // SBC: Subtract Memory from Accumulator with Borrow (zeropage)
            SBC(zeropage());
            cycles += 3;
            break;

        case 0xE6: // INC: Increment Memory by One (zeropage)
            INC(zeropage());
            cycles += 5;
            break;
            
        case 0xE7:
            illegal_opcode(opcode);
            break; 

        case 0xE8: //INX: Increment Index X by One
            INX();
            cycles += 2;
            break;
            
        case 0xE9: // SBC: Subtract Memory from Accumulator with Borrow (inmediate)
            SBC(inmediate());
            cycles += 2;
            break;
            
        case 0xEA: // Nop
            // No operation
            cycles += 2;
            break;
            
        case 0xEB:
            illegal_opcode(opcode);
            break; 

        case 0xEC: // CPX: Compare Memory and Index X (absolute)
            CPX(absolute());
            cycles += 4;
            break;

        case 0xED: // SBC: Subtract Memory from Accumulator with Borrow (absolute)
            SBC(absolute());
            cycles += 4;
            break;

        case 0xEE: // INC: Increment Memory by One (absolute)
            INC(absolute());
            cycles += 6;
            break;

        case 0xEF:
            illegal_opcode(opcode);
            break; 
            
        case 0xF0: // BEQ: Branch on Result Zero
            BXX(Z_Flag);
            cycles += 2;
            break;

        case 0xF1: // SBC: Subtract Memory from Accumulator with Borrow (indirectY)
            SBC(indirectY_1());
            cycles += 5;
            break;

        case 0xF2:
            illegal_opcode(opcode);
            break; 

        case 0xF3:
            illegal_opcode(opcode);
            break; 

        case 0xF4:
            illegal_opcode(opcode);
            break; 

        case 0xF5: // SBC: Subtract Memory from Accumulator with Borrow (zeropageX)
            SBC(zeropageX());
            cycles += 4;
            break;

        case 0xF6: // INC: Increment Memory by One (zeropageX)
            INC(zeropageX());
            cycles += 6;
            break;

        case 0xF7:
            illegal_opcode(opcode);
            break; 

        case 0xF8: // SED: Set Decimal Flag
            SED();
            cycles += 2;
            break;
            
        case 0xF9: // SBC: Subtract Memory from Accumulator with Borrow (absoluteY)
            SBC(absoluteY_1());
            cycles += 4;
            break;

        case 0xFA:
            illegal_opcode(opcode);
            break; 

        case 0xFB:
            illegal_opcode(opcode);
            break; 

        case 0xFC:
            illegal_opcode(opcode);
            break; 

        case 0xFD: // SBC: Subtract Memory from Accumulator with Borrow (absoluteX)
            SBC(absoluteX_1());
            cycles += 4;
            break;

        case 0xFE: // INC: Increment Memory by One (absoluteX)
            INC(absoluteX());
            cycles += 7;
            break;

        case 0xFF:
            illegal_opcode(opcode);
            break; 
    }
    return cycles;
}

