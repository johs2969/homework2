#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef unsigned int       uint32_t;
typedef int                int32_t;

// 레지스터 개수 및 메모리 크기 정의
#define NUM_REGISTERS 32
#define MEMORY_SIZE 16

// 레지스터 배열과 프로그램 카운터, 메모리 정의
uint32_t registers[NUM_REGISTERS];
uint32_t memory[MEMORY_SIZE];
uint32_t pc = 0;

// 프로세서 상태 정의
typedef enum {
    IF,
    ID,
    EX,
    MEM,
    WB
} STATE;

// 명령어의 종류 정의
typedef enum {
    ADD, ADDI, LW, SW, BEQ, BLT, LUI
} OPCODE;

// Binary 명령어 형식 정의 
typedef struct {
    char inst[10]; //opcode
    int param[3];  //operand 
} RAW_INST;

// RISC-V 명령어 구조체 정의
typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    int32_t imm;
} INSTRUCTION;

// 파이프라인 레지스터 구조체 정의
typedef struct {
    RAW_INST raw_inst; //binary 형태의 instruction 저장
    INSTRUCTION decoded_inst; //decoding된 instruction 저장
    uint32_t pc;
    int32_t alu_result;
    int32_t mem_data;
    uint8_t valid;
    uint8_t Frs1;
    uint8_t Frs2;
    int imm;
} PipelineRegister;

PipelineRegister IF_ID, ID_EX, EX_MEM, MEM_WB;

// 명령어 디코딩 함수: HW#1을 통해 수행된 내용과 동일
INSTRUCTION decode(RAW_INST target_instruction) {
    INSTRUCTION decoded_inst;
    if (strcmp(target_instruction.inst, "add") == 0)
        decoded_inst.opcode = ADD; // add
    else if (strcmp(target_instruction.inst, "addi") == 0)
        decoded_inst.opcode = ADDI; // addi
    else if (strcmp(target_instruction.inst, "lw") == 0)
        decoded_inst.opcode = LW; // lw
    else if (strcmp(target_instruction.inst, "sw") == 0)
        decoded_inst.opcode = SW; // sw    
    else if (strcmp(target_instruction.inst, "beq") == 0)
        decoded_inst.opcode = BEQ; // beq    
    else if (strcmp(target_instruction.inst, "blt") == 0)
        decoded_inst.opcode = BLT; // blt    
    else if (strcmp(target_instruction.inst, "lui") == 0)
        decoded_inst.opcode = LUI; // lui   

    switch (decoded_inst.opcode) {
    case ADD:
        decoded_inst.rd = target_instruction.param[0];
        decoded_inst.rs1 = target_instruction.param[1];
        decoded_inst.rs2 = target_instruction.param[2];
        break;
    case ADDI:
        decoded_inst.rd = target_instruction.param[0];
        decoded_inst.rs1 = target_instruction.param[1];
        decoded_inst.imm = target_instruction.param[2];
        break;
    case LW:
        decoded_inst.rd = target_instruction.param[0];
        decoded_inst.imm = target_instruction.param[1];
        decoded_inst.rs1 = target_instruction.param[2];
        break;
    case SW:
        decoded_inst.rs2 = target_instruction.param[0];
        decoded_inst.imm = target_instruction.param[1];
        decoded_inst.rs1 = target_instruction.param[2];
        break;
    case BEQ:
    case BLT:
        decoded_inst.rs1 = target_instruction.param[0];
        decoded_inst.rs2 = target_instruction.param[1];
        decoded_inst.imm = target_instruction.param[2];
        break;
    case LUI:
        decoded_inst.rd = target_instruction.param[0];
        decoded_inst.imm = target_instruction.param[1];
        break;
    }
    return decoded_inst;
}

// 명령어 페치(fetch)함수: 주어진 프로그램(txt파일)으로부터 명령어를 읽어 들임
RAW_INST fetch(uint32_t pc) {
    char filename[] = "hw02_programB.txt"; //프로그램B를 수행하기 위해서는 문자열을 "hw02_programB.txt"로 변경
    char buffer[100];
    FILE* file;
    char* token;
    char* context = NULL;
    int target_line = pc / 4;
    int current_line = 0;
    int match_flag = 0;
    RAW_INST target_instruction;

    errno_t err = fopen_s(&file, filename, "r");
    if (err != 0) {
        perror("파일을 열 수 없습니다");
        exit(1);
    }

    // 파일에서 한 줄씩 읽기
    //printf("curr pc: %d, targetline:%d\n", pc, target_line);
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (current_line == target_line) {
            token = strtok_s(buffer, " \t\n", &context);
            strcpy_s(target_instruction.inst, token);
            int i = 0;
            while (token != NULL) {
                //printf("%s\n", token);
                token = strtok_s(NULL, " \t\n", &context);
                if (token != NULL)
                    target_instruction.param[i] = atoi(token);
                i++;
            }
            match_flag = 1;
            break; // 원하는 라인을 찾았으므로 루프를 종료합니다.
        }
        current_line++;
    }


    // 파일을 끝까지 읽었는지 확인
    if (match_flag == 0) {
        strcpy_s(target_instruction.inst, "nop");
        fclose(file);
    }

    // 결과 출력
    //printf("첫 번째 단어: %s\n", target_instruction.inst);
    //printf("두 번째 단어: %d\n", target_instruction.param[0]);
    //printf("세 번째 단어: %d\n", target_instruction.param[1]);
    //printf("네 번째 단어: %d\n", target_instruction.param[2]);

    // 파일 닫기
    fclose(file);
    return target_instruction;
}

