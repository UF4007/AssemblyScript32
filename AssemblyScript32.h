#pragma once
#include <stdint.h>
#include <vector>
#include <atomic>

namespace asc{
    constexpr size_t namelen = 8;
    using name_t = char[namelen];
    // instruction: opcode(16) | address1(8) | address2(8) | oprand1(32, if have) | oprand2(32, if have)
    enum class opcode_t : uint16_t
    {
        EXPAND = 0x01, // reserved instruction
        MOV = 0x89,    // (op1,op2):move op2 to op1
        MOVE = 0xB8,   // (op1):move op1 to ESP
        XCHG = 0x87,   // (op1,op2):exchange op2 and op1

        ADD = 0x03, // (op1,op2):op1 + op2, save result to [ESP]
        SUB = 0x2B, // (op1,op2):op1 - op2, save result to [ESP]
        MUL = 0xF7, // (op1,op2):op1 * op2, save result to [ESP]
        DIV = 0xF8, // (op1,op2):op1 / op2, save result to [ESP]
        INC = 0x40, // (op1):op1 self increase
        DEC = 0x48, // (op1):op1 self reduce
        NEG = 0xF9, // (op1):op1 self negative

        AND = 0x23, // (op1,op2):op1 | op2, save result to [ESP]
        OR = 0x0B,  // (op1,op2):op1 & op2, save result to [ESP]
        XOR = 0x33, // (op1,op2):op1 ^ op2, save result to [ESP]
        NOT = 0xFa, // (op1):~op1, save result to [ESP]
        SHL = 0xD1, // (op1,op2):op1 << op2, save result to [ESP], SHR negative.

        PUSH = 0x50, // (op1):set [ESP] to op1, and ESP increase 4
        POP = 0x58,  // ():ESP reduce 4
        JMP = 0xE9,  // (op1):set EIP to op1
        JZ = 0x74,   // (op1):if [ESP] zero, set EIP to op1
        JNZ = 0x75,  // (op1):if [ESP] not zero, set EIP to op1
        CALL = 0xE8, // (op1):call callable op1
        CALLEXT = 0xFF, // (op1):call callable op1 in extlib
        RET = 0xC3,  // ():return
        NOP = 0x90,  // ():null operation
        INT = 0xCC,  // ():interrupt, dump core like segment fault.

        CMP = 0x39,   // (op1,op2):compare op1 == op2, save result to [ESP]
        CMPG = 0x7D,  // (op1,op2):compare op1 > op2, save result to [ESP]
        CMPGE = 0x7E, // (op1,op2):compare op1 >= op2, save result to [ESP]
        // LEA = 0x8D,   // no need lea.
    };
    //all operands are deemed as int32_t
    enum class oprand_t : uint8_t
    {
        IMM = 0,        // op = op
        IA = 1,         // op = [op]
        ESP = 2,        // op = esp + op
        ESP_IA = 3,     // op = [esp + op]
        // EBP = 4,     // EBP cannot be altered, and cannot be read. EBP in the callee function means the caller fucntion ESP.
        EBP_IA = 5,     // op = [ebp + op]
        DATA_IA = 6     // op = [data + op]
    };
    struct elf_t;
    struct func_t {
        inline bool addressing_esp(int32_t *&ret)
        {
            if (ESP >= assembly.size())
                return false;
            ret = (int32_t *)&assembly[ESP];
            return true;
        }
        bool addressing_w(oprand_t opt, int32_t op, int32_t* &ret);
        inline bool addressing_r(oprand_t opt, int32_t op, int32_t &ret)
        {
            switch (opt)
            {
            // lvalue
            case oprand_t::IMM:
                ret = op;
                return true;
            case oprand_t::ESP:
                ret = ESP + op;
                return true;
            // rvalue
            default:
            {
                int32_t *addr = nullptr;
                if (addressing_w(opt, op, addr))
                    ret = *addr;
                return true;
            }
            }
            return false;
        }
        bool callable_addressing(oprand_t opt, int32_t op, func_t*& ret, elf_t*& elf_callee);

        elf_t *elf_local;
        func_t *caller;

