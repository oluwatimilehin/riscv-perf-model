
#include "InstGenerator.hpp"
#include "Inst.hpp"
#include "mavis/Mavis.h"
#include "mavis/JSONUtils.hpp"
#include <boost/json.hpp>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>
#include <ostream>

#include "trace_analyser/TraceAnalyser.hpp"

namespace olympia
{
    std::unique_ptr<InstGenerator>
    InstGenerator::createGenerator(sparta::log::MessageSource & info_logger,
                                   MavisType* mavis_facade, const std::string & filename,
                                   const bool skip_nonuser_mode)
    {
        const std::string json_ext = "json";
        if ((filename.size() > json_ext.size())
            && filename.substr(filename.size() - json_ext.size()) == json_ext)
        {
            std::cout << "olympia: JSON file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(
                new JSONInstGenerator(info_logger, mavis_facade, filename));
        }

        const std::string stf_ext = "stf"; // Should cover both zstf and stf
        if ((filename.size() > stf_ext.size())
            && filename.substr(filename.size() - stf_ext.size()) == stf_ext)
        {
            std::cout << "olympia: STF file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(
                new TraceInstGenerator(info_logger, mavis_facade, filename, skip_nonuser_mode));
        }

        // Dunno what it is...
        sparta_assert(false, "Unknown file extension for '" << filename
                                                            << "'.  Expected .json or .[z]stf");
        return nullptr;
    }

    std::unique_ptr<InstGenerator> InstGenerator::createProbabilisticGenerator(
        sparta::log::MessageSource & info_logger, MavisType* mavis_facade,
        const std::string & filename, const sparta::Clock* clk, uint32_t sequence_length,
        const bool skip_nonuser_mode)
    {
        std::cout << "olympia: Running probabilistic instruction generator" << std::endl;
        return std::make_unique<ProbabilisticInstGenerator>(
            info_logger, mavis_facade, filename, clk, sequence_length, skip_nonuser_mode);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // JSON Inst Generator
    JSONInstGenerator::JSONInstGenerator(sparta::log::MessageSource & info_logger,
                                         MavisType* mavis_facade, const std::string & filename) :
        InstGenerator(info_logger, mavis_facade)
    {
        std::ifstream fs;
        std::ios_base::iostate exceptionMask = fs.exceptions() | std::ios::failbit;
        fs.exceptions(exceptionMask);

        try
        {
            jobj_ = mavis::parseJSON(filename).as_array();
        }
        catch (const std::ifstream::failure & e)
        {
            throw sparta::SpartaException("ERROR: Issues opening ") << filename << ": " << e.what();
        }
        n_insts_ = jobj_.size();
    }

    bool JSONInstGenerator::isDone() const { return (curr_inst_index_ >= n_insts_); }

    void JSONInstGenerator::reset(const InstPtr & inst_ptr, const bool skip = false)
    {
        const uint64_t saved_index = inst_ptr->getRewindIterator<uint64_t>();

        // Validate that the saved index is within bounds
        sparta_assert(saved_index < n_insts_,
                      "Rewind index " << saved_index << " is out of bounds for JSON trace with "
                                      << n_insts_ << " instructions.");

        curr_inst_index_ = saved_index;
        program_id_ = inst_ptr->getProgramID();

        ILOG("Rewinding JSON trace to instruction pid:"
             << program_id_ << " uid:" << inst_ptr->getUniqueID() << " index:" << curr_inst_index_
             << (skip ? " (skipping to next)" : " (inclusive)"));

        if (skip)
        {
            ++curr_inst_index_;
            ++program_id_;
        }
    }

    InstPtr JSONInstGenerator::getNextInst(const sparta::Clock* clk)
    {
        if (SPARTA_EXPECT_FALSE(isDone()))
        {
            return nullptr;
        }

        // Get the JSON record at the current index
        const auto jinst = jobj_.at(curr_inst_index_).as_object();
        InstPtr inst;
        if (const auto it = jinst.find("opcode"); it != jinst.end())
        {
            const boost::json::value & value = it->value();
            uint64_t opcode = value.is_string()
                                  ? std::strtoull(value.as_string().c_str(), nullptr, 0)
                                  : static_cast<uint64_t>(value.as_int64());
            inst = mavis_facade_->makeInst(opcode, clk);
        }
        else
        {
            const auto mit = jinst.find("mnemonic");
            if (mit == jinst.end())
            {
                throw sparta::SpartaException() << "Missing mnemonic at " << curr_inst_index_;
            }
            const std::string mnemonic = boost::json::value_to<std::string>(mit->value());

            auto addElement = [&jinst](mavis::OperandInfo & operands, const std::string & key,
                                       const mavis::InstMetaData::OperandFieldID operand_field_id,
                                       const mavis::InstMetaData::OperandTypes operand_type)
            {
                if (const auto it = jinst.find(key); it != jinst.end())
                {
                    operands.addElement(operand_field_id, operand_type,
                                        boost::json::value_to<uint64_t>(it->value()));
                }
            };

            mavis::OperandInfo srcs;
            addElement(srcs, "rs1", mavis::InstMetaData::OperandFieldID::RS1,
                       mavis::InstMetaData::OperandTypes::LONG);
            addElement(srcs, "fs1", mavis::InstMetaData::OperandFieldID::RS1,
                       mavis::InstMetaData::OperandTypes::DOUBLE);
            addElement(srcs, "rs2", mavis::InstMetaData::OperandFieldID::RS2,
                       mavis::InstMetaData::OperandTypes::LONG);
            addElement(srcs, "fs2", mavis::InstMetaData::OperandFieldID::RS2,
                       mavis::InstMetaData::OperandTypes::DOUBLE);
            addElement(srcs, "vs1", mavis::InstMetaData::OperandFieldID::RS1,
                       mavis::InstMetaData::OperandTypes::VECTOR);
            addElement(srcs, "vs2", mavis::InstMetaData::OperandFieldID::RS2,
                       mavis::InstMetaData::OperandTypes::VECTOR);

            mavis::OperandInfo dests;
            addElement(dests, "rd", mavis::InstMetaData::OperandFieldID::RD,
                       mavis::InstMetaData::OperandTypes::LONG);
            addElement(dests, "fd", mavis::InstMetaData::OperandFieldID::RD,
                       mavis::InstMetaData::OperandTypes::DOUBLE);
            addElement(dests, "vd", mavis::InstMetaData::OperandFieldID::RD,
                       mavis::InstMetaData::OperandTypes::VECTOR);

            if (const auto it = jinst.find("imm"); it != jinst.end())
            {
                const uint64_t imm = boost::json::value_to<uint64_t>(it->value());
                mavis::ExtractorDirectOpInfoList ex_info(mnemonic, srcs, dests, imm);
                inst = mavis_facade_->makeInstDirectly(ex_info, clk);
            }
            else
            {
                mavis::ExtractorDirectOpInfoList ex_info(mnemonic, srcs, dests);
                inst = mavis_facade_->makeInstDirectly(ex_info, clk);
            }

            if (const auto it = jinst.find("vaddr"); it != jinst.end())
            {
                uint64_t vaddr = std::strtoull(it->value().as_string().c_str(), nullptr, 0);
                inst->setTargetVAddr(vaddr);
            }

            VectorConfigPtr vector_config = inst->getVectorConfig();
            if (const auto it = jinst.find("vtype"); it != jinst.end())
            {
                // immediate, so decode from hex
                uint64_t vtype = std::strtoull(it->value().as_string().c_str(), nullptr, 0);
                std::string binaryString = std::bitset<32>(vtype).to_string();
                uint32_t sew = std::pow(2, std::stoi(binaryString.substr(26, 3), nullptr, 2)) * 8;
                uint32_t lmul = std::pow(2, std::stoi(binaryString.substr(29, 3), nullptr, 2));
                vector_config->setLMUL(lmul);
                vector_config->setSEW(sew);
            }

            if (const auto it = jinst.find("vta"); it != jinst.end())
            {
                const bool vta = boost::json::value_to<uint64_t>(it->value()) > 0 ? true : false;
                vector_config->setVTA(vta);
            }

            if (const auto it = jinst.find("vl"); it != jinst.end())
            {
                const uint64_t vl = boost::json::value_to<uint64_t>(it->value());
                vector_config->setVL(vl);
            }

            if (const auto it = jinst.find("taken"); it != jinst.end())
            {
                const bool taken = boost::json::value_to<uint64_t>(it->value());
                inst->setTakenBranch(taken);
            }
        }

        inst->setRewindIterator<uint64_t>(curr_inst_index_);
        inst->setUniqueID(++unique_id_);
        inst->setProgramID(program_id_++);
        ++curr_inst_index_;
        return inst;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // STF Inst Generator
    TraceInstGenerator::TraceInstGenerator(sparta::log::MessageSource & info_logger,
                                           MavisType* mavis_facade, const std::string & filename,
                                           const bool skip_nonuser_mode) :
        InstGenerator(info_logger, mavis_facade)
    {
        std::ifstream fs;
        std::ios_base::iostate exceptionMask = fs.exceptions() | std::ios::failbit;
        fs.exceptions(exceptionMask);

        try
        {
            fs.open(filename);
        }
        catch (const std::ifstream::failure & e)
        {
            throw sparta::SpartaException("ERROR: Issues opening ") << filename << ": " << e.what();
        }

        // If true, search for an stf-pte file alongside this trace.
        constexpr bool CHECK_FOR_STF_PTE = false;

        // Filter out mode change events regardless of skip_nonuser_mode
        // value. Required for traces that stay in machine mode the entire
        // time
        constexpr bool FILTER_MODE_CHANGE_EVENTS = true;
        constexpr size_t BUFFER_SIZE = 4096;
        reader_.reset(new stf::STFInstReader(filename, skip_nonuser_mode, CHECK_FOR_STF_PTE,
                                             FILTER_MODE_CHANGE_EVENTS, BUFFER_SIZE));

        next_it_ = reader_->begin();
    }

    bool TraceInstGenerator::isDone() const { return next_it_ == reader_->end(); }

    void TraceInstGenerator::reset(const InstPtr & inst_ptr, const bool skip = false)
    {
        auto saved_it = inst_ptr->getRewindIterator<stf::STFInstReader::iterator>();

        // Validate that the saved iterator is still valid (within buffer bounds)
        // The STF reader uses a sliding window buffer - if too many instructions
        // have been read since this instruction was fetched, the iterator becomes invalid
        sparta_assert(saved_it.valid(),
                      "Rewind iterator is no longer valid for instruction uid:"
                          << inst_ptr->getUniqueID() << " pid:" << inst_ptr->getProgramID()
                          << " - instruction has moved outside the STF buffer window. "
                          << "Consider increasing the STF buffer size (currently 4096).");

        next_it_ = saved_it;
        program_id_ = inst_ptr->getProgramID();

        ILOG("Rewinding STF trace to instruction pid:"
             << program_id_ << " uid:" << inst_ptr->getUniqueID()
             << (skip ? " (skipping to next)" : " (inclusive)"));

        if (skip)
        {
            ++next_it_;
            ++program_id_;
        }
    }

    InstPtr TraceInstGenerator::getNextInst(const sparta::Clock* clk)
    {
        if (SPARTA_EXPECT_FALSE(isDone()))
        {
            return nullptr;
        }

        mavis::Opcode opcode = next_it_->opcode();

        try
        {
            InstPtr inst = mavis_facade_->makeInst(opcode, clk);
            inst->setPC(next_it_->pc());
            inst->setUniqueID(++unique_id_);
            inst->setProgramID(program_id_++);
            inst->setRewindIterator<stf::STFInstReader::iterator>(next_it_);
            if (const auto & mem_accesses = next_it_->getMemoryAccesses(); !mem_accesses.empty())
            {
                using VectorAddrType = std::vector<sparta::memory::addr_t>;
                VectorAddrType addrs;
                std::for_each(next_it_->getMemoryAccesses().begin(),
                              next_it_->getMemoryAccesses().end(),
                              [&addrs](const auto & ma) { addrs.emplace_back(ma.getAddress()); });
                inst->setTargetVAddr(addrs.front());
                // For misaligns, more than 1 address is provided
                // inst->setVAddrVector(std::move(addrs));
            }
            inst->setCoF(next_it_->isCoF());
            if (next_it_->isBranch())
            {
                inst->setTakenBranch(next_it_->isTakenBranch());
                inst->setTargetVAddr(next_it_->branchTarget());
            }
            ++next_it_;
            return inst;
        }
        catch (std::exception & excpt)
        {
            std::cerr << "ERROR: Mavis failed decoding: 0x" << std::hex << opcode
                      << " for STF It PC: 0x" << next_it_->pc() << " STFID: " << std::dec
                      << next_it_->index() << " err: " << excpt.what() << std::endl;
            throw;
        }
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Probabilistic Inst Generator
    ProbabilisticInstGenerator::ProbabilisticInstGenerator(sparta::log::MessageSource & info_logger,
                                                           MavisType* mavis_facade,
                                                           const std::string & filename,
                                                           const sparta::Clock* clk,
                                                           uint32_t sequence_length,
                                                           const bool skip_nonuser_mode) :
        InstGenerator(info_logger, mavis_facade),
        gen_(rd_())
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        TraceAnalyser analyser(info_logger, mavis_facade_, clk);
        std::vector<std::shared_ptr<InstSequence>> sequences =
            analyser.analyse(filename, sequence_length, skip_nonuser_mode, true);
        auto t2 = std::chrono::high_resolution_clock::now();

        std::cout << std::format("Trace analysis took {} seconds.\n",
                                 (duration_cast<std::chrono::milliseconds>(t2 - t1)).count()
                                     / 1000.0);

        for (const auto & sequence : sequences)
        {
            id_to_seq_.emplace(sequence->getId(), sequence);
        }
        current_sequence_id = sequences[0]->getId();
    }

    bool ProbabilisticInstGenerator::isDone() const
    {
        return visited_sequences_.size() == id_to_seq_.size();
    }

    void ProbabilisticInstGenerator::reset(const InstPtr & inst_ptr, const bool skip = false)
    {
        // TODO: Need to think about what reseting should mean here. Perhaps we give each new
        // instruction a sequence ID, which will be the preceding sequence, so that we start from
        // there?
        std::cout << "Not yet implemented reset for Probabilistic Generator\n";
    }

    InstPtr ProbabilisticInstGenerator::getNextInst(const sparta::Clock* clk)
    {

        if (SPARTA_EXPECT_FALSE(isDone()))
        {
            return nullptr;
        }

        const auto & current_sequence = id_to_seq_[current_sequence_id];
        const auto & first_instruction = current_sequence->getInstructions()[0];
        const auto next_inst(
            InstPtr(new Inst(*(first_instruction)))); // Create copy the instruction
        next_inst->setUniqueID(++unique_id_);
        next_inst->setProgramID(program_id_++);

        visited_sequences_.insert(current_sequence_id);

        current_sequence_id = current_sequence->getNextSequence(gen_);

        return next_inst;
    }
} // namespace olympia