//레지스터 데이터 중, 0이 아닌 값을 출력하는 함수
void print_register_values() {
    for (int i = 0; i < NUM_REGISTERS; i++) {
        if (registers[i] != 0)
            printf("Register[x%02d]: %d \n", i, registers[i]);
    }
}

//메모리 데이터 중, 0이 아닌 값을 출력하는 함수
void print_memory_values() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i] != 0)
            printf("Memory[%02d]: %d \n", i, memory[i]);
    }
}

//명령어 정보를 출력하는 함수
void print_instruction_info(INSTRUCTION inst) {
    switch (inst.opcode) {
    case ADD:
        printf("opcode: ADD, rd:x%02u, rs1:x%02u, rs2:x%02u\n", inst.rd, inst.rs1, inst.rs2);
        break;
    case ADDI:
        printf("opcode: ADDI, rd:x%02u, rs1:x%02u, imm:%02d, \n", inst.rd, inst.rs1, inst.imm);
        break;
    case LW:
        printf("opcode: LW, rd:x%02u, offset:%02d, rs1:x%02u\n", inst.rd, inst.imm, inst.rs1);
        break;
    case SW:
        printf("opcode: SW, rs2:x%02u, offset:%02d, rs1:x%02u\n", inst.rs2, inst.imm, inst.rs1);
        break;
    case BEQ:
        printf("opcode: BEQ, rs1:x%02u, rs2:x%02u, imm:%02d\n", inst.rs1, inst.rs2, inst.imm);
        break;
    case BLT:
        printf("opcode: BLT, rs1:x%02u, rs2:x%02u, imm:%02d\n", inst.rs1, inst.rs2, inst.imm);
        break;
    case LUI:
        printf("opcode: LUI, rd:x%02u, imm:%02d\n", inst.rd, inst.imm);
        break;
    }
}