        uint32_t ESP;
        uint32_t EIP;
        uint32_t EIP_Begin;
        std::vector<uint8_t> assembly;
        bool operator()(elf_t* elf, func_t* caller);
    };
    using extfunc_t = bool (*)(func_t*);
    struct elf_t
    {
        struct dependency_t {
            name_t name;
            elf_t* ptr;
        };
        struct import_t{
            int dependency_index;
            int func_index;     // func index in target dependency elf
            elf_t* ptr;
        };
        name_t name;
        std::vector<dependency_t> dependency;   // name of external elf
        std::vector<import_t> imports;
        std::atomic<int> referenced;
        std::vector<uint8_t> data;
        std::vector<func_t> text;
        inline bool load_elf(elf_t * elf)
        {
            size_t i = 0;
            for (auto& it : dependency)
            {
                if (std::strcmp(elf->name, it.name) == 0)
                {
                    it.ptr = elf;
                    break;
                }
                i++;
            }
            if (i == dependency.size())
                return false;

            elf->referenced.fetch_add(1);
            for (auto& it : imports)
            {
                if (it.dependency_index == i)
                    it.ptr = elf;
            }
        }
        inline void unload_elf()
        {
            for (auto& it : imports)
            {
                it.ptr = nullptr;
            }
            for (auto& it : dependency)
            {
                if (it.ptr != nullptr)
                {
                    it.ptr->referenced.fetch_sub(1);
                    it.ptr = nullptr;
                }
            }
        }
        inline bool operator()() // execute index 0 function
        {
            if (text.size() == 0)
                return false;
            return text[0](this, nullptr);
        }
    };



    //extlib
#include "AS32_extlib.h"



