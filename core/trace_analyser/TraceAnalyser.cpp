#include "TraceAnalyser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>

#include <Inst.hpp>

// TODO: This class needs a test
namespace olympia
{
    std::vector<std::shared_ptr<InstSequence>>
    TraceAnalyser::analyse(const std::string & trace_path, const uint32_t sequence_length,
                           const bool skip_nonuser_mode, const bool write_json)

    {
        // TODO: We can cache the generated sequences according to the input parameters and use an
        // existing file if there's a match
        inst_generator_ = InstGenerator::createGenerator(info_logger_, mavis_facade_, trace_path,
                                                         skip_nonuser_mode);

        std::vector<std::shared_ptr<InstSequence>> sequences;
        std::unordered_map<std::size_t, std::shared_ptr<InstSequence>> id_to_seq;

        std::vector<InstPtr> instructions;
        std::unordered_map<size_t, InstPtr> instr_hash_to_instr;

        std::vector<InstPtr> current_instruction_sequence;
        current_instruction_sequence.reserve(sequence_length);

        auto insert_unique_instruction =
            [&instr_hash_to_instr, &instructions](const InstPtr & inst_ptr)
        {
            auto [it, inserted] = instr_hash_to_instr.try_emplace(inst_ptr->hash(), inst_ptr);
            if (inserted)
            {
                instructions.push_back(inst_ptr);
            }
        };

        // Initialize the first instruction sequence
        for (size_t i = 0; i < sequence_length; i++)
        {
            const auto & inst_ptr = inst_generator_->getNextInst(clk_);
            current_instruction_sequence.push_back(inst_ptr);
            insert_unique_instruction(inst_ptr);
        }

        int total_instruction_count = sequence_length;

        std::shared_ptr<InstSequence> first_sequence =
            std::make_shared<InstSequence>(current_instruction_sequence);

        std::shared_ptr<InstSequence> prev_sequence = first_sequence;

        sequences.push_back(prev_sequence);
        id_to_seq[prev_sequence->getId()] = prev_sequence;

        while (!inst_generator_->isDone())
        {
            std::shift_left(std::begin(current_instruction_sequence),
                            std::end(current_instruction_sequence), 1);
            const auto & inst_ptr = inst_generator_->getNextInst(clk_);
            current_instruction_sequence[sequence_length - 1] = inst_ptr;
            insert_unique_instruction(inst_ptr);

            auto new_sequence = std::make_shared<InstSequence>(current_instruction_sequence);
            auto [it, inserted] = id_to_seq.try_emplace(new_sequence->getId(), new_sequence);

            if (inserted)
            {
                sequences.push_back(new_sequence);
            }
            prev_sequence->addSuccessor(it->second->getId());
            prev_sequence = it->second;
            total_instruction_count++;
        }

        if (prev_sequence->getTransitions().empty())
        {
            prev_sequence->addSuccessor(first_sequence->getId());
        }

        if (write_json)
        {
            std::string result_file_name = std::format(
                "{}/{}-{}.json", results_dir,
                std::filesystem::path(trace_path).stem().generic_string(), sequence_length);
            writeJSONResults_(sequence_length, result_file_name, sequences, instructions);
        }

        std::cout << std::format("Extracted {} unique sequences of length {} and {} unique "
                                 "instructions from trace with {} instructions.\n",
                                 sequences.size(), sequence_length, instructions.size(),
                                 total_instruction_count);

        return sequences;
    }

    void
    TraceAnalyser::writeJSONResults_(const uint32_t sequence_length,
                                     const std::filesystem::path & result_file_path,
                                     const std::vector<std::shared_ptr<InstSequence>> & sequences,
                                     const std::vector<InstPtr> & instructions)
    {
        std::vector<boost::json::object> sequence_objs;
        for (const auto & sequence : sequences)
        {
            boost::json::object sequence_obj;
            sequence_obj["id"] = sequence->getId();

            std::vector<size_t> hashes;
            for (const auto & instr : sequence->getInstructions())
            {
                hashes.push_back(instr->hash());
            }
            sequence_obj["instructions"] = boost::json::value_from(hashes);

            std::vector<boost::json::object> successors;
            for (const auto & [successor_id, probability] : sequence->getTransitions())
            {
                boost::json::object successor_obj;
                successor_obj["id"] = successor_id;
                successor_obj["probability"] = probability;

                successors.push_back(successor_obj);
            }

            sequence_obj["successors"] = boost::json::value_from(successors);
            sequence_objs.push_back(sequence_obj);
        }

        std::vector<boost::json::object> instr_objs;
        for (const auto & instr : instructions)
        {
            boost::json::object instr_obj;
            instr_obj["hash"] = instr->hash();
            instr_obj["opcode"] = instr->getOpCode();
            instr_obj["disasm"] = instr->getDisasm();

            if (instr->isCoF())
            {
                instr_obj["cof"] = true;
            }

            if (instr->isBranch())
            {
                instr_obj["isBranch"] = true;
                instr_obj["taken"] = instr->isTakenBranch();
            }

            if (instr->isLoadStoreInst() || instr->isBranch())
            {
                instr_obj["targetVAddr"] = instr->getTargetVAddr();
            }

            instr_objs.push_back(instr_obj);
        }

        boost::json::value result_obj{{"sequenceLength", sequence_length},
                                      {"sequences", boost::json::value_from(sequence_objs)},
                                      {"instructions", boost::json::value_from(instr_objs)}};

        if (result_file_path.has_parent_path())
        {
            std::filesystem::create_directories(result_file_path.parent_path());
        }

        std::ofstream file(result_file_path);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + result_file_path.string());
        }

        file << boost::json::serialize(result_obj);

        std::cout << "Written statistical trace to " << result_file_path << "\n";
    }
} // namespace olympia