// 클록 사이클 실행 함수 - HW#02 수행을 위해 수정해야하는 함수, 해당 함수 외 "나머지 부분 수정할 필요 없음".
int clock_cycle(int cycle) {
    printf("***  Current Cycle:%02d, PC:%02d  *** \n", cycle, pc);

    int forwardA = 0, forwardB = 0;


    // Write Back (WB)
    if (MEM_WB.valid) {
        if (MEM_WB.decoded_inst.opcode == ADD || MEM_WB.decoded_inst.opcode == ADDI || MEM_WB.decoded_inst.opcode == LUI) {
            registers[MEM_WB.decoded_inst.rd] = MEM_WB.alu_result; //TODO: ADD와 같이, WB 단계에서 레지스터 파일 값을 변경해야 하는 명령어 타입에 대해    
            // ALU 연산 값으로 목적 레지스터 파일 값 업데이트 수행
        }
        else if (MEM_WB.decoded_inst.opcode == LW) {
            registers[MEM_WB.decoded_inst.rd] = MEM_WB.mem_data; //TODO-2: LW 연산에 적합한 동작 수행
        }
        printf("WB stage: ");
        print_instruction_info(MEM_WB.decoded_inst);
    }


    // Memory Access (MEM)
    if (EX_MEM.valid) {
        MEM_WB.decoded_inst = EX_MEM.decoded_inst;
        MEM_WB.alu_result = EX_MEM.alu_result;
        MEM_WB.pc = EX_MEM.pc;
        MEM_WB.valid = 1;

        if (EX_MEM.decoded_inst.opcode == LW) {
            MEM_WB.mem_data = memory[EX_MEM.alu_result / 4]; //TODO. 메모리에서 읽어온 데이터를 MEM_WB 레지스터에 저장
        }
        else if (EX_MEM.decoded_inst.opcode == SW) {
            memory[EX_MEM.alu_result / 4] = registers[EX_MEM.decoded_inst.rs2]; //TODO. 레지스터에서 읽어온 데이터를 메모리의 목적 주소에 저장
        }
        else if (EX_MEM.decoded_inst.opcode == BEQ) {
            if (EX_MEM.alu_result == 0) {
                printf("branch performed!! Invalid instructions in the previous stages.\n");
                pc = EX_MEM.pc + EX_MEM.decoded_inst.imm; // TODO. 프로그램 카운터 값 목적지 주소로 업데이트
                IF_ID.valid = ID_EX.valid = EX_MEM.valid = 0; // TODO. Branch taken으로 인한, pipelining stage 업데이트
            }
        }
        else if (EX_MEM.decoded_inst.opcode == BLT) {
            if (EX_MEM.alu_result < 0) {
                printf("branch performed!! Invalid instructions in the previous stages.\n");
                pc = EX_MEM.pc + EX_MEM.decoded_inst.imm; // TODO. 프로그램 카운터 값 목적지 주소로 업데이트
                IF_ID.valid = ID_EX.valid = EX_MEM.valid = 0; // TODO. Branch taken으로 인한, pipelining stage 업데이트
            }
        }
        printf("MEM stage: ");
        print_instruction_info(EX_MEM.decoded_inst);
        //print_memory_values();
    }
    else {
        MEM_WB.valid = 0;
    }

    // mux control signal A
    if ((MEM_WB.decoded_inst.rd != 0) &&
        (MEM_WB.decoded_inst.opcode == ADD || MEM_WB.decoded_inst.opcode == ADDI || MEM_WB.decoded_inst.opcode == LUI || MEM_WB.decoded_inst.opcode == LW) &&
        (MEM_WB.decoded_inst.rd == ID_EX.decoded_inst.rs1))
    {
        forwardA = 01;

        if (MEM_WB.decoded_inst.opcode == ADD || MEM_WB.decoded_inst.opcode == ADDI || MEM_WB.decoded_inst.opcode == LUI) {
            ID_EX.Frs1 = MEM_WB.alu_result;
        }
        else {
            ID_EX.Frs1 = MEM_WB.mem_data;
        }

    }
    else if ((EX_MEM.decoded_inst.rd != 0) &&
        (EX_MEM.decoded_inst.opcode == ADD || EX_MEM.decoded_inst.opcode == ADDI || EX_MEM.decoded_inst.opcode == LUI) &&
        (EX_MEM.decoded_inst.rd == ID_EX.decoded_inst.rs1))
    {
        forwardA = 10;

        ID_EX.Frs1 = EX_MEM.alu_result;

    }

    // mux control signal B
    if ((MEM_WB.decoded_inst.rd != 0) &&
        (MEM_WB.decoded_inst.opcode == ADD || MEM_WB.decoded_inst.opcode == ADDI || MEM_WB.decoded_inst.opcode == LUI || MEM_WB.decoded_inst.opcode == LW) &&
        (MEM_WB.decoded_inst.rd == ID_EX.decoded_inst.rs2))
    {
        forwardB = 01;

        if (MEM_WB.decoded_inst.opcode == ADD || MEM_WB.decoded_inst.opcode == ADDI || MEM_WB.decoded_inst.opcode == LUI) {
            ID_EX.Frs2 = MEM_WB.alu_result;
        }
        else {
            ID_EX.Frs2 = MEM_WB.mem_data;
        }

    }
    else if ((EX_MEM.decoded_inst.rd != 0) &&
        (EX_MEM.decoded_inst.opcode == ADD || EX_MEM.decoded_inst.opcode == ADDI || EX_MEM.decoded_inst.opcode == LUI ) &&
        (EX_MEM.decoded_inst.rd == ID_EX.decoded_inst.rs2))
    {
        forwardB = 10;

        ID_EX.Frs2 = EX_MEM.alu_result;

    }


    // Execute (EX)
    if (ID_EX.valid) {
        EX_MEM.decoded_inst = ID_EX.decoded_inst;
        EX_MEM.pc = ID_EX.pc;
        EX_MEM.valid = 1;
        //TODO. 주어진 명령어 종류에 따른 적합한 ALU 연산 수행.
        //ADD, ADDI, LW, SW, BEQ, BLT, LUI


        if (ID_EX.decoded_inst.opcode == ADD) {
            EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] + registers[ID_EX.decoded_inst.rs2]; // TODO. add
            //
        }
        else if (ID_EX.decoded_inst.opcode == ADDI) {
            if (forwardA == 0)
                EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] + ID_EX.decoded_inst.imm; // TODO. addi
            else if (forwardA)
                EX_MEM.alu_result = ID_EX.Frs1 + ID_EX.decoded_inst.imm;
            //
        }
        else if (ID_EX.decoded_inst.opcode == LW) {
            EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] + ID_EX.decoded_inst.imm; // TODO. load
        }

        else if (ID_EX.decoded_inst.opcode == SW) {
            EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] + ID_EX.decoded_inst.imm; // TODO. store
            //
        }
        else if (ID_EX.decoded_inst.opcode == BEQ) {
            if (forwardA == 0 && forwardB == 0)
                EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] - registers[ID_EX.decoded_inst.rs2]; // TODO. beq
            else if (forwardA == 0 && forwardB != 0)
                EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] - ID_EX.Frs2;
            else if (forwardA != 0 && forwardB == 0)
                EX_MEM.alu_result = ID_EX.Frs1 - registers[ID_EX.decoded_inst.rs2];
            else
                EX_MEM.alu_result = ID_EX.Frs1 - ID_EX.Frs2;
            //
        }
        else if (ID_EX.decoded_inst.opcode == BLT) {  // blt - 아래는 예시 제공을 위한 완성된 코드입니다. 
            if (forwardA == 0 && forwardB == 0)
                EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] - registers[ID_EX.decoded_inst.rs2];
            else if (forwardA == 0 && forwardB != 0)
                EX_MEM.alu_result = registers[ID_EX.decoded_inst.rs1] - ID_EX.Frs2;
            else if (forwardA != 0 && forwardB == 0)
                EX_MEM.alu_result = ID_EX.Frs1 - registers[ID_EX.decoded_inst.rs2];
            else
                EX_MEM.alu_result = ID_EX.Frs1 - ID_EX.Frs2;
            //
        }
        else if (ID_EX.decoded_inst.opcode == LUI) {  // lui - 아래는 예시 제공을 위한 완성된 코드입니다. 
            EX_MEM.alu_result = (ID_EX.decoded_inst.imm << 12);
        }


        printf("EX stage: ");
        print_instruction_info(ID_EX.decoded_inst);
    }
    else {
        EX_MEM.valid = 0;
    }

    // Instruction Decode / Register Fetch (ID)
    if (IF_ID.valid) {

        ID_EX.decoded_inst = decode(IF_ID.raw_inst);
        ID_EX.pc = IF_ID.pc;
        ID_EX.valid = 1;
        printf("ID stage: ");
        print_instruction_info(ID_EX.decoded_inst);
    }
    else {
        ID_EX.valid = 0;
    }



    //Stall
    if ((ID_EX.decoded_inst.opcode == LW) && (((ID_EX.decoded_inst.rd == decode(IF_ID.raw_inst).rs1)) || ((ID_EX.decoded_inst.rd == decode(IF_ID.raw_inst).rs2))))
    {
        ID_EX.decoded_inst.rs1 = 0;
        ID_EX.decoded_inst.rs2 = 0;
        ID_EX.decoded_inst.rd = 0;
        ID_EX.decoded_inst.imm = 0;
        ID_EX.decoded_inst.opcode = 0;
        printf("IF stage: binary value of inst[%d]\n", pc / 4);

    }
    else {
        // Instruction Fetch (IF)
        IF_ID.raw_inst = fetch(pc); //Do fetch - implemented
        IF_ID.pc = pc;
        if (strcmp(IF_ID.raw_inst.inst, "nop") != 0) {

            IF_ID.valid = 1;
            printf("IF stage: binary value of inst[%d]\n", pc / 4);
            pc += 4; //update the program counter

        }
        else {  // there is no 
            IF_ID.valid = 0;
            printf("There is no more instruction to read.\n");
        }

    }
    print_register_values();    //print registers' value if it is not zero.
    print_memory_values();      //print memory data if it is not zero.
    printf("\n");

    //더 이상 수행할 명령어가 없고 모든 stage에서 아무런 동작도 하지 않는 경우, 프로그램을 종료함.
    if (strcmp(IF_ID.raw_inst.inst, "nop") == 0 && !ID_EX.valid && !EX_MEM.valid && !MEM_WB.valid) {
        return -1;
    }
    else
        return 1;


}



int main() {
    /* 프로그램 A를 위한 초기화
    registers[2] = 4;
    registers[3] = 8;
    registers[5] = 16;
    registers[7] = 7;
    registers[8] = 8;
    registers[9] = 9;
    memory[2] = 44;
    memory[9] = 40;
    */
    /// *** 프로그램 B를 사용할 때는 위의 초기화 부분을 전체 주석처리 하고 수행합니다. ***

    pc = 0;

    // 클록 사이클 실행
    int i = 0;
    for (i = 0; i < 100; i++) {
        if (clock_cycle(i) == -1)
            break;
    }

    printf(" *** Program is done.. *** \n");
    printf("Total cycle simulated: %02d\n", i);
    print_register_values();
    print_memory_values();
    // 결과 출력
    //printf("x3 = %d\n", registers[3]);  // x3의 값 출력

    return 0;
}