    // definitions
    inline bool func_t::addressing_w(oprand_t opt, int32_t op, int32_t *&ret)
    {
        switch (opt)
        {
        case oprand_t::IMM:
            return false; // cannot write a rvalue.
        case oprand_t::IA:
        {
            uint32_t off = static_cast<uint32_t>(op);
            if (off >= assembly.size())
                return false;
            ret = (int32_t *)&assembly[off];
        }
            return true;
        case oprand_t::ESP:
        {
            return false; // cannot write a rvalue.
        }
        case oprand_t::ESP_IA:
        {
            uint32_t off = ESP + op;
            if (off >= assembly.size())
                return false;
            ret = (int32_t *)&assembly[off];
        }
            return true;
        case oprand_t::EBP_IA:
        {
            if (caller == nullptr)
                return false;
            uint32_t off = caller->ESP + op;
            if (off >= caller->assembly.size())
                return false;
            ret = (int32_t *)&caller->assembly[off];
        }
            return true;
        case oprand_t::DATA_IA:
        {
            uint32_t off = op;
            if (off >= elf_local->data.size())
                return false;
            ret = (int32_t *)&elf_local->data[off];
        }
            return true;
        }
        return false;
    }
    inline bool func_t::callable_addressing(oprand_t opt, int32_t op, func_t*& ret, elf_t*& elf_callee)
    {
        int32_t ind;
        if (addressing_r(opt, op, ind))
        {
            if (ind < 0) // import callable
            {
                ind *= -1;
                ind--;
                if (ind >= elf_local->imports.size())
                    return false;
                auto &impo = elf_local->imports[ind];
                if (impo.ptr == nullptr)
                    return false;
                elf_callee = impo.ptr;
                if (impo.func_index >= elf_callee->text.size())
                    return false;
                ret = &elf_callee->text[impo.func_index];
                return true;
            }
            else // local elf callable
            {
                if (ind >= elf_local->text.size())
                    return false;
                ret = &elf_local->text[ind];
                return true;
            }
        }
        else
            return false;
    }
    inline bool func_t::operator()(elf_t* elf, func_t* caller)
    {
        ESP = 0;
        EIP = EIP_Begin;
        this->elf_local = elf;
        this->caller = caller;
        while (1)
        {
            if (EIP + 3 * sizeof(uint32_t) > assembly.size())
                return false; // #Seg Fault

            opcode_t& opcode = *(opcode_t*)&assembly[EIP];
            oprand_t& oprandT1 = *(oprand_t*)&assembly[EIP + sizeof(opcode_t)];
            oprand_t& oprandT2 = *(oprand_t*)&assembly[EIP + sizeof(opcode_t) + sizeof(oprand_t)];
            bool isJump = false;
            switch (opcode)
            {
                // case opcode_t::EXPAND:
                //     break;
            case opcode_t::MOV:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t op2 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t) + sizeof(int32_t)];
                int32_t* rv;
                int32_t lv;
                if (addressing_w(oprandT1, op1, rv) && addressing_r(oprandT2, op2, lv))
                {
                    *rv = lv;
                    EIP += 2 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::MOVE:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t lv;
                if (addressing_r(oprandT2, op1, lv))
                {
                    ESP = lv;
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::XCHG:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t op2 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t) + sizeof(int32_t)];
                int32_t* rv1;
                int32_t* rv2;
                if (addressing_w(oprandT1, op1, rv1) && addressing_w(oprandT2, op2, rv2))
                {
                    int32_t tmp = *rv1;
                    *rv1 = *rv2;
                    *rv2 = tmp;
                    EIP += 2 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::INC:
            case opcode_t::NEG:
            case opcode_t::DEC:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t* rv;
                if (addressing_w(oprandT1, op1, rv))
                {
                    switch (opcode)
                    {
                    case opcode_t::INC:
                        (*rv)++;
                        break;
                    case opcode_t::NEG:
                        (*rv) *= -1;
                        break;
                    case opcode_t::DEC:
                        (*rv)--;
                    }
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::ADD:
            case opcode_t::SUB:
            case opcode_t::MUL:
            case opcode_t::DIV:
            case opcode_t::AND:
            case opcode_t::OR:
            case opcode_t::XOR:
            case opcode_t::SHL:
            case opcode_t::CMP:
            case opcode_t::CMPG:
            case opcode_t::CMPGE:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t op2 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t) + sizeof(int32_t)];
                int32_t* espad;
                int32_t lv;
                int32_t lv2;
                if (addressing_r(oprandT1, op1, lv) && addressing_r(oprandT2, op2, lv2) && addressing_esp(espad))
                {
                    switch (opcode)
                    {
                    case opcode_t::ADD:
                        *espad = lv + lv2;
                        break;
                    case opcode_t::SUB:
                        *espad = lv - lv2;
                        break;
                    case opcode_t::MUL:
                        *espad = lv * lv2;
                        break;
                    case opcode_t::DIV:
                        if (lv2 == 0)
                            return false;
                        *espad = lv / lv2;
                        break;
                    case opcode_t::AND:
                        *espad = lv & lv2;
                        break;
                    case opcode_t::OR:
                        *espad = lv | lv2;
                        break;
                    case opcode_t::XOR:
                        *espad = lv ^ lv2;
                        break;
                    case opcode_t::SHL:
                        if (lv2 > 0) {
                            *espad = lv << lv2;
                        }
                        else if (lv2 < 0) {
                            *espad = lv >> -lv2;
                        }
                        break;
                    case opcode_t::CMP:
                        *espad = lv == lv2;
                        break;
                    case opcode_t::CMPG:
                        *espad = lv > lv2;
                        break;
                    case opcode_t::CMPGE:
                        *espad = lv >= lv2;
                        break;
                    }
                    EIP += 2 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            break;
            case opcode_t::NOT:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t* espad;
                int32_t lv;
                if (addressing_r(oprandT1, op1, lv) && addressing_esp(espad))
                {
                    *espad = ~lv;
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::PUSH:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t* espad;
                int32_t lv;
                if (addressing_r(oprandT1, op1, lv) && addressing_esp(espad))
                {
                    *espad = lv;
                    ESP += 4;
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::POP:
                if (ESP >= 4)
                    ESP -= 4;
                else
                    return false;
                break;
            case opcode_t::JMP:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t lv;
                if (addressing_r(oprandT1, op1, lv))
                {
                    EIP = lv;
                    isJump = true;
                }
                else
                    return false;
            }
            break;
            case opcode_t::JNZ:
            case opcode_t::JZ:
            {
                int32_t op1 = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t* espad;
                int32_t lv;
                if (addressing_r(oprandT1, op1, lv) && addressing_esp(espad))
                {
                    switch (opcode)
                    {
                    case opcode_t::JNZ:
                        if (*espad != 0)
                        {
                            EIP = lv;
                            isJump = true;
                        }
                        else
                            EIP += 1 * sizeof(uint32_t);
                        break;
                    case opcode_t::JZ:
                        if (*espad == 0)
                        {
                            EIP = lv;
                            isJump = true;
                        }
                        else
                            EIP += 1 * sizeof(uint32_t);
                        break;
                    }
                }
                else
                    return false;
            }
            break;
            case opcode_t::CALL:
            {
                int32_t op = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                func_t* callable;
                elf_t* elf_callee = this->elf_local;
                if (callable_addressing(oprandT1, op, callable, elf_callee))
                {
                    if (callable->operator()(elf_callee, this) == false)
                        return false;
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::CALLEXT:
            {
                int32_t op = *(int32_t*)&assembly[EIP + sizeof(opcode_t) + 2 * sizeof(oprand_t)];
                int32_t ind;
                if (addressing_r(oprandT1, op, ind))
                {
                    if (ind < 0 || ind >= sizeof(extlib))
                        return false;
                    if (extlib[ind](this) == false)
                        return false;
                    EIP += 1 * sizeof(uint32_t);
                }
                else
                    return false;
            }
            break;
            case opcode_t::RET:
                return true;
                break;
            case opcode_t::NOP:
                break;
            case opcode_t::INT:
                return false;
                break;
            default:
                return false; // #Undefined Opcode
            }

            if (isJump == false)
                EIP += sizeof(uint32_t);
        }
    }
};